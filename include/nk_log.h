#ifndef NK_LOG_H
#define NK_LOG_H

#include <stdio.h>
#include <stdbool.h>

/* Log levels */
typedef enum {
    NK_LOG_DEBUG = 0,
    NK_LOG_INFO,
    NK_LOG_WARN,
    NK_LOG_ERROR
} nk_log_level_t;

/* Log role (originating process context) */
typedef enum {
    NK_LOG_ROLE_HOST = 0,
    NK_LOG_ROLE_PARENT,
    NK_LOG_ROLE_CHILD
} nk_log_role_t;

/* Global log configuration */
extern nk_log_level_t nk_log_level;
extern bool nk_log_enabled;
extern bool nk_log_educational;  /* Enable educational explanations */

/**
 * nk_log_apply_env - Apply environment overrides for logging
 *
 * Supported env vars:
 *  - NK_LOG_ENABLED: 0/1, false/true, no/yes
 *  - NK_LOG_LEVEL: debug|info|warn|error or 0-3
 *  - NK_LOG_EDUCATIONAL: 0/1, false/true, no/yes
 */
void nk_log_apply_env(void);

/**
 * nk_log_set_level - Set the minimum log level
 * @level: Minimum level to log
 */
void nk_log_set_level(nk_log_level_t level);

/**
 * nk_log_enable - Enable or disable logging
 * @enabled: true to enable, false to disable
 */
void nk_log_enable(bool enabled);

/**
 * nk_log_set_educational - Enable educational mode (explains what's happening)
 * @enabled: true for educational mode
 */
void nk_log_set_educational(bool enabled);

/**
 * nk_log_set_role - Set process role used in log prefixes
 * @role: Log role (host/parent/child)
 */
void nk_log_set_role(nk_log_role_t role);

/**
 * nk_log_at - Log a message at specified level with call-site source location
 * @level: Log level
 * @file: Source file
 * @line: Source line
 * @fmt: Printf-style format string
 */
void nk_log_at(nk_log_level_t level, const char *file, int line, const char *fmt, ...);

/**
 * nk_log_explain_at - Log an educational explanation with source location
 * @file: Source file
 * @line: Source line
 * @what: What operation is being performed
 * @why: Why it's needed (educational context)
 */
void nk_log_explain_at(const char *file, int line, const char *what, const char *why);

/**
 * nk_log_stderr_at - Print stderr line with source location prefix
 * @file: Source file
 * @line: Source line
 * @fmt: Printf-style format string
 */
void nk_log_stderr_at(const char *file, int line, const char *fmt, ...);

/* Convenience macros */
#define nk_log(level, fmt, ...) \
    nk_log_at((level), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define nk_log_debug(fmt, ...) \
    nk_log_at(NK_LOG_DEBUG, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define nk_log_info(fmt, ...) \
    nk_log_at(NK_LOG_INFO, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define nk_log_warn(fmt, ...) \
    nk_log_at(NK_LOG_WARN, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define nk_log_error(fmt, ...) \
    nk_log_at(NK_LOG_ERROR, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define nk_log_explain(what, why) \
    nk_log_explain_at(__FILE__, __LINE__, (what), (why))
#define nk_stderr(fmt, ...) \
    nk_log_stderr_at(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)

/* Educational logging macros */
#define nk_log_step(n, what) \
    do { \
        nk_log_info("[%d] %s", n, what); \
        if (nk_log_educational) nk_log_explain(what, NULL); \
    } while(0)

#define nk_log_explain_op(what, why) \
    do { \
        nk_log_info("→ %s", what); \
        if (nk_log_educational) nk_log_explain(what, why); \
    } while(0)

#endif /* NK_LOG_H */
