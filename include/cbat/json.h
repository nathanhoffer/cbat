/*
 * cbat/json.h - parse, build, query and print JSON (cbat's answer to
 * Python's json module).
 *
 *     CBatJson *doc = NULL;
 *     CBatParseInfo info;
 *     if (cbat_json_parse(text, strlen(text), &doc, &info) != CBAT_OK) {
 *         printf("line %zu col %zu: %s\n", info.line, info.column, info.message);
 *     }
 *     const char *name = cbat_json_as_string(cbat_json_pointer(doc, "/user/name"), "?");
 *
 *     CBatJson *reply = cbat_json_new_object();
 *     cbat_json_object_set_string(reply, "status", "ok");
 *     CBatString *text_out = NULL;
 *     cbat_json_to_string(reply, 2, &text_out);   // pretty-printed
 *
 * Objects keep insertion order. Numbers are doubles; integers that fit in
 * int64 are also kept exactly. Strings are UTF-8 (validated when parsing).
 *
 * Ownership: a value added to an array/object (append/set) belongs to the
 * container on success; on failure the caller still owns it.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_JSON_H
#define CBAT_JSON_H

#include "cbat/base.h"
#include "cbat/string.h"

CBAT_BEGIN_DECLS

typedef enum CBatJsonType {
    CBAT_JSON_NULL,
    CBAT_JSON_BOOL,
    CBAT_JSON_NUMBER,
    CBAT_JSON_STRING,
    CBAT_JSON_ARRAY,
    CBAT_JSON_OBJECT
} CBatJsonType;

typedef struct CBatJson CBatJson;

/** Maximum nesting depth accepted by the parser. */
#define CBAT_JSON_MAX_DEPTH 512

/** Optional extensions; max_depth 0 uses CBAT_JSON_MAX_DEPTH, larger limits are rejected. */
typedef struct CBatJsonParseOptions {
    bool allow_comments;       /**< C-style block and // line comments. */
    bool allow_trailing_commas;
    size_t max_depth;
} CBatJsonParseOptions;

/* ---- Parsing and printing ----------------------------------------------- */

/** Parses one JSON document (RFC 8259). `info` may be NULL. */
CBAT_API CBatError cbat_json_parse(const char *text, size_t length, CBatJson **out,
                                   CBatParseInfo *info);

/** Parses with optional extensions (NULL = strict defaults). Output unchanged on error. */
CBAT_API CBatError cbat_json_parse_with(const char *text, size_t length,
    const CBatJsonParseOptions *options, CBatJson **out, CBatParseInfo *info);

/** Serializes with object keys sorted recursively by unsigned UTF-8 bytes; source unchanged. */
CBAT_API CBatError cbat_json_to_string_sorted(const CBatJson *value, int indent, CBatString **out);

/** RFC 7396 merge patch in place; deep copies patch values. Atomic on OOM, including aliases. */
CBAT_API CBatError cbat_json_merge_patch(CBatJson *target, const CBatJson *patch);

/** Reads and parses a JSON file. */
CBAT_API CBatError cbat_json_parse_file(const char *path, CBatJson **out, CBatParseInfo *info);

/**
 * Serializes to text. indent 0 = compact; > 0 = pretty with that many spaces.
 * NaN and infinity cannot be represented: CBAT_ERR_OUT_OF_RANGE.
 */
CBAT_API CBatError cbat_json_to_string(const CBatJson *value, int indent, CBatString **out);

/** Serializes and writes to a file. */
CBAT_API CBatError cbat_json_write_file(const char *path, const CBatJson *value, int indent);

/* ---- Creating ----------------------------------------------------------- */
/* Each returns NULL only when out of memory. */

CBAT_API CBatJson *cbat_json_new_null(void);
CBAT_API CBatJson *cbat_json_new_bool(bool value);
CBAT_API CBatJson *cbat_json_new_number(double value);
CBAT_API CBatJson *cbat_json_new_int(int64_t value);
/** Copies `value` (NULL gives JSON null). */
CBAT_API CBatJson *cbat_json_new_string(const char *value);
CBAT_API CBatJson *cbat_json_new_string_length(const char *value, size_t length);
CBAT_API CBatJson *cbat_json_new_array(void);
CBAT_API CBatJson *cbat_json_new_object(void);

/** Deep copy. */
CBAT_API CBatJson *cbat_json_copy(const CBatJson *value);

/** Frees a value and everything inside it. NULL is ignored. */
CBAT_API void cbat_json_free(CBatJson *value);

/* ---- Inspecting --------------------------------------------------------- */

/** Type of `value` (CBAT_JSON_NULL for a NULL pointer). */
CBAT_API CBatJsonType cbat_json_type(const CBatJson *value);

