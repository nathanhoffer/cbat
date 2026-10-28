/*
 * cbat/ini.h - insertion-ordered INI settings (case-sensitive sections/keys).
 * Supports key=value / key:value, full-line comments, and inline comments
 * beginning with ';' or '#' after whitespace (or after a closing quote).
 * Single/double-quoted values
 * preserve whitespace; only matching quotes and backslashes are unescaped.
 * No interpolation, inheritance or multiline values. Part of cbat. MIT License.
 */
#ifndef CBAT_INI_H
#define CBAT_INI_H
#include "cbat/base.h"
#include "cbat/string.h"
CBAT_BEGIN_DECLS
typedef struct CBatIni CBatIni;

/** New empty settings object; NULL on OOM. */
CBAT_API CBatIni *cbat_ini_new(void);
/** Same with retained allocator (NULL = default); allocator must outlive results. */
CBAT_API CBatIni *cbat_ini_new_with(const CBatAllocator *allocator);
/**
 * Parses length bytes (NULL allowed for length 0), optional UTF-8 BOM.
 * Embedded NUL is invalid everywhere; other controls except tab are invalid
 * in section names, keys and values.
 * Duplicate sections merge and duplicate keys replace, retaining order.
 * Empty named sections are retained. Output unchanged on failure; info may be NULL.
 */
CBAT_API CBatError cbat_ini_parse(const char *text, size_t length, CBatIni **out, CBatParseInfo *info);
/** Reads and parses a whole file; output unchanged on failure. */
CBAT_API CBatError cbat_ini_read_file(const char *path, CBatIni **out, CBatParseInfo *info);
/** Frees sections, keys, values and wrapper. NULL is ignored. */
CBAT_API void cbat_ini_free(CBatIni *ini);
/** Borrowed value until mutation/free; NULL for missing/invalid arguments. "" section is global. */
CBAT_API const char *cbat_ini_get(const CBatIni *ini, const char *section, const char *key);
/** Typed lookup: NOT_FOUND if absent; numeric parser errors propagate; output unchanged on error. */
CBAT_API CBatError cbat_ini_get_int(const CBatIni *ini, const char *section, const char *key, int64_t *out);
/** Same for ASCII decimal doubles, independent of numeric locale. */
CBAT_API CBatError cbat_ini_get_double(const CBatIni *ini, const char *section, const char *key, double *out);
/** Accepts true/false, yes/no, on/off, 1/0 case-insensitively, ignoring surrounding space/tab. */
CBAT_API CBatError cbat_ini_get_bool(const CBatIni *ini, const char *section, const char *key, bool *out);
/**
 * Copies key/value into a section (created as needed). Names must not have
 * edge space/tab, line breaks or controls. Section names exclude brackets;
 * keys are nonempty and exclude '=', ':', '[' and ']' (internal spaces allowed),
 * and cannot start with '#' or ';'. Values exclude line breaks/controls
 * except tabs. Invalid input gives INVALID_ARGUMENT. Atomic on OOM.
 */
CBAT_API CBatError cbat_ini_set(CBatIni *ini, const char *section, const char *key, const char *value);
/**
 * New owned string using the config allocator. Writes global keys first,
 * then named sections in insertion order; keys keep their order. Values are
 * always quoted with backslash/quote escaping. Output unchanged on error.
 */
CBAT_API CBatError cbat_ini_to_string(const CBatIni *ini, CBatString **out);
/** Borrowed section names in insertion order, including "" if it has global keys. Start iterator at 0. */
CBAT_API bool cbat_ini_next_section(const CBatIni *ini, size_t *iterator, const char **name);
/** Borrowed keys/values in section order. Start iterator at 0; key/value outputs may be NULL. */
CBAT_API bool cbat_ini_next_key(const CBatIni *ini, const char *section, size_t *iterator, const char **key, const char **value);
CBAT_END_DECLS
#endif /* CBAT_INI_H */
