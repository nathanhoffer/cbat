/*
 * cbat/path.c - lexical path manipulation.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/path.h"

#include <string.h>

bool cbat_path_is_separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

/* Length of a Windows drive prefix ("C:") or 0. */
static size_t cbat__path_drive_length(const char *path) {
#ifdef _WIN32
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        return 2;
    }
#else
    CBAT_UNUSED(path);
#endif
    return 0;
}

bool cbat_path_is_absolute(const char *path) {
    if (path == NULL) {
        return false;
    }
    return cbat_path_is_separator(path[cbat__path_drive_length(path)]);
}

CBatString *cbat_path_join(const char *base, const char *child) {
    CBatString *result;
    size_t base_length;
    if (base == NULL) {
        base = "";
    }
    if (child == NULL) {
        child = "";
    }
    if (cbat_path_is_absolute(child) || base[0] == '\0') {
        return cbat_string_from(child);
    }
    result = cbat_string_from(base);
    if (result == NULL) {
        return NULL;
    }
    base_length = strlen(base);
    if (child[0] != '\0' && !cbat_path_is_separator(base[base_length - 1]) &&
        cbat_string_append_char(result, '/') != CBAT_OK) {
        cbat_string_free(result);
        return NULL;
    }
    if (cbat_string_append(result, child) != CBAT_OK) {
        cbat_string_free(result);
        return NULL;
    }
    return result;
}

CBatString *cbat_path_join_many(const char *const *parts, size_t count) {
    CBatString *result = cbat_string_new();
    size_t i;
    for (i = 0; result != NULL && parts != NULL && i < count; i++) {
        CBatString *next = cbat_path_join(cbat_string_cstr(result), parts[i]);
        cbat_string_free(result);
        result = next;
    }
    return result;
}

/* Finds the last component as [start, end) ignoring trailing separators. */
static void cbat__path_last_component(const char *path, size_t *start, size_t *end) {
    size_t root = cbat__path_drive_length(path);
    size_t e = strlen(path);
    size_t s;
    while (e > root && cbat_path_is_separator(path[e - 1])) {
        e--;
    }
    s = e;
    while (s > root && !cbat_path_is_separator(path[s - 1])) {
        s--;
    }
    *start = s;
    *end = e;
}

CBatString *cbat_path_basename(const char *path) {
    size_t start;
    size_t end;
    if (path == NULL) {
        path = "";
    }
    cbat__path_last_component(path, &start, &end);
    return cbat_string_from_length(path + start, end - start);
}

CBatString *cbat_path_dirname(const char *path) {
    size_t start;
    size_t end;
    size_t drive;
    if (path == NULL) {
        path = "";
    }
    drive = cbat__path_drive_length(path);
    cbat__path_last_component(path, &start, &end);
    /* Drop separators between the parent and the last component. */
    while (start > drive && cbat_path_is_separator(path[start - 1])) {
        start--;
    }
    if (start > drive) {
        return cbat_string_from_length(path, start);
    }
    if (cbat_path_is_separator(path[drive])) {
        return cbat_string_from_length(path, drive + 1); /* root, e.g. "/" or "C:\" */
    }
    if (drive > 0) {
        return cbat_string_from_length(path, drive); /* "C:" */
    }
    return cbat_string_from(".");
}

/* Offset of the extension's dot inside [start, end), or `end` if none. */
static size_t cbat__path_extension_start(const char *path, size_t start, size_t end) {
    size_t i = end;
    while (i > start + 1) {
        i--;
        if (path[i] == '.') {
            return i + 1 == end ? end : i; /* trailing dot: no extension */
        }
    }
    return end;
}

CBatString *cbat_path_extension(const char *path) {
    size_t start;
    size_t end;
    size_t dot;
    if (path == NULL) {
        path = "";
    }
    cbat__path_last_component(path, &start, &end);
    dot = cbat__path_extension_start(path, start, end);
    return cbat_string_from_length(path + dot, end - dot);
}

CBatString *cbat_path_stem(const char *path) {
    size_t start;
    size_t end;
    size_t dot;
    if (path == NULL) {
        path = "";
    }
    cbat__path_last_component(path, &start, &end);
    dot = cbat__path_extension_start(path, start, end);
    return cbat_string_from_length(path + start, dot - start);
}

CBatString *cbat_path_with_extension(const char *path, const char *extension) {
    size_t start;
    size_t end;
    size_t dot;
    CBatString *result;
    CBatError err = CBAT_OK;
    if (path == NULL) {
        path = "";
    }
    if (extension == NULL) {
        extension = "";
    }
    cbat__path_last_component(path, &start, &end);
    dot = cbat__path_extension_start(path, start, end);
    result = cbat_string_from_length(path, dot);
    if (result == NULL) {
        return NULL;
    }
    if (extension[0] != '\0' && extension[0] != '.') {
        err = cbat_string_append_char(result, '.');
    }
    if (err == CBAT_OK) {
        err = cbat_string_append(result, extension);
    }
    if (err != CBAT_OK) {
        cbat_string_free(result);
        return NULL;
    }
    return result;
}

CBatString *cbat_path_normalize(const char *path) {
    CBatString *result;
    size_t drive;
    size_t i;
    size_t root_length;
    bool absolute;
    CBatError err = CBAT_OK;
    if (path == NULL) {
        path = "";
    }
    drive = cbat__path_drive_length(path);
    absolute = cbat_path_is_separator(path[drive]);
    result = cbat_string_from_length(path, drive);
    if (result == NULL) {
        return NULL;
    }
    if (absolute) {
        err = cbat_string_append_char(result, '/');
    }
    root_length = result->length;
    i = drive;
    while (err == CBAT_OK && path[i] != '\0') {
        size_t start;
        size_t length;
        while (cbat_path_is_separator(path[i])) {
            i++;
        }
        start = i;
        while (path[i] != '\0' && !cbat_path_is_separator(path[i])) {
            i++;
        }
        length = i - start;
        if (length == 0 || (length == 1 && path[start] == '.')) {
            continue;
        }
        if (length == 2 && path[start] == '.' && path[start + 1] == '.') {
            /* Pop the previous component unless it is itself "..". */
            size_t previous = result->length;
            while (previous > root_length && result->data[previous - 1] != '/') {
                previous--;
            }
            if (result->length > root_length &&
                !(result->length - previous == 2 && result->data[previous] == '.' &&
                  result->data[previous + 1] == '.')) {
                cbat_string_truncate(result, previous > root_length ? previous - 1 : root_length);
                continue;
            }
            if (absolute) {
                continue; /* cannot go above the root */
            }
        }
        if (result->length > root_length) {
            err = cbat_string_append_char(result, '/');
        }
        if (err == CBAT_OK) {
            err = cbat_string_append_length(result, path + start, length);
        }
    }
    if (err == CBAT_OK && result->length == 0) {
        err = cbat_string_append_char(result, '.');
    }
    if (err != CBAT_OK) {
        cbat_string_free(result);
        return NULL;
    }
    return result;
}
