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
#include <sys/syscall.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>

#include "nk_container.h"

/* Namespace mapping */
static const int ns_map[] = {
    [NK_NS_PID]     = CLONE_NEWPID,
    [NK_NS_NETWORK] = CLONE_NEWNET,
    [NK_NS_IPC]     = CLONE_NEWIPC,
    [NK_NS_UTS]     = CLONE_NEWUTS,
    [NK_NS_MOUNT]   = CLONE_NEWNS,
    [NK_NS_USER]    = CLONE_NEWUSER,
    [NK_NS_CGROUP]  = CLONE_NEWCGROUP,
};

static const char *ns_names[] = {
    [NK_NS_PID]     = "pid",
    [NK_NS_NETWORK] = "network",
    [NK_NS_IPC]     = "ipc",
    [NK_NS_UTS]     = "uts",
    [NK_NS_MOUNT]   = "mount",
    [NK_NS_USER]    = "user",
    [NK_NS_CGROUP]  = "cgroup",
};

/**
 * nk_namespace_get_clone_flags - Convert namespace config to clone flags
 */
int nk_namespace_get_clone_flags(const nk_namespace_config_t *namespaces, size_t len) {
    int flags = 0;
    for (size_t i = 0; i < len; i++) {
        if (namespaces[i].enable && !namespaces[i].path) {
            flags |= ns_map[namespaces[i].type];
        }
    }
    return flags;
}

/**
 * nk_namespace_set_hostname - Set container hostname
 */
int nk_namespace_set_hostname(const char *hostname) {
    if (sethostname(hostname, strlen(hostname)) == -1) {
        fprintf(stderr, "Error: Failed to set hostname '%s': %s\n",
                hostname, strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * nk_namespace_join - Join an existing namespace
 */
int nk_namespace_join(nk_namespace_type_t type, const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Failed to open namespace %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    if (setns(fd, ns_map[type]) == -1) {
        fprintf(stderr, "Error: Failed to join namespace %s: %s\n",
                ns_names[type], strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/**
 * nk_namespace_setup_user - Setup user namespace
 */
__attribute__((unused))
static int nk_namespace_setup_user(uid_t uid, gid_t gid) {
    /* Write to uid_map */
    int uid_map = open("/proc/self/uid_map", O_WRONLY);
    if (uid_map == -1) {
        fprintf(stderr, "Error: Failed to open uid_map: %s\n", strerror(errno));
        return -1;
    }

    /* Map container root to real root */
    if (dprintf(uid_map, "0 %d 1\n", uid) == -1) {
        fprintf(stderr, "Error: Failed to write uid_map: %s\n", strerror(errno));
        close(uid_map);
        return -1;
    }
    close(uid_map);

    /* Write to gid_map */
    int gid_map = open("/proc/self/gid_map", O_WRONLY);
    if (gid_map == -1) {
        fprintf(stderr, "Error: Failed to open gid_map: %s\n", strerror(errno));
        return -1;
    }

    if (dprintf(gid_map, "0 %d 1\n", gid) == -1) {
        fprintf(stderr, "Error: Failed to write gid_map: %s\n", strerror(errno));
        close(gid_map);
        return -1;
    }
    close(gid_map);

    /* Disable setgroups for unprivileged user namespaces */
    int setgroups = open("/proc/self/setgroups", O_WRONLY);
    if (setgroups != -1) {
        write(setgroups, "deny", 4);
        close(setgroups);
    }

    return 0;
}

/**
 * nk_container_setup_namespaces - Create and configure container namespaces
 */
int nk_container_setup_namespaces(const nk_container_ctx_t *ctx) {
    if (!ctx || !ctx->namespaces) {
        fprintf(stderr, "Error: No namespace configuration provided\n");
        return -1;
    }

    /* Check if we need to join any existing namespaces */
    for (size_t i = 0; i < ctx->namespaces_len; i++) {
        if (ctx->namespaces[i].path) {
            if (nk_namespace_join(ctx->namespaces[i].type, ctx->namespaces[i].path) == -1) {
                return -1;
            }
        }
    }

    return 0;
}

/**
 * nk_namespace_clone_flags_to_string - Convert flags to string for debugging
 */
const char *nk_namespace_flags_to_string(int flags) {
    static char buf[256];
    buf[0] = '\0';

    if (flags & CLONE_NEWPID)    strcat(buf, "PID ");
    if (flags & CLONE_NEWNET)   strcat(buf, "NET ");
    if (flags & CLONE_NEWIPC)    strcat(buf, "IPC ");
    if (flags & CLONE_NEWUTS)    strcat(buf, "UTS ");
    if (flags & CLONE_NEWNS)     strcat(buf, "MNT ");
    if (flags & CLONE_NEWUSER)   strcat(buf, "USER ");
    if (flags & CLONE_NEWCGROUP) strcat(buf, "CGROUP ");

    return buf;
}
