#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "nk_log.h"

/* Global log configuration */
nk_log_level_t nk_log_level = NK_LOG_INFO;
bool nk_log_enabled = true;
bool nk_log_educational = false;
static nk_log_role_t nk_log_role = NK_LOG_ROLE_HOST;
static bool nk_log_env_applied = false;

/* Log level names */
static const char *level_names[] = {
    [NK_LOG_DEBUG] = "DEBUG",
    [NK_LOG_INFO]  = "INFO",
    [NK_LOG_WARN]  = "WARN",
    [NK_LOG_ERROR] = "ERROR",
};

/* Log level colors */
static const char *level_colors[] = {
    [NK_LOG_DEBUG] = "\033[0;36m",  /* Cyan */
    [NK_LOG_INFO]  = "\033[0;32m",  /* Green */
    [NK_LOG_WARN]  = "\033[1;33m",  /* Yellow */
    [NK_LOG_ERROR] = "\033[0;31m",  /* Red */
};

/* Log role names */
static const char *role_names[] = {
    [NK_LOG_ROLE_HOST]   = "HOST",
    [NK_LOG_ROLE_PARENT] = "PARENT",
    [NK_LOG_ROLE_CHILD]  = "CHILD",
};

/* Log role colors */
static const char *role_colors[] = {
    [NK_LOG_ROLE_HOST]   = "\033[0;37m",  /* White */
    [NK_LOG_ROLE_PARENT] = "\033[0;34m",  /* Blue */
    [NK_LOG_ROLE_CHILD]  = "\033[0;35m",  /* Magenta */
};

static const char *color_reset = "\033[0m";

