#ifndef NK_VM_H
#define NK_VM_H

#include <stdint.h>
#include <stdbool.h>

/* VM configuration */
typedef struct nk_vm_config {
    char *vm_id;                    /* VM identifier */
    uint32_t vcpus;                 /* Number of vCPUs */
    uint64_t memory_mb;             /* Memory in MB */
    char *kernel_path;              /* Path to kernel image */
    char *rootfs_path;              /* Path to rootfs/initramfs */
    bool enable_network;            /* Enable network interface */
    char *tap_device;               /* TAP device name */
} nk_vm_config_t;

/* VM state */
typedef enum {
    NK_VM_NOT_CREATED,
    NK_VM_CREATED,
    NK_VM_RUNNING,
    NK_VM_STOPPED,
    NK_VM_PAUSED
} nk_vm_state_t;

/* VM context */
typedef struct nk_vm {
    char *vm_id;                    /* VM identifier */
    nk_vm_config_t config;          /* VM configuration */
    nk_vm_state_t state;            /* Current state */
    pid_t firecracker_pid;          /* Firecracker VMM PID */
    char *api_socket;               /* Firecracker API socket path */
    int control_fd;                 /* Control channel */
} nk_vm_t;

/* VM API */

/**
 * nk_vm_create - Create a new VM
 * @config: VM configuration
 *
 * Returns: VM context, or NULL on error
 */
nk_vm_t *nk_vm_create(const nk_vm_config_t *config);

/**
 * nk_vm_start - Start a created VM
 * @vm: VM context
 *
 * Returns: 0 on success, -1 on error
 */
int nk_vm_start(nk_vm_t *vm);

/**
 * nk_vm_stop - Stop a running VM
 * @vm: VM context
 *
 * Returns: 0 on success, -1 on error
 */
int nk_vm_stop(nk_vm_t *vm);

/**
 * nk_vm_delete - Delete a VM
 * @vm: VM context
 *
 * Returns: 0 on success, -1 on error
 */
int nk_vm_delete(nk_vm_t *vm);

/**
 * nk_vm_state - Query VM state
 * @vm: VM context
 *
 * Returns: Current VM state
 */
nk_vm_state_t nk_vm_state(const nk_vm_t *vm);

/**
 * nk_vm_free - Free VM resources
 * @vm: VM context to free
 */
void nk_vm_free(nk_vm_t *vm);

/**
 * nk_vm_execute_container - Execute a container inside the VM
 * @vm: VM context
 * @bundle_path: Path to container bundle
 *
 * Returns: 0 on success, -1 on error
 */
int nk_vm_execute_container(nk_vm_t *vm, const char *bundle_path);

#endif /* NK_VM_H */
