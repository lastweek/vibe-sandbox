#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <pwd.h>
#include <grp.h>

#include "nk_container.h"
#include "nk_log.h"

/* Make cap-ng optional */
#ifdef HAVE_LIBCAPNG
#include <cap-ng.h>
#else
#define CAPNG_NONE 0
#endif

/* Stack size for child process */
#define STACK_SIZE (1024 * 1024)

/* Container execution context passed to child */
typedef struct {
    const nk_container_ctx_t *ctx;
    const char *container_id;
    const char *hostname;
    int sync_pipe[2];  /* Pipe for parent-child synchronization */
    char **env;        /* Environment variables */
} container_exec_ctx_t;

#define CHILD_SYNC_READY '1'
#define CHILD_SYNC_ERROR '0'

/**
 * nk_process_drop_capabilities - Drop capabilities
 */
static int nk_process_drop_capabilities(void) {
#ifdef HAVE_LIBCAPNG
    if (capng_have_capabilities() == CAPNG_NONE) {
        return 0;  /* libcap-ng not available or no capabilities */
    }

    /* Clear all capabilities from the bounding set */
    if (capng_clear(CAPNG_SELECT_BOTH) == -1) {
        nk_stderr( "Warning: Failed to clear capabilities\n");
        return -1;
    }

    if (capng_apply(CAPNG_SELECT_BOTH) == -1) {
        nk_stderr( "Warning: Failed to apply capabilities\n");
        return -1;
    }
#else
    /* libcap-ng not available, skip capability dropping */
    nk_stderr( "Warning: libcap-ng not available, skipping capability dropping\n");
#endif
    return 0;
}

/**
 * nk_process_set_rlimits - Set resource limits
 */
static int nk_process_set_rlimits(void) {
    /* Set stack size limit */
    struct rlimit rl = {
        .rlim_cur = 8192 * 1024,  /* 8 MB */
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_STACK, &rl) == -1) {
        nk_stderr( "Warning: Failed to set stack limit: %s\n",
                strerror(errno));
    }

    return 0;
}

/**
 * nk_process_set_uid_gid - Set user and group IDs
 */
