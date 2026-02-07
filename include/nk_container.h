#ifndef NK_CONTAINER_H
#define NK_CONTAINER_H

#include "nk_oci.h"
#include <stdbool.h>

/* Container namespaces */
typedef enum {
    NK_NS_PID,       /* Process ID namespace */
    NK_NS_NETWORK,   /* Network namespace */
    NK_NS_IPC,       /* IPC namespace */
    NK_NS_UTS,       /* UTS namespace */
    NK_NS_MOUNT,     /* Mount namespace */
    NK_NS_USER,      /* User namespace */
    NK_NS_CGROUP     /* Cgroup namespace */
} nk_namespace_type_t;

/* Namespace configuration */
typedef struct nk_namespace_config {
    nk_namespace_type_t type;
    char *path;      /* Path to existing namespace (for joining) */
    bool enable;     /* Whether to create this namespace */
} nk_namespace_config_t;

/* cgroup configuration */
typedef struct nk_cgroup_config {
    char *path;              /* Cgroup path */
    uint64_t memory_limit;   /* Memory limit in bytes */
    uint64_t cpu_shares;     /* CPU shares */
    uint64_t pids_limit;     /* Max processes */
} nk_cgroup_config_t;

/* Container execution context */
typedef struct nk_container_ctx {
    char *rootfs;                    /* Root filesystem path */
    char **mounts;                   /* Mount entries */
    size_t mounts_len;
    nk_namespace_config_t *namespaces;
    size_t namespaces_len;
    nk_cgroup_config_t *cgroup;
    char **env;                      /* Environment variables */
    size_t env_len;
    char *cwd;                       /* Working directory */
    char **args;                     /* Process arguments */
    size_t args_len;
    bool terminal;                   /* Attach terminal */
} nk_container_ctx_t;

/* Container API */

/**
 * nk_namespace_get_clone_flags - Get clone flags from namespace config
 * @namespaces: Array of namespace configurations
 * @len: Length of array
 *
 * Returns: Clone flags bitmask
 */
int nk_namespace_get_clone_flags(const nk_namespace_config_t *namespaces, size_t len);

/**
 * nk_namespace_set_hostname - Set container hostname
 * @hostname: Hostname to set
 *
 * Returns: 0 on success, -1 on error
 */
int nk_namespace_set_hostname(const char *hostname);

/**
 * nk_namespace_join - Join an existing namespace
 * @type: Namespace type
 * @path: Path to namespace file
 *
 * Returns: 0 on success, -1 on error
 */
int nk_namespace_join(nk_namespace_type_t type, const char *path);

/**
 * nk_namespace_flags_to_string - Convert clone flags to string (debug)
 * @flags: Clone flags
 *
 * Returns: String representation
 */
const char *nk_namespace_flags_to_string(int flags);

/**
 * nk_container_setup_namespaces - Create container namespaces
 * @ctx: Container context
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_setup_namespaces(const nk_container_ctx_t *ctx);

/**
 * nk_container_setup_cgroups - Setup container cgroups
 * @ctx: Container context
 * @container_id: Container ID for cgroup naming
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_setup_cgroups(const nk_container_ctx_t *ctx, const char *container_id);

/**
 * nk_container_setup_rootfs - Setup root filesystem
 * @ctx: Container context
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_setup_rootfs(const nk_container_ctx_t *ctx);

/**
 * nk_container_mount_custom - Mount custom mounts from OCI spec
 * @mounts: Array of mounts
 * @len: Number of mounts
 * @rootfs: Root filesystem path
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_mount_custom(const nk_oci_mount_t *mounts, size_t len,
                               const char *rootfs);

/**
 * nk_container_exec - Execute container process
 * @ctx: Container context
 *
 * Returns: PID of container init, or -1 on error
 */
pid_t nk_container_exec(const nk_container_ctx_t *ctx);

/**
 * nk_container_add_to_cgroup - Add process to container cgroup
 * @container_id: Container ID
 * @pid: Process ID
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_add_to_cgroup(const char *container_id, pid_t pid);

/**
 * nk_container_wait - Wait for container process to exit
 * @pid: Process ID
 * @status: Pointer to store exit status
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_wait(pid_t pid, int *status);

/**
 * nk_container_signal - Send signal to container process
 * @pid: Process ID
 * @sig: Signal number
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_signal(pid_t pid, int sig);

/**
 * nk_cgroup_cleanup - Cleanup cgroup resources
 * @container_id: Container ID
 *
 * Returns: 0 on success, -1 on error
 */
int nk_cgroup_cleanup(const char *container_id);

/**
 * nk_container_cleanup - Cleanup container resources
 * @ctx: Container context
 * @container_id: Container ID
 */
void nk_container_cleanup(const nk_container_ctx_t *ctx, const char *container_id);

#endif /* NK_CONTAINER_H */
