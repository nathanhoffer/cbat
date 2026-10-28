/*
 * cbat/log.c - leveled logging.
 * Messages are formatted into a stack buffer first so that logging still
 * works when memory is exhausted (long messages are then truncated).
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/log.h"

#include <stdarg.h>
#include <string.h>

#include "cbat/time.h"

#define CBAT__LOG_BUFFER 1024

static CBatLogLevel cbat__log_level = CBAT_LOG_INFO;
static FILE *cbat__log_output = NULL;
static CBatLogHandler cbat__log_handler = NULL;
static void *cbat__log_user_data = NULL;
static bool cbat__log_show_time = false;
static bool cbat__log_show_location = false;

void cbat_log_set_level(CBatLogLevel level) {
    cbat__log_level = level;
}

CBatLogLevel cbat_log_get_level(void) {
    return cbat__log_level;
}

void cbat_log_set_output(FILE *output) {
    cbat__log_output = output;
}

void cbat_log_set_handler(CBatLogHandler handler, void *user_data) {
    cbat__log_handler = handler;
    cbat__log_user_data = user_data;
}

void cbat_log_set_show_time(bool show) {
    cbat__log_show_time = show;
}

void cbat_log_set_show_location(bool show) {
    cbat__log_show_location = show;
}

const char *cbat_log_level_name(CBatLogLevel level) {
    switch (level) {
    case CBAT_LOG_DEBUG: return "DEBUG";
    case CBAT_LOG_INFO: return "INFO";
    case CBAT_LOG_WARNING: return "WARNING";
    case CBAT_LOG_ERROR: return "ERROR";
    case CBAT_LOG_OFF: return "OFF";
    default: return "UNKNOWN";
    }
}

static bool cbat__log_equals_ignore_case(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        char x = *a >= 'A' && *a <= 'Z' ? (char)(*a - 'A' + 'a') : *a;
        char y = *b >= 'A' && *b <= 'Z' ? (char)(*b - 'A' + 'a') : *b;
        if (x != y) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

CBatError cbat_log_parse_level(const char *name, CBatLogLevel *out) {
    static const struct {
        const char *name;
        CBatLogLevel level;
    } names[] = {
        {"debug", CBAT_LOG_DEBUG}, {"info", CBAT_LOG_INFO},   {"warning", CBAT_LOG_WARNING},
        {"warn", CBAT_LOG_WARNING}, {"error", CBAT_LOG_ERROR}, {"off", CBAT_LOG_OFF},
    };
    size_t i;
    if (name == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    for (i = 0; i < CBAT_ARRAY_LEN(names); i++) {
        if (cbat__log_equals_ignore_case(name, names[i].name)) {
            *out = names[i].level;
            return CBAT_OK;
        }
    }
    return CBAT_ERR_NOT_FOUND;
}

/* Strips directories so locations read "csv.c:42" rather than full paths. */
static const char *cbat__log_basename(const char *path) {
    const char *base = path;
    const char *p;
    if (path == NULL) {
        return "?";
    }
    for (p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

static void cbat__log_default(CBatLogLevel level, const char *file, int line,
                              const char *message) {
    FILE *out = cbat__log_output != NULL ? cbat__log_output : stderr;
    if (cbat__log_show_time) {
        CBatDateTime now;
        if (cbat_datetime_now(true, &now) == CBAT_OK) {
            fprintf(out, "%04lld-%02d-%02d %02d:%02d:%02d.%03d ", (long long)now.year, now.month,
                    now.day, now.hour, now.minute, now.second, now.nanosecond / 1000000);
        }
    }
    fprintf(out, "%s: ", cbat_log_level_name(level));
    if (cbat__log_show_location) {
        fprintf(out, "%s:%d: ", cbat__log_basename(file), line);
    }
    fprintf(out, "%s\n", message);
    fflush(out);
}

void cbat_log_message(CBatLogLevel level, const char *file, int line, const char *format, ...) {
    char stack_buffer[CBAT__LOG_BUFFER];
    char *message = stack_buffer;
    va_list args;
    int needed;
    if (level < cbat__log_level || level >= CBAT_LOG_OFF || format == NULL) {
        return;
    }
    va_start(args, format);
    needed = vsnprintf(stack_buffer, sizeof(stack_buffer), format, args);
    va_end(args);
    if (needed < 0) {
        return;
    }
    if ((size_t)needed >= sizeof(stack_buffer)) {
        char *heap = (char *)cbat_alloc(NULL, (size_t)needed + 1);
        if (heap != NULL) {
            va_start(args, format);
            vsnprintf(heap, (size_t)needed + 1, format, args);
            va_end(args);
            message = heap;
        } /* else: keep the truncated stack copy */
    }
    if (cbat__log_handler != NULL) {
        cbat__log_handler(level, file, line, message, cbat__log_user_data);
    } else {
        cbat__log_default(level, file, line, message);
    }
    if (message != stack_buffer) {
        cbat_free(message);
    }
}
