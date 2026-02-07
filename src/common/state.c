#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <jansson.h>

#include "nk.h"

#define NK_STATE_DIR "run"
#define STATE_FILE "state.json"

static const char *state_to_string(nk_container_state_t state) {
    switch (state) {
    case NK_STATE_CREATED:
        return "created";
    case NK_STATE_RUNNING:
        return "running";
    case NK_STATE_STOPPED:
        return "stopped";
    case NK_STATE_PAUSED:
        return "paused";
    default:
        return "unknown";
    }
}

static nk_container_state_t string_to_state(const char *str) {
    if (strcmp(str, "created") == 0) return NK_STATE_CREATED;
    if (strcmp(str, "running") == 0) return NK_STATE_RUNNING;
    if (strcmp(str, "stopped") == 0) return NK_STATE_STOPPED;
    if (strcmp(str, "paused") == 0) return NK_STATE_PAUSED;
    return NK_STATE_CREATED;
}

static const char *mode_to_string(nk_execution_mode_t mode) {
    return mode == NK_MODE_VM ? "vm" : "container";
}

static nk_execution_mode_t string_to_mode(const char *str) {
    if (strcmp(str, "vm") == 0) return NK_MODE_VM;
    return NK_MODE_CONTAINER;
}

static char *get_container_state_path(const char *container_id) {
    char *path = NULL;
    if (asprintf(&path, "%s/%s/%s", NK_STATE_DIR, container_id, STATE_FILE) == -1) {
        return NULL;
    }
    return path;
}

static char *get_container_dir(const char *container_id) {
    char *path = NULL;
    if (asprintf(&path, "%s/%s", NK_STATE_DIR, container_id) == -1) {
        return NULL;
    }
    return path;
}

static int ensure_container_dir(const char *container_id) {
    char *dir = get_container_dir(container_id);
    if (!dir) {
        return -1;
    }

    struct stat st;
    int ret = 0;

    if (stat(dir, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: %s exists but is not a directory\n", dir);
            ret = -1;
        }
    } else if (mkdir(dir, 0755) == -1) {
        fprintf(stderr, "Error: Failed to create directory %s: %s\n",
                dir, strerror(errno));
        ret = -1;
    }

    free(dir);
    return ret;
}

/**
 * nk_state_save - Save container state to disk
 */
int nk_state_save(const nk_container_t *container) {
    if (!container || !container->id) {
        return -1;
    }

    if (ensure_container_dir(container->id) == -1) {
        return -1;
    }

    char *state_path = get_container_state_path(container->id);
    if (!state_path) {
        return -1;
    }

    /* Create JSON object */
    json_t *root = json_object();
    if (!root) {
        free(state_path);
        return -1;
    }

    json_object_set_new(root, "id", json_string(container->id));
    json_object_set_new(root, "bundle_path", json_string(container->bundle_path ? container->bundle_path : ""));
    json_object_set_new(root, "state", json_string(state_to_string(container->state)));
    json_object_set_new(root, "mode", json_string(mode_to_string(container->mode)));
    json_object_set_new(root, "pid", json_integer(container->init_pid));

    /* Write to file */
    int ret = 0;
    FILE *f = fopen(state_path, "w");
    if (f) {
        if (json_dumpf(root, f, JSON_INDENT(2)) == -1) {
            fprintf(stderr, "Error: Failed to write state file\n");
            ret = -1;
        }
        fclose(f);
    } else {
        fprintf(stderr, "Error: Failed to open state file for writing: %s\n",
                strerror(errno));
        ret = -1;
    }

    json_decref(root);
    free(state_path);
    return ret;
}

/**
 * nk_state_load - Load container state from disk
 */
nk_container_t *nk_state_load(const char *container_id) {
    if (!container_id) {
        return NULL;
    }

    char *state_path = get_container_state_path(container_id);
    if (!state_path) {
        return NULL;
    }

    /* Read JSON file */
    FILE *f = fopen(state_path, "r");
    if (!f) {
        fprintf(stderr, "Error: Failed to open state file for reading: %s\n",
                strerror(errno));
        free(state_path);
        return NULL;
    }

    json_error_t error;
    json_t *root = json_loadf(f, 0, &error);
    fclose(f);
    free(state_path);

    if (!root) {
        fprintf(stderr, "Error: Failed to parse state file: %s at line %d\n",
                error.text, error.line);
        return NULL;
    }

    /* Parse JSON into container structure */
    nk_container_t *container = calloc(1, sizeof(*container));
    if (!container) {
        json_decref(root);
        return NULL;
    }

    json_t *id = json_object_get(root, "id");
    json_t *bundle = json_object_get(root, "bundle_path");
    json_t *state = json_object_get(root, "state");
    json_t *mode = json_object_get(root, "mode");
    json_t *pid = json_object_get(root, "pid");

    if (id && json_is_string(id)) {
        container->id = strdup(json_string_value(id));
    }
    if (bundle && json_is_string(bundle)) {
        const char *bundle_str = json_string_value(bundle);
        if (strlen(bundle_str) > 0) {
            container->bundle_path = strdup(bundle_str);
        }
    }
    if (state && json_is_string(state)) {
        container->state = string_to_state(json_string_value(state));
    }
    if (mode && json_is_string(mode)) {
        container->mode = string_to_mode(json_string_value(mode));
    }
    if (pid && json_is_integer(pid)) {
        container->init_pid = json_integer_value(pid);
    }

    container->control_fd = -1;
    container->state_file = state_path = get_container_state_path(container_id);

    json_decref(root);
    return container;
}

/**
 * nk_state_delete - Delete container state from disk
 */
int nk_state_delete(const char *container_id) {
    if (!container_id) {
        return -1;
    }

    char *state_path = get_container_state_path(container_id);
    if (!state_path) {
        return -1;
    }

    int ret = unlink(state_path);
    if (ret == -1 && errno != ENOENT) {
        fprintf(stderr, "Error: Failed to delete state file: %s\n",
                strerror(errno));
    }

    free(state_path);

    /* Try to remove container directory */
    char *dir = get_container_dir(container_id);
    if (dir) {
        rmdir(dir);
        free(dir);
    }

    return ret;
}

/**
 * nk_state_exists - Check if container state exists
 */
bool nk_state_exists(const char *container_id) {
    if (!container_id) {
        return false;
    }

    char *state_path = get_container_state_path(container_id);
    if (!state_path) {
        return false;
    }

    struct stat st;
    bool exists = (stat(state_path, &st) == 0 && S_ISREG(st.st_mode));

    free(state_path);
    return exists;
}
