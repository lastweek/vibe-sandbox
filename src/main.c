#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>

#include "nk.h"
#include "nk_oci.h"
#include "nk_container.h"
#include "common/state.h"

#define NK_STATE_DIR "run"

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <command> [options]\n\n", prog_name);
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  create [options] <container-id>  Create a new container\n");
    fprintf(stderr, "  start <container-id>              Start a created container\n");
    fprintf(stderr, "  delete <container-id>             Delete a container\n");
    fprintf(stderr, "  state <container-id>              Query container state\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -b, --bundle=<path>    Path to container bundle (default: .)\n");
    fprintf(stderr, "  -r, --runtime=<mode>   Execution mode: container|vm (default: container)\n");
    fprintf(stderr, "  -p, --pid-file=<file>  File to write container PID\n");
    fprintf(stderr, "  -h, --help            Show this help message\n");
    fprintf(stderr, "  -v, --version         Show version information\n");
}

static void print_version(void) {
    printf("nano-kata version %d.%d.%d\n",
           NK_VERSION_MAJOR, NK_VERSION_MINOR, NK_VERSION_PATCH);
    printf("Educational OCI Container Runtime with VM Isolation\n");
}

int nk_parse_args(int argc, char *argv[], nk_options_t *opts) {
    if (argc < 2) {
        return -1;
    }

    /* Initialize options with defaults */
    memset(opts, 0, sizeof(*opts));
    opts->bundle_path = strdup(".");
    opts->mode = NK_MODE_CONTAINER;

    /* Check for help/version flags first */
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        opts->command = "help";
        return 0;
    }
    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        opts->command = "version";
        return 0;
    }

    /* First argument is the command */
    opts->command = argv[1];

    /* Parse remaining options */
    static struct option long_options[] = {
        {"bundle",    required_argument, 0, 'b'},
        {"runtime",   required_argument, 0, 'r'},
        {"pid-file",  required_argument, 0, 'p'},
        {"help",      no_argument,       0, 'h'},
        {"version",   no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    int opt_index = 0;
    optind = 2; /* Start parsing from argv[2] */

    while ((opt = getopt_long(argc, argv, "b:r:p:hv", long_options, &opt_index)) != -1) {
        switch (opt) {
        case 'b':
            free(opts->bundle_path);
            opts->bundle_path = strdup(optarg);
            break;
        case 'r':
            if (strcmp(optarg, "container") == 0) {
                opts->mode = NK_MODE_CONTAINER;
            } else if (strcmp(optarg, "vm") == 0) {
                opts->mode = NK_MODE_VM;
            } else {
                fprintf(stderr, "Error: Invalid runtime mode '%s'\n", optarg);
                return -1;
            }
            break;
        case 'p':
            opts->pid_file = strdup(optarg);
            break;
        case 'h':
            print_usage(argv[0]);
            exit(0);
        case 'v':
            print_version();
            exit(0);
        default:
            return -1;
        }
    }

    /* Container ID is the last non-option argument */
    if (optind < argc) {
        opts->container_id = argv[optind];
    }

    /* Validate command */
    if (strcmp(opts->command, "create") == 0) {
        if (!opts->container_id) {
            fprintf(stderr, "Error: create command requires container-id\n");
            return -1;
        }
    } else if (strcmp(opts->command, "start") == 0 ||
               strcmp(opts->command, "delete") == 0 ||
               strcmp(opts->command, "state") == 0) {
        if (!opts->container_id) {
            fprintf(stderr, "Error: %s command requires container-id\n", opts->command);
            return -1;
        }
    } else {
        fprintf(stderr, "Error: Unknown command '%s'\n", opts->command);
        return -1;
    }

    return 0;
}

static int ensure_state_dir(void) {
    struct stat st;

    if (stat(NK_STATE_DIR, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        fprintf(stderr, "Error: %s exists but is not a directory\n", NK_STATE_DIR);
        return -1;
    }

    if (mkdir(NK_STATE_DIR, 0755) == -1) {
        fprintf(stderr, "Error: Failed to create %s: %s\n",
                NK_STATE_DIR, strerror(errno));
        return -1;
    }

    return 0;
}

int nk_container_create(const nk_options_t *opts) {
    fprintf(stdout, "Creating container '%s' (mode: %s)\n",
            opts->container_id,
            opts->mode == NK_MODE_CONTAINER ? "container" : "vm");

    /* Ensure state directory exists */
    if (ensure_state_dir() == -1) {
        return -1;
    }

    /* Check if container already exists */
    if (nk_state_exists(opts->container_id)) {
        fprintf(stderr, "Error: Container '%s' already exists\n", opts->container_id);
        return -1;
    }

    /* Load OCI spec from bundle */
    nk_oci_spec_t *spec = nk_oci_spec_load(opts->bundle_path);
    if (!spec) {
        fprintf(stderr, "Error: Failed to load OCI spec from %s\n", opts->bundle_path);
        return -1;
    }

    /* Validate OCI spec */
    if (!nk_oci_spec_validate(spec)) {
        fprintf(stderr, "Error: Invalid OCI spec\n");
        nk_oci_spec_free(spec);
        return -1;
    }

    fprintf(stdout, "  Bundle: %s\n", opts->bundle_path);
    fprintf(stdout, "  Root: %s\n", spec->root ? spec->root->path : "none");

    /* Create container structure */
    nk_container_t *container = calloc(1, sizeof(*container));
    if (!container) {
        nk_oci_spec_free(spec);
        return -1;
    }

    container->id = strdup(opts->container_id);
    container->bundle_path = strdup(opts->bundle_path);
    container->state = NK_STATE_CREATED;
    container->mode = opts->mode;
    container->init_pid = 0;
    container->control_fd = -1;

    /* Save container state */
    if (nk_state_save(container) == -1) {
        fprintf(stderr, "Error: Failed to save container state\n");
        nk_container_free(container);
        nk_oci_spec_free(spec);
        return -1;
    }

    nk_oci_spec_free(spec);
    nk_container_free(container);

    fprintf(stdout, "  Status: created\n");

    return 0;
}

int nk_container_start(const char *container_id) {
    fprintf(stdout, "Starting container '%s'\n", container_id);

    /* Load container state */
    nk_container_t *container = nk_state_load(container_id);
    if (!container) {
        fprintf(stderr, "Error: Container '%s' not found\n", container_id);
        return -1;
    }

    if (container->state != NK_STATE_CREATED) {
        fprintf(stderr, "Error: Container is not in 'created' state\n");
        nk_container_free(container);
        return -1;
    }

    /* Only support container mode for now */
    if (container->mode == NK_MODE_VM) {
        fprintf(stderr, "Error: VM mode not yet implemented (Phase 3)\n");
        nk_container_free(container);
        return -1;
    }

    /* Load OCI spec */
    nk_oci_spec_t *spec = nk_oci_spec_load(container->bundle_path);
    if (!spec) {
        fprintf(stderr, "Error: Failed to load OCI spec\n");
        nk_container_free(container);
        return -1;
    }

    if (!spec->process || !spec->root) {
        fprintf(stderr, "Error: Invalid OCI spec - missing process or root\n");
        nk_oci_spec_free(spec);
        nk_container_free(container);
        return -1;
    }

    /* Build container context from OCI spec */
    nk_container_ctx_t ctx = {0};

    /* Root filesystem path */
    char rootfs_path[PATH_MAX];
    snprintf(rootfs_path, sizeof(rootfs_path), "%s/%s",
             container->bundle_path, spec->root->path);
    ctx.rootfs = rootfs_path;

    /* Parse namespaces from OCI spec */
    if (spec->linux_config && spec->linux_config->namespaces) {
        size_t ns_count = spec->linux_config->namespaces_len;
        ctx.namespaces = calloc(ns_count, sizeof(nk_namespace_config_t));
        if (ctx.namespaces) {
            for (size_t i = 0; i < ns_count; i++) {
                /* Map namespace type string to enum */
                const char *type = spec->linux_config->namespaces[i].type;
                if (strcmp(type, "pid") == 0) ctx.namespaces[i].type = NK_NS_PID;
                else if (strcmp(type, "network") == 0) ctx.namespaces[i].type = NK_NS_NETWORK;
                else if (strcmp(type, "ipc") == 0) ctx.namespaces[i].type = NK_NS_IPC;
                else if (strcmp(type, "uts") == 0) ctx.namespaces[i].type = NK_NS_UTS;
                else if (strcmp(type, "mount") == 0) ctx.namespaces[i].type = NK_NS_MOUNT;
                else if (strcmp(type, "user") == 0) ctx.namespaces[i].type = NK_NS_USER;
                else if (strcmp(type, "cgroup") == 0) ctx.namespaces[i].type = NK_NS_CGROUP;

                ctx.namespaces[i].path = spec->linux_config->namespaces[i].path;
                ctx.namespaces[i].enable = true;
            }
            ctx.namespaces_len = ns_count;
        }
    }

    /* Process arguments */
    ctx.args = spec->process->args;
    ctx.args_len = spec->process->args_len;

    /* Environment variables */
    ctx.env = spec->process->env;
    ctx.env_len = spec->process->env_len;

    /* Working directory */
    ctx.cwd = spec->process->cwd ? spec->process->cwd : "/";

    /* Terminal */
    ctx.terminal = spec->process->terminal;

    /* Mounts */
    ctx.mounts = NULL;
    ctx.mounts_len = 0;

    /* Cgroups - create minimal config */
    nk_cgroup_config_t cg_cfg = {0};
    ctx.cgroup = &cg_cfg;

    printf("  Executing: %s\n", ctx.args[0]);

    /* Execute container process */
    pid_t pid = nk_container_exec(&ctx);
    if (pid == -1) {
        fprintf(stderr, "Error: Failed to execute container\n");
        free(ctx.namespaces);
        nk_oci_spec_free(spec);
        nk_container_free(container);
        return -1;
    }

    /* Update state to running */
    container->state = NK_STATE_RUNNING;
    container->init_pid = pid;

    if (nk_state_save(container) == -1) {
        fprintf(stderr, "Warning: Failed to save container state\n");
    }

    /* Cleanup */
    free(ctx.namespaces);
    nk_oci_spec_free(spec);
    nk_container_free(container);

    fprintf(stdout, "  Status: running (PID: %d)\n", (int)pid);

    return 0;
}

int nk_container_delete(const char *container_id) {
    fprintf(stdout, "Deleting container '%s'\n", container_id);

    /* Load container state */
    nk_container_t *container = nk_state_load(container_id);
    if (!container) {
        fprintf(stderr, "Error: Container '%s' not found\n", container_id);
        return -1;
    }

    /* Stop container if running */
    if (container->state == NK_STATE_RUNNING && container->init_pid > 0) {
        fprintf(stdout, "  Stopping container (PID: %d)\n", container->init_pid);

        /* Send SIGTERM first */
        if (nk_container_signal(container->init_pid, SIGTERM) == 0) {
            /* Wait a bit for graceful shutdown */
            usleep(100000);  /* 100ms */

            /* Check if process still exists */
            if (kill(container->init_pid, 0) == 0) {
                /* Force kill if still running */
                fprintf(stdout, "  Force killing...\n");
                nk_container_signal(container->init_pid, SIGKILL);
            }
        }
    }

    /* Cleanup cgroups */
    nk_cgroup_cleanup(container_id);

    /* Delete state file */
    if (nk_state_delete(container_id) == -1) {
        fprintf(stderr, "Warning: Failed to delete state file\n");
    }

    nk_container_free(container);

    fprintf(stdout, "  Status: deleted\n");

    return 0;
}

nk_container_state_t nk_container_state(const char *container_id) {
    /* Load container state */
    nk_container_t *container = nk_state_load(container_id);
    if (!container) {
        fprintf(stderr, "Error: Container '%s' not found\n", container_id);
        return -1;
    }

    nk_container_state_t state = container->state;
    nk_container_free(container);

    return state;
}

void nk_container_free(nk_container_t *container) {
    if (!container) {
        return;
    }

    free(container->id);
    free(container->bundle_path);
    free(container->state_file);
    if (container->control_fd != -1) {
        close(container->control_fd);
    }
    free(container);
}

int main(int argc, char *argv[]) {
    nk_options_t opts;

    if (nk_parse_args(argc, argv, &opts) == -1) {
        print_usage(argv[0]);
        return 1;
    }

    int ret = 0;

    if (strcmp(opts.command, "help") == 0) {
        print_usage(argv[0]);
    } else if (strcmp(opts.command, "version") == 0) {
        print_version();
    } else if (strcmp(opts.command, "create") == 0) {
        ret = nk_container_create(&opts);
    } else if (strcmp(opts.command, "start") == 0) {
        ret = nk_container_start(opts.container_id);
    } else if (strcmp(opts.command, "delete") == 0) {
        ret = nk_container_delete(opts.container_id);
    } else if (strcmp(opts.command, "state") == 0) {
        nk_container_state_t state = nk_container_state(opts.container_id);
        const char *state_str;

        switch (state) {
        case NK_STATE_CREATED:
            state_str = "created";
            break;
        case NK_STATE_RUNNING:
            state_str = "running";
            break;
        case NK_STATE_STOPPED:
            state_str = "stopped";
            break;
        case NK_STATE_PAUSED:
            state_str = "paused";
            break;
        default:
            state_str = "unknown";
            break;
        }

        printf("%s\n", state_str);
    }

    /* Cleanup options */
    free(opts.bundle_path);
    free(opts.pid_file);

    return ret;
}
