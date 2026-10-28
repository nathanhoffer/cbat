/*
 * cbat/string.h - CBatString, a growable byte string that is always
 * NUL-terminated, plus UTF-8 and number-parsing helpers
 * (cbat's answer to Python's str methods).
 *
 *     CBatString *greeting = cbat_string_from("Hello");
 *     cbat_string_append_format(greeting, ", %s! You are %d.", name, age);
 *     puts(cbat_string_cstr(greeting));
 *     cbat_string_free(greeting);
 *
 * Positions and lengths are in bytes. Case conversion and trimming are
 * ASCII-only; use the cbat_utf8_* helpers to walk code points.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_STRING_H
#define CBAT_STRING_H

#include "cbat/base.h"
#include "cbat/list.h"

CBAT_BEGIN_DECLS

/** Fields may be read freely; change them only through the functions below. */
typedef struct CBatString {
    char *data;       /* never NULL; data[length] == '\0' */
    size_t length;    /* bytes, not counting the terminator */
    size_t capacity;  /* bytes allocated, including the terminator */
    const CBatAllocator *allocator;
} CBatString;

/* ---- Creating and freeing ----------------------------------------------- */

/** New empty string. NULL only when out of memory. */
CBAT_API CBatString *cbat_string_new(void);
CBAT_API CBatString *cbat_string_new_with(const CBatAllocator *allocator);

/** Copy of a C string (NULL gives an empty string). */
CBAT_API CBatString *cbat_string_from(const char *text);

/** Copy of `length` bytes (may contain NULs). */
CBAT_API CBatString *cbat_string_from_length(const char *text, size_t length);

/** printf into a new string. */
CBAT_API CBatString *cbat_string_from_format(const char *format, ...) CBAT_PRINTF_FORMAT(1, 2);

CBAT_API CBatString *cbat_string_copy(const CBatString *s);

/** Frees the string. NULL is ignored. */
CBAT_API void cbat_string_free(CBatString *s);

/**
 * Frees the CBatString wrapper but returns its buffer, which the caller now
 * owns and must release with cbat_release(s->allocator, ...) - cbat_free()
 * when the string used the default allocator.
 */
CBAT_API char *cbat_string_steal(CBatString *s);

/** A CBatDestructor for list/dict slots holding `CBatString *`. */
CBAT_API void cbat_string_destructor(void *element);

/* ---- Reading ------------------------------------------------------------ */

/** The contents as a C string ("" for NULL). Valid until the next change. */
CBAT_API const char *cbat_string_cstr(const CBatString *s);
CBAT_API size_t cbat_string_length(const CBatString *s);
CBAT_API bool cbat_string_is_empty(const CBatString *s);

/* ---- Modifying ---------------------------------------------------------- */

CBAT_API void cbat_string_clear(CBatString *s);
CBAT_API CBatError cbat_string_reserve(CBatString *s, size_t additional);
/** Replaces the contents. */
CBAT_API CBatError cbat_string_set(CBatString *s, const char *text);

CBAT_API CBatError cbat_string_append(CBatString *s, const char *text);
CBAT_API CBatError cbat_string_append_length(CBatString *s, const char *text, size_t length);
CBAT_API CBatError cbat_string_append_char(CBatString *s, char c);
CBAT_API CBatError cbat_string_append_string(CBatString *s, const CBatString *other);
CBAT_API CBatError cbat_string_append_format(CBatString *s, const char *format, ...)
    CBAT_PRINTF_FORMAT(2, 3);
CBAT_API CBatError cbat_string_append_vformat(CBatString *s, const char *format, va_list args);
/** Appends the UTF-8 encoding of a code point (CBAT_ERR_INVALID_UTF8 if invalid). */
CBAT_API CBatError cbat_string_append_codepoint(CBatString *s, uint32_t codepoint);

/** Inserts `text` before byte `position` (position == length appends). */
CBAT_API CBatError cbat_string_insert(CBatString *s, size_t position, const char *text);

/** Removes up to `count` bytes starting at `position` (clamped to the end). */
CBAT_API CBatError cbat_string_erase(CBatString *s, size_t position, size_t count);

/** Shortens to `length` bytes (no-op if already shorter). */
CBAT_API void cbat_string_truncate(CBatString *s, size_t length);

/**
 * Replaces every occurrence of `from` with `to`. Stores the number of
 * replacements in *count when count is non-NULL. Empty `from` is invalid.
 */
