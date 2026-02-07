#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>

#include "nk_oci.h"

#define CONFIG_JSON "config.json"

static char *load_json_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Failed to open %s: %s\n", path, strerror(errno));
        return NULL;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read entire file */
    char *content = malloc(size + 1);
    if (!content) {
        fprintf(stderr, "Error: Failed to allocate memory for config\n");
        fclose(f);
        return NULL;
    }

    size_t read = fread(content, 1, size, f);
    content[read] = '\0';
    fclose(f);

    return content;
}

static nk_oci_process_t *parse_process(json_t *proc_obj) {
    nk_oci_process_t *proc = calloc(1, sizeof(*proc));
    if (!proc) {
        return NULL;
    }

    /* Parse terminal flag */
    json_t *term = json_object_get(proc_obj, "terminal");
    if (term) {
        proc->terminal = json_is_true(term);
    }

    /* Parse console size */
    json_t *console_size = json_object_get(proc_obj, "consoleSize");
    if (console_size && json_is_string(console_size)) {
        proc->console_size = strdup(json_string_value(console_size));
    }

    /* Parse user */
    json_t *user = json_object_get(proc_obj, "user");
    if (user && json_is_object(user)) {
        json_t *uid = json_object_get(user, "uid");
        json_t *gid = json_object_get(user, "gid");
        if (uid) proc->uid = json_integer_value(uid);
        if (gid) proc->gid = json_integer_value(gid);
    }

    /* Parse args (required) */
    json_t *args = json_object_get(proc_obj, "args");
    if (!args || !json_is_array(args)) {
        fprintf(stderr, "Error: Process args is required\n");
        free(proc);
        return NULL;
    }

    size_t args_len = json_array_size(args);
    proc->args = calloc(args_len, sizeof(char *));
    if (!proc->args) {
        free(proc);
        return NULL;
    }

    for (size_t i = 0; i < args_len; i++) {
        json_t *arg = json_array_get(args, i);
        if (json_is_string(arg)) {
            proc->args[i] = strdup(json_string_value(arg));
        }
    }
    proc->args_len = args_len;

    /* Parse env (optional) */
    json_t *env = json_object_get(proc_obj, "env");
    if (env && json_is_array(env)) {
        size_t env_len = json_array_size(env);
        proc->env = calloc(env_len, sizeof(char *));
        if (proc->env) {
            for (size_t i = 0; i < env_len; i++) {
                json_t *e = json_array_get(env, i);
                if (json_is_string(e)) {
                    proc->env[i] = strdup(json_string_value(e));
                }
            }
            proc->env_len = env_len;
        }
    }

    /* Parse cwd */
    json_t *cwd = json_object_get(proc_obj, "cwd");
    if (cwd && json_is_string(cwd)) {
        proc->cwd = strdup(json_string_value(cwd));
    } else {
        proc->cwd = strdup("/");
    }

    /* Parse noNewPrivileges */
    json_t *nnp = json_object_get(proc_obj, "noNewPrivileges");
    if (nnp) {
        proc->no_new_privileges = json_is_true(nnp);
    }

    return proc;
}

static nk_oci_root_t *parse_root(json_t *root_obj) {
    nk_oci_root_t *root = calloc(1, sizeof(*root));
    if (!root) {
        return NULL;
    }

    /* Parse path (required) */
    json_t *path = json_object_get(root_obj, "path");
    if (!path || !json_is_string(path)) {
        fprintf(stderr, "Error: Root path is required\n");
        free(root);
        return NULL;
    }
    root->path = strdup(json_string_value(path));

    /* Parse readonly */
    json_t *readonly = json_object_get(root_obj, "readonly");
    if (readonly) {
        root->readonly = json_is_true(readonly);
    }

    return root;
}

