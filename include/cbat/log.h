/*
 * cbat/log.h - leveled logging (cbat's answer to Python's logging).
 *
 *     cbat_log_set_level(CBAT_LOG_DEBUG);
 *     CBAT_LOG_INFO("loaded %d rows from %s", count, path);
 *     CBAT_LOG_ERROR("could not open %s: %s", path, cbat_error_str(err));
 *
 * Output goes to stderr by default as "LEVEL: message", optionally with a
 * timestamp and source location. A custom handler can capture messages
 * (the app uses this to show them in its console). Configuration is global
 * and not thread-safe; set it up once at startup.
 * Keep configuration fixed while worker threads run. Custom handlers, user
 * data and shared output streams must support concurrent calls or be locked
 * by the caller. Do not change configuration from a handler.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_LOG_H
#define CBAT_LOG_H

#include <stdio.h>

#include "cbat/base.h"

CBAT_BEGIN_DECLS

typedef enum CBatLogLevel {
    CBAT_LOG_DEBUG,
    CBAT_LOG_INFO,
    CBAT_LOG_WARNING,
    CBAT_LOG_ERROR,
    CBAT_LOG_OFF /* as a threshold: log nothing */
} CBatLogLevel;

/** Receives each message that passes the level filter (already formatted). */
typedef void (*CBatLogHandler)(CBatLogLevel level, const char *file, int line,
                               const char *message, void *user_data);

/** Messages below `level` are dropped. Default: CBAT_LOG_INFO. */
CBAT_API void cbat_log_set_level(CBatLogLevel level);
CBAT_API CBatLogLevel cbat_log_get_level(void);

/** Where the default handler writes (NULL = stderr). */
CBAT_API void cbat_log_set_output(FILE *output);

/** Replaces the default handler (NULL restores it). */
CBAT_API void cbat_log_set_handler(CBatLogHandler handler, void *user_data);

/** Default-handler options: prefix lines with local time / "file:line". */
CBAT_API void cbat_log_set_show_time(bool show);
CBAT_API void cbat_log_set_show_location(bool show);

/** "DEBUG", "INFO", "WARNING", "ERROR" or "OFF". */
CBAT_API const char *cbat_log_level_name(CBatLogLevel level);

/** Parses a level name, case-insensitively ("warn" is accepted too). */
CBAT_API CBatError cbat_log_parse_level(const char *name, CBatLogLevel *out);

/** Logs a printf-style message. Prefer the CBAT_LOG_* macros. */
CBAT_API void cbat_log_message(CBatLogLevel level, const char *file, int line,
                               const char *format, ...) CBAT_PRINTF_FORMAT(4, 5);

#define CBAT_LOG_DEBUG(...) cbat_log_message(CBAT_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define CBAT_LOG_INFO(...) cbat_log_message(CBAT_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define CBAT_LOG_WARNING(...) cbat_log_message(CBAT_LOG_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define CBAT_LOG_ERROR(...) cbat_log_message(CBAT_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

CBAT_END_DECLS

#endif /* CBAT_LOG_H */