/** "null", "bool", "number", "string", "array" or "object". */
CBAT_API const char *cbat_json_type_name(CBatJsonType type);

/** Deep equality (object key order does not matter). */
CBAT_API bool cbat_json_equals(const CBatJson *a, const CBatJson *b);

/* Strict getters: CBAT_ERR_TYPE when the value has another type. */
CBAT_API CBatError cbat_json_get_bool(const CBatJson *value, bool *out);
CBAT_API CBatError cbat_json_get_number(const CBatJson *value, double *out);
/** Integral numbers only (CBAT_ERR_TYPE for 1.5, CBAT_ERR_OVERFLOW for 1e30). */
CBAT_API CBatError cbat_json_get_int64(const CBatJson *value, int64_t *out);
/** Borrowed pointer, valid while the value lives. `length` may be NULL. */
CBAT_API CBatError cbat_json_get_string(const CBatJson *value, const char **out, size_t *length);

/* Forgiving getters: return `fallback` when missing or of another type. */
CBAT_API bool cbat_json_as_bool(const CBatJson *value, bool fallback);
CBAT_API double cbat_json_as_number(const CBatJson *value, double fallback);
CBAT_API int64_t cbat_json_as_int64(const CBatJson *value, int64_t fallback);
CBAT_API const char *cbat_json_as_string(const CBatJson *value, const char *fallback);

/* ---- Arrays ------------------------------------------------------------- */

/** Number of items (array) or members (object); 0 for anything else. */
CBAT_API size_t cbat_json_length(const CBatJson *value);

/** Item `index` of an array, or NULL. */
CBAT_API CBatJson *cbat_json_array_get(const CBatJson *array, size_t index);

/** Appends `item` (taking ownership on success). */
CBAT_API CBatError cbat_json_array_append(CBatJson *array, CBatJson *item);

/** Removes and frees item `index`. */
CBAT_API CBatError cbat_json_array_remove(CBatJson *array, size_t index);

CBAT_API CBatError cbat_json_array_append_null(CBatJson *array);
CBAT_API CBatError cbat_json_array_append_bool(CBatJson *array, bool value);
CBAT_API CBatError cbat_json_array_append_number(CBatJson *array, double value);
CBAT_API CBatError cbat_json_array_append_int(CBatJson *array, int64_t value);
CBAT_API CBatError cbat_json_array_append_string(CBatJson *array, const char *value);

/* ---- Objects ------------------------------------------------------------ */

/** Member `key` of an object, or NULL. */
CBAT_API CBatJson *cbat_json_object_get(const CBatJson *object, const char *key);

/** Sets member `key` to `value` (taking ownership on success; replaces). */
CBAT_API CBatError cbat_json_object_set(CBatJson *object, const char *key, CBatJson *value);

/** Removes and frees member `key` (CBAT_ERR_NOT_FOUND if absent). */
CBAT_API CBatError cbat_json_object_remove(CBatJson *object, const char *key);

/** Iterates members in insertion order, starting with *iterator = 0. */
CBAT_API bool cbat_json_object_next(const CBatJson *object, size_t *iterator, const char **key,
                                    CBatJson **value);

CBAT_API CBatError cbat_json_object_set_null(CBatJson *object, const char *key);
CBAT_API CBatError cbat_json_object_set_bool(CBatJson *object, const char *key, bool value);
CBAT_API CBatError cbat_json_object_set_number(CBatJson *object, const char *key, double value);
CBAT_API CBatError cbat_json_object_set_int(CBatJson *object, const char *key, int64_t value);
CBAT_API CBatError cbat_json_object_set_string(CBatJson *object, const char *key,
                                               const char *value);

/* ---- Querying ----------------------------------------------------------- */

/**
 * Looks up a value by JSON Pointer (RFC 6901), e.g. "/users/0/name".
 * "" is the root. "~1" means '/' and "~0" means '~' inside a key.
 * Returns NULL if any step is missing.
 */
CBAT_API CBatJson *cbat_json_pointer(const CBatJson *root, const char *pointer);

/** Typed JSON Pointer lookups; missing/invalid pointer or wrong type returns fallback. */
CBAT_API int64_t cbat_json_pointer_int(const CBatJson *root, const char *pointer, int64_t fallback);
/** Same for numbers. */
CBAT_API double cbat_json_pointer_number(const CBatJson *root, const char *pointer, double fallback);
/** Borrowed string, or fallback. */
CBAT_API const char *cbat_json_pointer_string(const CBatJson *root, const char *pointer, const char *fallback);
/** Same for booleans. */
CBAT_API bool cbat_json_pointer_bool(const CBatJson *root, const char *pointer, bool fallback);

CBAT_END_DECLS

#endif /* CBAT_JSON_H */
