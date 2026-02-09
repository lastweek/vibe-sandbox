#ifndef NK_H
#define NK_H

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

/* nano-sandbox version */
#define NK_VERSION_MAJOR 0
#define NK_VERSION_MINOR 1
#define NK_VERSION_PATCH 0

/* Container states */
typedef enum {
    NK_STATE_INVALID = -1,
    NK_STATE_CREATED,
    NK_STATE_RUNNING,
    NK_STATE_STOPPED,
    NK_STATE_PAUSED
} nk_container_state_t;

/* Execution modes */
typedef enum {
    NK_MODE_CONTAINER,  /* Pure container (namespaces + cgroups) */
    NK_MODE_VM          /* VM-based (Firecracker) */
} nk_execution_mode_t;

/* Container context */
typedef struct nk_container {
    char *id;                       /* Container ID */
    char *bundle_path;              /* Path to container bundle */
    nk_container_state_t state;     /* Current state */
    nk_execution_mode_t mode;       /* Execution mode */
    pid_t init_pid;                 /* PID of container init process */
    char *state_file;               /* Path to state file */
    int control_fd;                 /* Control pipe for container */
} nk_container_t;

/* Command-line options */
typedef struct nk_options {
    char *command;                  /* create|start|run|exec|delete|state */
    char *container_id;             /* Container ID */
    char *bundle_path;              /* Bundle path */
    char *pid_file;                 /* PID file path */
    char *resume_exec;              /* Optional command for resume */
    nk_execution_mode_t mode;       /* Execution mode */
    bool attach;                    /* Attach to container process */
    bool detach;                    /* Run detached from terminal */
    bool rm;                        /* Remove container after run exits */
} nk_options_t;

/* Core API functions */

/**
 * nk_parse_args - Parse command-line arguments
 * @argc: Argument count
 * @argv: Argument vector
 * @opts: Options structure to fill
 *
 * Returns: 0 on success, -1 on error
 */
int nk_parse_args(int argc, char *argv[], nk_options_t *opts);

/**
 * nk_container_create - Create a new container
 * @opts: Container options
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_create(const nk_options_t *opts);

/**
 * nk_container_start - Start a created container
 * @container_id: Container ID
 * @attach: Wait for container process and mirror exit status
 * @container_exit_code: Optional output for container exit code (attach mode)
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_start(const char *container_id, bool attach, int *container_exit_code);

/**
 * nk_container_run - Create and start container (docker-style run)
 * @opts: Run options
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_run(const nk_options_t *opts);

/**
 * nk_container_resume - Re-enter a running container namespace
 * @container_id: Container ID
 * @exec_cmd: Optional command to run via /bin/sh -lc (NULL => interactive shell)
 *
 * Returns: command exit code on success, -1 on runtime/setup error
 */
int nk_container_resume(const char *container_id, const char *exec_cmd);

/**
 * nk_container_delete - Delete a container
 * @container_id: Container ID
 *
 * Returns: 0 on success, -1 on error
 */
int nk_container_delete(const char *container_id);

/**
 * nk_container_state - Query container state
 * @container_id: Container ID
 *
 * Returns: Current state, or -1 on error
 */
nk_container_state_t nk_container_state(const char *container_id);

/**
 * nk_container_free - Free container resources
 * @container: Container to free
 */
void nk_container_free(nk_container_t *container);

#endif /* NK_H */
