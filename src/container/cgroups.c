#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#include "nk_container.h"

#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_V2_CHECK "/sys/fs/cgroup/cgroup.controllers"

/**
 * nk_cgroup_is_v2 - Check if cgroups v2 is available
 */
static int nk_cgroup_is_v2(void) {
    struct stat st;
    return (stat(CGROUP_V2_CHECK, &st) == 0);
}

/**
 * nk_cgroup_create - Create a cgroup for the container
 */
static int nk_cgroup_create(const char *container_id) {
    char cgroup_path[PATH_MAX];
    snprintf(cgroup_path, sizeof(cgroup_path), "%s/nano-kata/%s",
             CGROUP_ROOT, container_id);

    /* Create parent cgroup directory */
    char parent_dir[PATH_MAX];
    snprintf(parent_dir, sizeof(parent_dir), "%s/nano-kata", CGROUP_ROOT);

    if (mkdir(parent_dir, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create %s: %s\n",
                parent_dir, strerror(errno));
        return -1;
    }

    /* Create container cgroup directory */
    if (mkdir(cgroup_path, 0755) == -1 && errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create %s: %s\n",
                cgroup_path, strerror(errno));
        return -1;
    }

    /* Enable all controllers */
    char controllers_path[PATH_MAX];
    snprintf(controllers_path, sizeof(controllers_path),
             "%s/cgroup.subtree_control", parent_dir);

    int fd = open(controllers_path, O_WRONLY);
    if (fd != -1) {
        /* Enable common controllers */
        const char *controllers = "+cpu +memory +pids +io +cpuset";
        write(fd, controllers, strlen(controllers));
        close(fd);
    }

    printf("  Created cgroup: %s\n", cgroup_path);
    return 0;
}

/**
 * nk_cgroup_set_memory_limit - Set memory limit for container
 */
static int nk_cgroup_set_memory_limit(const char *container_id, uint64_t limit) {
    if (limit == 0) {
        return 0;  /* No limit */
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/nano-kata/%s/memory.max",
             CGROUP_ROOT, container_id);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Failed to open %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    char limit_str[64];
    snprintf(limit_str, sizeof(limit_str), "%lu", limit);

    if (write(fd, limit_str, strlen(limit_str)) == -1) {
        fprintf(stderr, "Warning: Failed to set memory limit: %s\n",
                strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    printf("  Set memory limit: %lu bytes\n", limit);
    return 0;
}

/**
 * nk_cgroup_set_cpu_shares - Set CPU shares for container
 */
static int nk_cgroup_set_cpu_shares(const char *container_id, uint64_t shares) {
    if (shares == 0) {
        return 0;  /* No limit */
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/nano-kata/%s/cpu.weight",
             CGROUP_ROOT, container_id);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Failed to open %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    char shares_str[64];
    snprintf(shares_str, sizeof(shares_str), "%lu", shares);

    if (write(fd, shares_str, strlen(shares_str)) == -1) {
        fprintf(stderr, "Warning: Failed to set CPU shares: %s\n",
                strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    printf("  Set CPU weight: %lu\n", shares);
    return 0;
}

/**
 * nk_cgroup_set_pids_limit - Set max processes for container
 */
static int nk_cgroup_set_pids_limit(const char *container_id, uint64_t limit) {
    if (limit == 0) {
        return 0;  /* No limit */
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/nano-kata/%s/pids.max",
             CGROUP_ROOT, container_id);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Failed to open %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    char limit_str[64];
    snprintf(limit_str, sizeof(limit_str), "%lu", limit);

    if (write(fd, limit_str, strlen(limit_str)) == -1) {
        fprintf(stderr, "Warning: Failed to set PIDs limit: %s\n",
                strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    printf("  Set PIDs limit: %lu\n", limit);
    return 0;
}

/**
 * nk_cgroup_add_process - Add process to cgroup
 */
static int nk_cgroup_add_process(const char *container_id, pid_t pid) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/nano-kata/%s/cgroup.procs",
             CGROUP_ROOT, container_id);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Failed to open %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    char pid_str[64];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    if (write(fd, pid_str, strlen(pid_str)) == -1) {
        fprintf(stderr, "Error: Failed to add process to cgroup: %s\n",
                strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/**
 * nk_cgroup_delete - Delete container cgroup
 */
static int nk_cgroup_delete(const char *container_id) {
    char cgroup_path[PATH_MAX];
    int len = snprintf(cgroup_path, sizeof(cgroup_path), "%s/nano-kata",
                       CGROUP_ROOT);
    if (len > 0 && len < (int)sizeof(cgroup_path)) {
        snprintf(cgroup_path + len, sizeof(cgroup_path) - len, "/%s", container_id);
    }

    /* Remove all processes from cgroup first */
    char procs_path[PATH_MAX];
    len = snprintf(procs_path, sizeof(procs_path), "%s/cgroup.procs", cgroup_path);
    (void)len;  /* Mark as intentionally unused */

    int fd = open(procs_path, O_WRONLY);
    if (fd != -1) {
        /* Move processes to parent cgroup */
        write(fd, "0", 1);
        close(fd);
    }

    /* Remove cgroup directory */
    if (rmdir(cgroup_path) == -1 && errno != ENOENT) {
        fprintf(stderr, "Warning: Failed to remove cgroup %s: %s\n",
                cgroup_path, strerror(errno));
    }

    return 0;
}

/**
 * nk_container_setup_cgroups - Setup cgroups for container
 */
int nk_container_setup_cgroups(const nk_container_ctx_t *ctx,
                                const char *container_id) {
    if (!ctx || !ctx->cgroup || !container_id) {
        return 0;  /* Cgroups are optional */
    }

    /* Check if cgroups v2 is available */
    if (!nk_cgroup_is_v2()) {
        fprintf(stderr, "Warning: cgroups v2 not available, skipping cgroup setup\n");
        return 0;
    }

    printf("  Setting up cgroups...\n");

    /* Create cgroup */
    if (nk_cgroup_create(container_id) == -1) {
        return -1;
    }

    /* Set resource limits */
    if (ctx->cgroup->memory_limit > 0) {
        nk_cgroup_set_memory_limit(container_id, ctx->cgroup->memory_limit);
    }

    if (ctx->cgroup->cpu_shares > 0) {
        nk_cgroup_set_cpu_shares(container_id, ctx->cgroup->cpu_shares);
    }

    if (ctx->cgroup->pids_limit > 0) {
        nk_cgroup_set_pids_limit(container_id, ctx->cgroup->pids_limit);
    }

    return 0;
}

/**
 * nk_container_add_to_cgroup - Add process to container cgroup
 */
int nk_container_add_to_cgroup(const char *container_id, pid_t pid) {
    if (!container_id) {
        return 0;
    }

    return nk_cgroup_add_process(container_id, pid);
}

/**
 * nk_cgroup_cleanup - Cleanup cgroup resources
 */
int nk_cgroup_cleanup(const char *container_id) {
    if (!container_id) {
        return 0;
    }

    return nk_cgroup_delete(container_id);
}