static nk_oci_linux_t *parse_linux(json_t *linux_obj) {
    nk_oci_linux_t *linux_cfg = calloc(1, sizeof(*linux_cfg));
    if (!linux_cfg) {
        return NULL;
    }

    /* Parse namespaces */
    json_t *namespaces = json_object_get(linux_obj, "namespaces");
    if (namespaces && json_is_array(namespaces)) {
        size_t ns_len = json_array_size(namespaces);
        linux_cfg->namespaces = calloc(ns_len, sizeof(nk_oci_namespace_t));
        if (linux_cfg->namespaces) {
            for (size_t i = 0; i < ns_len; i++) {
                json_t *ns = json_array_get(namespaces, i);
                json_t *type = json_object_get(ns, "type");
                json_t *path = json_object_get(ns, "path");

                if (type && json_is_string(type)) {
                    linux_cfg->namespaces[i].type = strdup(json_string_value(type));
                    if (path && json_is_string(path)) {
                        linux_cfg->namespaces[i].path = strdup(json_string_value(path));
                    }
                    linux_cfg->namespaces_len++;
                }
            }
        }
    }

    /* Parse rootfs propagation */
    json_t *prop = json_object_get(linux_obj, "rootfsPropagation");
    if (prop && json_is_string(prop)) {
        linux_cfg->rootfs_propagation = strdup(json_string_value(prop));
    }

    /* TODO: Parse resources */

    return linux_cfg;
}

nk_oci_spec_t *nk_oci_spec_load(const char *bundle_path) {
    char config_path[PATH_MAX];
    snprintf(config_path, sizeof(config_path), "%s/%s", bundle_path, CONFIG_JSON);

    /* Load and parse JSON */
    char *json_content = load_json_file(config_path);
    if (!json_content) {
        return NULL;
    }

    json_error_t error;
    json_t *root = json_loads(json_content, 0, &error);
    free(json_content);

    if (!root) {
        fprintf(stderr, "Error: Failed to parse config.json: %s at line %d\n",
                error.text, error.line);
        return NULL;
    }

    nk_oci_spec_t *spec = calloc(1, sizeof(*spec));
    if (!spec) {
        json_decref(root);
        return NULL;
    }

    /* Parse OCI version */
    json_t *version = json_object_get(root, "ociVersion");
    if (version && json_is_string(version)) {
        spec->oci_version = strdup(json_string_value(version));
    }

    /* Parse process (optional for create) */
    json_t *process = json_object_get(root, "process");
    if (process) {
        spec->process = parse_process(process);
    }

    /* Parse root */
    json_t *root_obj = json_object_get(root, "root");
    if (root_obj) {
        spec->root = parse_root(root_obj);
    }

    /* Parse hostname */
    json_t *hostname = json_object_get(root, "hostname");
    if (hostname && json_is_string(hostname)) {
        spec->hostname = strdup(json_string_value(hostname));
    }

    /* Parse mounts */
    json_t *mounts = json_object_get(root, "mounts");
    if (mounts && json_is_array(mounts)) {
        size_t mounts_len = json_array_size(mounts);
        spec->mounts = calloc(mounts_len, sizeof(nk_oci_mount_t));
        if (spec->mounts) {
            for (size_t i = 0; i < mounts_len; i++) {
                json_t *m = json_array_get(mounts, i);
                json_t *dest = json_object_get(m, "destination");
                json_t *type = json_object_get(m, "type");
                json_t *src = json_object_get(m, "source");
                json_t *opts = json_object_get(m, "options");

                if (dest && json_is_string(dest)) {
                    spec->mounts[i].destination = strdup(json_string_value(dest));
                    if (type && json_is_string(type)) {
                        spec->mounts[i].type = strdup(json_string_value(type));
                    }
                    if (src && json_is_string(src)) {
                        spec->mounts[i].source = strdup(json_string_value(src));
                    }
                    if (opts && json_is_array(opts)) {
                        size_t opts_len = json_array_size(opts);
                        spec->mounts[i].options = calloc(opts_len, sizeof(char *));
                        if (spec->mounts[i].options) {
                            for (size_t j = 0; j < opts_len; j++) {
                                json_t *opt = json_array_get(opts, j);
                                if (json_is_string(opt)) {
                                    spec->mounts[i].options[j] = strdup(json_string_value(opt));
                                }
                            }
                            spec->mounts[i].options_len = opts_len;
                        }
                    }
                    spec->mounts_len++;
                }
            }
        }
    }

    /* Parse Linux-specific configuration */
    json_t *linux_obj = json_object_get(root, "linux");
    if (linux_obj) {
        spec->linux_config = parse_linux(linux_obj);
    }

    /* Parse annotations */
    json_t *annotations = json_object_get(root, "annotations");
    if (annotations && json_is_object(annotations)) {
        const char *key;
        json_t *value;
        size_t ann_len = json_object_size(annotations);

        spec->annotations = calloc(ann_len * 2, sizeof(char *));
        if (spec->annotations) {
            size_t i = 0;
            json_object_foreach(annotations, key, value) {
                if (json_is_string(value)) {
                    char *ann = malloc(strlen(key) + strlen(json_string_value(value)) + 2);
                    sprintf(ann, "%s=%s", key, json_string_value(value));
                    spec->annotations[i++] = ann;
                    spec->annotations_len++;
                }
            }
        }
    }

    json_decref(root);
    return spec;
}