CBAT_API CBatError cbat_string_replace(CBatString *s, const char *from, const char *to,
                                       size_t *count);

/** ASCII upper/lower case, in place. */
CBAT_API void cbat_string_to_upper(CBatString *s);
CBAT_API void cbat_string_to_lower(CBatString *s);

/** Remove ASCII whitespace from both ends / the start / the end. */
CBAT_API void cbat_string_trim(CBatString *s);
CBAT_API void cbat_string_trim_left(CBatString *s);
CBAT_API void cbat_string_trim_right(CBatString *s);

/* ---- Searching and comparing -------------------------------------------- */

/** Byte offset of the first `needle` at or after `start`, or CBAT_NPOS. */
CBAT_API size_t cbat_string_find(const CBatString *s, const char *needle, size_t start);

/** Byte offset of the last `needle`, or CBAT_NPOS. */
CBAT_API size_t cbat_string_rfind(const CBatString *s, const char *needle);

CBAT_API bool cbat_string_contains(const CBatString *s, const char *needle);

/** Number of non-overlapping occurrences (0 for an empty needle). */
CBAT_API size_t cbat_string_count(const CBatString *s, const char *needle);

CBAT_API bool cbat_string_starts_with(const CBatString *s, const char *prefix);
CBAT_API bool cbat_string_ends_with(const CBatString *s, const char *suffix);

/** Byte-wise equality with a C string. */
CBAT_API bool cbat_string_equals(const CBatString *s, const char *text);

/** Byte-wise ordering, like strcmp but length-aware. */
CBAT_API int cbat_string_compare(const CBatString *a, const CBatString *b);

/** A CBatCompare for slots holding `CBatString *`. */
CBAT_API int cbat_string_compare_elements(const void *a, const void *b);

/* ---- Producing new strings ---------------------------------------------- */

/** New string of `count` bytes from `start`, clamped like a Python slice. */
CBAT_API CBatString *cbat_string_substring(const CBatString *s, size_t start, size_t count);

/** `text` repeated `times` times. */
CBAT_API CBatString *cbat_string_repeat(const char *text, size_t times);

/**
 * Splits `text` on every `separator` (non-empty) like Python's
 * str.split(sep): "a,,b" gives ["a", "", "b"]. *out receives a new list of
 * `CBatString *` that owns its strings: free it with cbat_list_free().
 */
CBAT_API CBatError cbat_string_split(const char *text, const char *separator, CBatList **out);

/** Splits on runs of ASCII whitespace, dropping empty pieces (str.split()). */
CBAT_API CBatError cbat_string_split_whitespace(const char *text, CBatList **out);

/** Joins a list of `CBatString *` with `separator` between items. */
CBAT_API CBatError cbat_string_join(const CBatList *strings, const char *separator,
                                    CBatString **out);

/* ---- Parsing numbers ---------------------------------------------------- */

/**
 * Parses a whole string as a base-10 integer, allowing surrounding ASCII
 * whitespace and a sign. CBAT_ERR_PARSE for anything else,
 * CBAT_ERR_OVERFLOW when it does not fit.
 */
CBAT_API CBatError cbat_string_to_int64(const char *text, int64_t *out);

/** Same, in `base` 2..36 (no prefixes such as 0x). */
CBAT_API CBatError cbat_string_to_int64_base(const char *text, int base, int64_t *out);

/** Parses a whole string as a double ("1.5", "-2e10", "inf", "nan"). */
CBAT_API CBatError cbat_string_to_double(const char *text, double *out);

/* ---- UTF-8 -------------------------------------------------------------- */

/**
 * Decodes one code point from the start of `text`. Returns the number of
 * bytes used (1-4), or 0 for invalid, overlong, surrogate or truncated input.
 */
CBAT_API size_t cbat_utf8_decode(const char *text, size_t length, uint32_t *codepoint);

/** Encodes a code point; returns bytes written (1-4) or 0 if not encodable. */
CBAT_API size_t cbat_utf8_encode(uint32_t codepoint, char out[4]);

CBAT_API bool cbat_utf8_is_valid(const char *text, size_t length);

/** Number of code points (CBAT_ERR_INVALID_UTF8 on bad input). */
CBAT_API CBatError cbat_utf8_count(const char *text, size_t length, size_t *count);

CBAT_END_DECLS

#endif /* CBAT_STRING_H */
