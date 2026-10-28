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

/** Byte-width padding; unchanged when width <= length. Atomic on OOM/overflow. */
CBAT_API CBatError cbat_string_pad_left(CBatString *s, size_t width, char fill);
/** Same, adding bytes on the right. fill may be NUL (length remains explicit). */
CBAT_API CBatError cbat_string_pad_right(CBatString *s, size_t width, char fill);
/** Centers by byte width; odd extra padding goes on the right. Atomic on error. */
CBAT_API CBatError cbat_string_center(CBatString *s, size_t width, char fill);
/** Removes bytes in the C string characters from both ends; NULL arguments are ignored. */
CBAT_API void cbat_string_strip_chars(CBatString *s, const char *characters);
/** Removes a matching prefix (otherwise no-op); NULL arguments are invalid. */
CBAT_API CBatError cbat_string_remove_prefix(CBatString *s, const char *prefix);
/** Removes a matching suffix (otherwise no-op); NULL arguments are invalid. */
CBAT_API CBatError cbat_string_remove_suffix(CBatString *s, const char *suffix);

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

/** ASCII case-insensitive, length-aware equality; NULL arguments give false. */
CBAT_API bool cbat_string_equals_ignore_case(const CBatString *s, const char *text);
/** ASCII case-insensitive prefix check; NULL arguments give false. */
CBAT_API bool cbat_string_starts_with_ignore_case(const CBatString *s, const char *prefix);
/** Nonempty C string of ASCII digits only (false for NULL). */
CBAT_API bool cbat_string_is_ascii_digits(const char *text);
/**
 * Natural ordering: file9 < file10. Digit runs compare by magnitude, without
 * integer overflow; numeric ties prefer shorter digit runs. Other bytes
 * compare unsigned, case-sensitively. NULL sorts before non-NULL.
 */
CBAT_API int cbat_compare_natural(const char *a, const char *b);
/** CBatCompare for element slots holding const char * (NULL slots sort first). */
CBAT_API int cbat_compare_natural_elements(const void *a, const void *b);

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

/** Same with at most max_splits; 0 returns the entire text as one field. */
CBAT_API CBatError cbat_string_split_max(const char *text, const char *separator, size_t max_splits, CBatList **out);
/** Splits on LF, CRLF or CR, excluding a trailing empty line; empty input gives no lines. */
CBAT_API CBatError cbat_string_split_lines(const char *text, CBatList **out);
/**
 * Splits at the first nonempty separator, returning two owned strings.
 * NOT_FOUND leaves outputs unchanged. NULL arguments/aliased output slots
 * give INVALID_ARGUMENT. Free each result with cbat_string_free.
 */
CBAT_API CBatError cbat_string_partition(const char *text, const char *separator, CBatString **before, CBatString **after);

/**
 * Splits on runs of ASCII whitespace: space, tab (\t), newline (\n), carriage
 * return (\r), vertical tab (\v), and form feed (\f). Leading/trailing and
 * repeated whitespace produce no empty fields. "  the\tcat  " gives
 * ["the", "cat"]. Does not treat non-ASCII spaces as separators.
 * out receives an owned list of owned CBatString pointers; free the list.
 */
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

/**
 * Parses an ASCII decimal double ("1.5", "-2e10", "inf", "nan"), regardless
 * of LC_NUMERIC. Surrounding ASCII whitespace/sign allowed; commas, hex and
 * trailing junk give ERR_PARSE. Overflow gives ERR_OVERFLOW; underflow rounds
 * to a subnormal or signed zero. Output unchanged on failure (including OOM).
 * Do not change the process locale concurrently with this call.
 */
CBAT_API CBatError cbat_string_to_double(const char *text, double *out);

/** Decimal uint64, optional '+', ASCII whitespace; rejects '-'. Output unchanged on error. */
CBAT_API CBatError cbat_string_to_uint64(const char *text, uint64_t *out);

/**
 * Shortest round-tripping decimal for binary64, choosing fixed/scientific
 * notation by length (fixed on a tie). Uses '.', no redundant exponent zeros
 * or '+'. Preserves -0; non-finite values are "inf", "-inf", "nan".
 * Writes a NUL terminator and returns length (0 for NULL output).
 */
CBAT_API size_t cbat_format_double(double value, char out[32]);
/** Appends cbat_format_double's locale-independent result; unchanged on OOM. */
CBAT_API CBatError cbat_string_append_double(CBatString *s, double value);
/** Decimal int64 including INT64_MIN. Writes NUL; length, or 0 for NULL output. */
CBAT_API size_t cbat_format_int64(int64_t value, char out[21]);
/** Decimal int64 with groups of three (separator 0 disables grouping). Atomic on OOM. */
CBAT_API CBatError cbat_string_append_int64_grouped(CBatString *s, int64_t value, char separator);

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

/** Reverse UTF-8 code points (not grapheme clusters) into a new owned string.
 * Invalid UTF-8 gives ERR_INVALID_UTF8; outputs unchanged on error. */
CBAT_API CBatError cbat_utf8_reverse(const char *text, CBatString **out);
/** Code-point slice, start/count clamped to length. Validates the entire C string.
 * New owned string; invalid UTF-8/errors leave output unchanged. */
CBAT_API CBatError cbat_utf8_substring(const char *text, size_t start, size_t count, CBatString **out);

CBAT_END_DECLS

#endif /* CBAT_STRING_H */