static void nk_oci_process_free(nk_oci_process_t *proc) {
    if (!proc) return;

    for (size_t i = 0; i < proc->args_len; i++) {
        free(proc->args[i]);
    }
    free(proc->args);

    for (size_t i = 0; i < proc->env_len; i++) {
        free(proc->env[i]);
    }
    free(proc->env);

    free(proc->cwd);
    free(proc->console_size);
    free(proc->user);
    free(proc->additional_gids);
    free(proc);
}

static void nk_oci_root_free(nk_oci_root_t *root) {
    if (!root) return;
    free(root->path);
    free(root);
}

void nk_oci_spec_free(nk_oci_spec_t *spec) {
    if (!spec) return;

    free(spec->oci_version);
    nk_oci_process_free(spec->process);
    nk_oci_root_free(spec->root);
    free(spec->hostname);

    for (size_t i = 0; i < spec->mounts_len; i++) {
        free(spec->mounts[i].destination);
        free(spec->mounts[i].type);
        free(spec->mounts[i].source);
        for (size_t j = 0; j < spec->mounts[i].options_len; j++) {
            free(spec->mounts[i].options[j]);
        }
        free(spec->mounts[i].options);
    }
    free(spec->mounts);

    if (spec->linux_config) {
        for (size_t i = 0; i < spec->linux_config->namespaces_len; i++) {
            free(spec->linux_config->namespaces[i].type);
            free(spec->linux_config->namespaces[i].path);
        }
        free(spec->linux_config->namespaces);
        free(spec->linux_config->rootfs_propagation);
        free(spec->linux_config->resources);
        free(spec->linux_config);
    }

    for (size_t i = 0; i < spec->annotations_len; i++) {
        free(spec->annotations[i]);
    }
    free(spec->annotations);

    free(spec);
}

bool nk_oci_spec_validate(const nk_oci_spec_t *spec) {
    if (!spec) {
        fprintf(stderr, "Error: Spec is NULL\n");
        return false;
    }

    if (!spec->root || !spec->root->path) {
        fprintf(stderr, "Error: Root path is required\n");
        return false;
    }

    if (!spec->process) {
        fprintf(stderr, "Error: Process configuration is required\n");
        return false;
    }

    if (!spec->process->args || spec->process->args_len == 0) {
        fprintf(stderr, "Error: Process args are required\n");
        return false;
    }

    return true;
}

const char *nk_oci_spec_get_annotation(const nk_oci_spec_t *spec, const char *key) {
    if (!spec || !key) {
        return NULL;
    }

    size_t key_len = strlen(key);
    for (size_t i = 0; i < spec->annotations_len; i++) {
        if (strncmp(spec->annotations[i], key, key_len) == 0 &&
            spec->annotations[i][key_len] == '=') {
            return &spec->annotations[i][key_len + 1];
        }
    }

    return NULL;
}