static const char *nk_basename(const char *path) {
    const char *slash;

    if (!path) {
        return "?";
    }
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool nk_parse_env_bool(const char *value, bool *out) {
    if (!value || !out) {
        return false;
    }
    if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0 || strcasecmp(value, "off") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool nk_parse_env_level(const char *value, nk_log_level_t *out) {
    if (!value || !out) {
        return false;
    }
    if (strcasecmp(value, "debug") == 0 || strcmp(value, "0") == 0) {
        *out = NK_LOG_DEBUG;
        return true;
    }
    if (strcasecmp(value, "info") == 0 || strcmp(value, "1") == 0) {
        *out = NK_LOG_INFO;
        return true;
    }
    if (strcasecmp(value, "warn") == 0 || strcasecmp(value, "warning") == 0 || strcmp(value, "2") == 0) {
        *out = NK_LOG_WARN;
        return true;
    }
    if (strcasecmp(value, "error") == 0 || strcmp(value, "3") == 0) {
        *out = NK_LOG_ERROR;
        return true;
    }
    return false;
}

void nk_log_apply_env(void) {
    const char *env;
    bool enabled;
    nk_log_level_t level;

    if (nk_log_env_applied) {
        return;
    }
    nk_log_env_applied = true;

    env = getenv("NK_LOG_ENABLED");
    if (nk_parse_env_bool(env, &enabled)) {
        nk_log_enabled = enabled;
    }

    env = getenv("NK_LOG_LEVEL");
    if (nk_parse_env_level(env, &level)) {
        nk_log_level = level;
    }

    env = getenv("NK_LOG_EDUCATIONAL");
    if (nk_parse_env_bool(env, &enabled)) {
        nk_log_educational = enabled;
    }
}

/* Educational explanations for common operations */
static const struct {
    const char *operation;
    const char *explanation;
} educational_notes[] = {
    {"Loading OCI spec",
     "The OCI spec defines everything about the container: what to run, filesystem, namespaces, and resource limits"},

    {"Validating OCI spec",
     "Ensuring the config.json has all required fields like process args, root filesystem, and OCI version"},

    {"Creating container state",
     "Saving container metadata to disk so we can manage it later (start/stop/delete)"},

    {"Parsing namespaces",
     "Namespaces isolate container from host: PID namespace gives container its own PID 1"},

    {"Setting up root filesystem",
     "Container needs its own filesystem view. We'll mount /proc, /sys, /dev and use pivot_root"},

    {"Creating cgroup",
     "Cgroups limit container resources (CPU, memory, PIDs). Different from namespaces which isolate"},

    {"Mounting proc",
     "The /proc filesystem gives process info. Container's /proc shows only container's processes"},

    {"Mounting sysfs",
     "/sys exposes kernel info. Container gets limited view for hardware and kernel parameters"},

    {"Mounting dev",
     "Device nodes (/dev/null, /dev/zero, etc.) needed for most programs. Creating minimal set"},

    {"Pivoting root",
     "Atomic swap of root filesystem. pivot_root() is safer than chroot() for container isolation"},

    {"Setting hostname",
     "In UTS namespace, container can have its own hostname without affecting host"},

    {"Dropping capabilities",
     "Linux capabilities are fine-grained privileges. Container runs with reduced privileges even as root"},

    {"Calling clone()",
     "clone() system call creates new process WITH namespaces. Returns twice: parent gets PID, child gets 0"},

    {"Executing container process",
     "execve() replaces current process with container binary. PID stays same, memory is replaced"},
};

void nk_log_set_level(nk_log_level_t level) {
    nk_log_level = level;
}

void nk_log_enable(bool enabled) {
    nk_log_enabled = enabled;
}

void nk_log_set_educational(bool enabled) {
    nk_log_educational = enabled;
}

void nk_log_set_role(nk_log_role_t role) {
    nk_log_role = role;
}

void nk_log_at(nk_log_level_t level, const char *file, int line, const char *fmt, ...) {
    nk_log_apply_env();
    if (!nk_log_enabled || level < nk_log_level) {
        return;
    }

    /* Get current time */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d.%03d",
             tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(ts.tv_nsec / 1000000));

    /* Format the message */
    va_list args;
    va_start(args, fmt);

    char msg[1024];
    vsnprintf(msg, sizeof(msg), fmt, args);

    va_end(args);

    /* Print with colors if output is a terminal */
    if (isatty(STDERR_FILENO)) {
        fprintf(stderr, "%s[%s]%s [%s%s%s] [%s%s%s] [%s:%d] %s%s%s\n",
                "\033[0;90m",  /* Dim gray for timestamp */
                timestamp,
                color_reset,
                role_colors[nk_log_role], role_names[nk_log_role], color_reset,
                level_colors[level], level_names[level], color_reset,
                nk_basename(file), line,
                "\033[0m", msg, color_reset);
    } else {
        fprintf(stderr, "[%s] [%s] [%s] [%s:%d] %s\n",
                timestamp, role_names[nk_log_role], level_names[level], nk_basename(file), line, msg);
    }
}

void nk_log_explain_at(const char *file, int line, const char *what, const char *why) {
    nk_log_apply_env();
    if (!nk_log_educational) {
        return;
    }

    /* Look up educational note */
    const char *explanation = why;
    if (!explanation) {
        for (size_t i = 0; i < sizeof(educational_notes) / sizeof(educational_notes[0]); i++) {
            if (strstr(what, educational_notes[i].operation) != NULL) {
                explanation = educational_notes[i].explanation;
                break;
            }
        }
    }

    if (explanation) {
        if (isatty(STDERR_FILENO)) {
            fprintf(stderr, "%s      │ [%s%s%s] [%s:%d] %s%s%s\n", "\033[0;90m",
                    role_colors[nk_log_role], role_names[nk_log_role], color_reset,
                    nk_basename(file), line,
                    "\033[0;37m", explanation, "\033[0m");
        } else {
            fprintf(stderr, "      │ [%s] [%s:%d] %s\n",
                    role_names[nk_log_role], nk_basename(file), line, explanation);
        }
    }
}

void nk_log_stderr_at(const char *file, int line, const char *fmt, ...) {
    va_list args;

    nk_log_apply_env();
    if (!nk_log_enabled) {
        return;
    }

    if (isatty(STDERR_FILENO)) {
        fprintf(stderr, "[%s%s%s] [%s:%d] ",
                role_colors[nk_log_role], role_names[nk_log_role], color_reset,
                nk_basename(file), line);
    } else {
        fprintf(stderr, "[%s] [%s:%d] ", role_names[nk_log_role], nk_basename(file), line);
    }

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