__attribute__((unused))
static int nk_process_set_uid_gid(uid_t uid, gid_t gid) {
    if (setgid(gid) == -1) {
        nk_stderr( "Error: Failed to set GID %d: %s\n",
                (int)gid, strerror(errno));
        return -1;
    }

    if (setuid(uid) == -1) {
        nk_stderr( "Error: Failed to set UID %d: %s\n",
                (int)uid, strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * container_child_fn - Child process execution function
 */
static int container_child_fn(void *arg) {
    container_exec_ctx_t *exec_ctx = (container_exec_ctx_t *)arg;
    const nk_container_ctx_t *ctx = exec_ctx->ctx;
    nk_log_set_role(NK_LOG_ROLE_CHILD);

    nk_log_debug("Child process started (in isolated namespaces)");

    /* Close parent end of sync pipe */
    close(exec_ctx->sync_pipe[0]);

    /* Setup hostname */
    if (exec_ctx->hostname && ctx->namespaces) {
        for (size_t i = 0; i < ctx->namespaces_len; i++) {
            if (ctx->namespaces[i].type == NK_NS_UTS) {
                nk_log_debug("Setting hostname in UTS namespace");
                nk_namespace_set_hostname(exec_ctx->hostname);
                break;
            }
        }
    }

    /* Setup root filesystem */
    nk_log_debug("Setting up root filesystem");
    if (nk_container_setup_rootfs(ctx) == -1) {
        const char status = CHILD_SYNC_ERROR;
        (void)write(exec_ctx->sync_pipe[1], &status, 1);
        close(exec_ctx->sync_pipe[1]);
        return 1;
    }
    nk_log_debug("Root filesystem ready");

    /* Change to working directory */
    if (ctx->cwd) {
        if (chdir(ctx->cwd) == -1) {
            nk_log_warn("Failed to chdir to %s: %s", ctx->cwd, strerror(errno));
            /* Try root instead */
            chdir("/");
        }
    }

    /* Set user and group */
    /* For now, run as root - Phase 2 enhancement */

    /* Drop capabilities */
    nk_log_debug("Dropping capabilities");
    nk_process_drop_capabilities();

    /* Set resource limits */
    nk_process_set_rlimits();

    /*
     * Detached/non-terminal workloads should not share the caller's
     * controlling terminal, otherwise a parent/session exit can deliver SIGHUP.
     */
    if (!ctx->terminal) {
        if (setsid() == -1) {
            nk_log_warn("Failed to detach child session: %s", strerror(errno));
        } else {
            nk_log_debug("Detached child into a new session");
        }
    }

    /* Notify parent we're ready */
    nk_log_debug("Notifying parent: ready to exec");
    const char ready = CHILD_SYNC_READY;
    (void)write(exec_ctx->sync_pipe[1], &ready, 1);
    close(exec_ctx->sync_pipe[1]);

    /* Execute the container process */
    nk_log_debug("Executing: %s", ctx->args[0]);
    if (ctx->args && ctx->args_len > 0) {
        execve(ctx->args[0], ctx->args, exec_ctx->env);
    }

    /* If we get here, exec failed */
    nk_log_error("Failed to execute %s: %s",
            ctx->args ? ctx->args[0] : "none", strerror(errno));
    if (errno == ENOEXEC) {
        nk_stderr("Hint: executable format is incompatible with host CPU architecture.\n");
        nk_stderr("Hint: rebuild rootfs for this host, then reinstall bundle:\n");
        nk_stderr("      ./scripts/setup-rootfs.sh --force && make install\n");
    }
    return 1;
}

/**
 * nk_container_exec - Execute container process
 */
pid_t nk_container_exec(const nk_container_ctx_t *ctx) {
    if (!ctx || !ctx->rootfs || !ctx->args || ctx->args_len == 0) {
        nk_log_error("Invalid container context");
        return -1;
    }

    /* Get clone flags for namespaces */
    int clone_flags = SIGCHLD;
    if (ctx->namespaces) {
        clone_flags |= nk_namespace_get_clone_flags(ctx->namespaces, ctx->namespaces_len);
    }

    nk_log_debug("Clone flags: %s (0x%x)", nk_namespace_flags_to_string(clone_flags), clone_flags);
    nk_log_info("Clone flags: %s", nk_namespace_flags_to_string(clone_flags));

    if (nk_log_educational) {
        nk_log_explain_op("Allocating stack for child process",
            "clone() requires separate stack. Unlike fork(), clone can create threads with shared memory.");
    }

    /* Create stack for child process */
    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        nk_log_error("Failed to allocate stack");
        return -1;
    }
    char *stack_top = stack + STACK_SIZE;
    nk_log_debug("Allocated %d byte stack at %p", STACK_SIZE, stack);

    /* Create sync pipe for parent-child coordination */
    if (nk_log_educational) {
        nk_log_explain_op("Creating sync pipe",
            "Parent waits on pipe while child sets up namespaces, rootfs, etc. "
            "Ensures parent knows when child is ready before continuing.");
    }

    int sync_pipe[2];
    if (pipe(sync_pipe) == -1) {
        nk_log_error("Failed to create sync pipe: %s", strerror(errno));
        free(stack);
        return -1;
    }
    nk_log_debug("Sync pipe created: fd[%d, %d]", sync_pipe[0], sync_pipe[1]);

    /* Setup execution context */
    container_exec_ctx_t exec_ctx = {
        .ctx = ctx,
        .sync_pipe[0] = sync_pipe[0],
        .sync_pipe[1] = sync_pipe[1],
        .env = NULL,
    };

    /* Build environment for child */
    if (ctx->env && ctx->env_len > 0) {
        exec_ctx.env = ctx->env;
    } else {
        /* Default environment */
        static char *default_env[] = {
            "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
            "TERM=xterm",
            "HOME=/root",
            NULL,
        };
        exec_ctx.env = default_env;
    }

    /* Clone child process */
    nk_log_set_role(NK_LOG_ROLE_PARENT);
    pid_t pid = clone(container_child_fn, stack_top, clone_flags, &exec_ctx);
    if (pid == -1) {
        nk_stderr( "Error: Failed to clone container process: %s\n",
                strerror(errno));
        close(sync_pipe[0]);
        close(sync_pipe[1]);
        free(stack);
        return -1;
    }

    /* Close child end of sync pipe */
    close(sync_pipe[1]);

    /* Wait for child to signal ready */
    char buf;
    ssize_t ret = read(sync_pipe[0], &buf, 1);
    if (ret <= 0 || buf != CHILD_SYNC_READY) {
        nk_stderr( "Error: Child process failed to initialize\n");
        close(sync_pipe[0]);
        (void)waitpid(pid, NULL, 0);
        free(stack);
        return -1;
    }
    close(sync_pipe[0]);

    /* Add child to cgroup */
    if (exec_ctx.container_id) {
        nk_container_add_to_cgroup(exec_ctx.container_id, pid);
    }

    free(stack);
    return pid;
}

/**
 * nk_container_wait - Wait for container process to exit
 */
int nk_container_wait(pid_t pid, int *status) {
    if (waitpid(pid, status, 0) == -1) {
        nk_stderr( "Error: Failed to wait for container: %s\n",
                strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * nk_container_signal - Send signal to container process
 */
int nk_container_signal(pid_t pid, int sig) {
    if (kill(pid, sig) == -1) {
        nk_stderr( "Error: Failed to send signal %d to PID %d: %s\n",
                sig, (int)pid, strerror(errno));
        return -1;
    }
    return 0;
}
