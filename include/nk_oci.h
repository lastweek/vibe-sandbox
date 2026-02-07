#ifndef NK_OCI_H
#define NK_OCI_H

#include <jansson.h>
#include <stdbool.h>
#include <stdint.h>

/* OCI runtime spec - process structure */
typedef struct nk_oci_process {
    char **args;                   /* Command line args */
    size_t args_len;
    char **env;                    /* Environment variables */
    size_t env_len;
    char *cwd;                     /* Current working directory */
    char *user;                    /* User to run as */
    uid_t uid;
    gid_t gid;
    gid_t *additional_gids;
    size_t additional_gids_len;
    bool no_new_privileges;
    char *console_size;
    bool terminal;
} nk_oci_process_t;

/* OCI runtime spec - root structure */
typedef struct nk_oci_root {
    char *path;                    /* Root filesystem path */
    bool readonly;                 /* Read-only flag */
} nk_oci_root_t;

/* OCI runtime spec - mount */
typedef struct nk_oci_mount {
    char *destination;             /* Mount destination */
    char *type;                    /* Filesystem type */
    char *source;                  /* Mount source */
    char **options;                /* Mount options */
    size_t options_len;
} nk_oci_mount_t;

/* OCI runtime spec - Linux namespace */
typedef struct nk_oci_namespace {
    char *type;                    /* Namespace type */
    char *path;                    /* Namespace path (for joining) */
} nk_oci_namespace_t;

/* OCI runtime spec - Linux resource limits */
typedef struct nk_oci_resources {
    /* Memory limits */
    struct {
        uint64_t limit;
        uint64_t reservation;
        uint64_t swap;
        bool kernel;
    } memory;

    /* CPU limits */
    struct {
        uint64_t shares;
        uint64_t quota;
        uint64_t period;
        int realtime_runtime;
        int realtime_period;
    } cpu;

    /* Process limits */
    uint64_t pids_limit;
} nk_oci_resources_t;

/* OCI runtime spec - Linux configuration */
typedef struct nk_oci_linux {
    nk_oci_namespace_t *namespaces;
    size_t namespaces_len;
    nk_oci_resources_t *resources;
    char *rootfs_propagation;
} nk_oci_linux_t;

/* OCI runtime spec - main configuration */
typedef struct nk_oci_spec {
    char *oci_version;             /* OCI version string */
    nk_oci_process_t *process;     /* Process configuration */
    nk_oci_root_t *root;           /* Root filesystem */
    char *hostname;                /* Container hostname */
    nk_oci_mount_t *mounts;        /* Mounts */
    size_t mounts_len;
    nk_oci_linux_t *linux_config;  /* Linux-specific config */
    char **annotations;            /* Annotations (key=value) */
    size_t annotations_len;
} nk_oci_spec_t;

/* OCI spec API */

/**
 * nk_oci_spec_load - Load OCI spec from config.json
 * @bundle_path: Path to container bundle directory
 *
 * Returns: Parsed spec, or NULL on error
 */
nk_oci_spec_t *nk_oci_spec_load(const char *bundle_path);

/**
 * nk_oci_spec_free - Free OCI spec structure
 * @spec: Spec to free
 */
void nk_oci_spec_free(nk_oci_spec_t *spec);

/**
 * nk_oci_spec_validate - Validate OCI spec
 * @spec: Spec to validate
 *
 * Returns: true if valid, false otherwise
 */
bool nk_oci_spec_validate(const nk_oci_spec_t *spec);

/**
 * nk_oci_spec_get_annotation - Get annotation value by key
 * @spec: OCI spec
 * @key: Annotation key
 *
 * Returns: Annotation value, or NULL if not found
 */
const char *nk_oci_spec_get_annotation(const nk_oci_spec_t *spec, const char *key);

#endif /* NK_OCI_H */
