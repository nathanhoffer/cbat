/* cbat/regex.h - ordered Pike VM regular expressions. MIT License.
 * Byte-oriented: spans are byte offsets, dot consumes one byte, classes and
 * case folding use ASCII. UTF-8 literals match their encoded bytes.
 * Search is O(program size * text length), without recursive backtracking.
 * Supports captures, alternatives, greedy/lazy repetition, classes, ^ $ and
 * \b. No lookaround, backreferences or locale-sensitive classes.
 * At a zero-width repetition cycle, the first instruction visit wins;
 * captures can retain the preceding consuming iteration instead of a final
 * empty iteration. This avoids cycling and differs from backtracking engines.
 * Compilation limits: 128 syntax levels, 1000 per repeat bound, 10000 nodes
 * and instructions. Excess gives OUT_OF_RANGE. Matching uses per-call scratch
 * memory; compiled patterns are immutable and may be shared between threads.
 * Optional tier 2: include explicitly; CMake CBAT_WITH_REGEX (default ON).
 */
#ifndef CBAT_REGEX_H
#define CBAT_REGEX_H
#include "cbat/base.h"
#include "cbat/list.h"
#include "cbat/string.h"
CBAT_BEGIN_DECLS
typedef struct CBatRegex CBatRegex;
enum { CBAT_REGEX_IGNORE_CASE = 1, CBAT_REGEX_MULTILINE = 2, CBAT_REGEX_DOT_ALL = 4 };
#define CBAT_REGEX_MAX_GROUPS 16
/** Half-open byte span, both CBAT_NPOS when unmatched. */
typedef struct CBatSpan { size_t start; size_t end; } CBatSpan;
/** Group 0 is the whole match; group_count includes it (at most 15 explicit captures). */
typedef struct CBatRegexMatch { CBatSpan groups[CBAT_REGEX_MAX_GROUPS]; size_t group_count; } CBatRegexMatch;
/** Compiles a NUL-terminated pattern; info optional. Output unchanged on failure. */
CBAT_API CBatError cbat_regex_compile(const char *pattern, int flags, CBatRegex **out, CBatParseInfo *info);
/** Compile with retained allocator (NULL = default); allocator must outlive pattern/results. */
CBAT_API CBatError cbat_regex_compile_with(const char *pattern, int flags, const CBatAllocator *allocator,
    CBatRegex **out, CBatParseInfo *info);
/** Frees compiled pattern; NULL ignored. */
CBAT_API void cbat_regex_free(CBatRegex *regex);
/** Whole-string match. False for invalid input or allocation failure; use search for fallible lookup. */
CBAT_API bool cbat_regex_is_match(const CBatRegex *regex, const char *text);
/** Leftmost-first search at/after byte start. NOT_FOUND on no match; output unchanged on error. */
CBAT_API CBatError cbat_regex_search(const CBatRegex *regex, const char *text, size_t start, CBatRegexMatch *out);
/** Owned list of nonoverlapping matches. Empty matches advance one byte; includes final empty match. */
CBAT_API CBatError cbat_regex_find_all(const CBatRegex *regex, const char *text, CBatList **out);
/** Owned string with nonoverlapping replacements; \0..\9 expand captures, \\ escapes backslash.
 * Unmatched captures expand empty; unknown/dangling escapes are INVALID_ARGUMENT. */
CBAT_API CBatError cbat_regex_replace(const CBatRegex *regex, const char *text, const char *replacement, CBatString **out);
/** Owned list of owned strings between nonoverlapping matches; preserves empty pieces, omits captures. */
CBAT_API CBatError cbat_regex_split(const CBatRegex *regex, const char *text, CBatList **out);
/** Number of explicit capture groups (excludes group 0); 0 for NULL. */
CBAT_API size_t cbat_regex_group_count(const CBatRegex *regex);
CBAT_END_DECLS
#endif
