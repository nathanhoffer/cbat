/*
 * cbat/url.h - percent encoding, URI-reference components and form queries.
 * Generic syntax follows RFC 3986: https://www.rfc-editor.org/rfc/rfc3986
 * No networking, IDNA conversion or relative-reference resolution is done.
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_URL_H
#define CBAT_URL_H
#include "cbat/base.h"
#include "cbat/string.h"
#include "cbat/dict.h"
CBAT_BEGIN_DECLS

/** Percent-encodes C-string bytes, keeping only ASCII unreserved characters; uppercase hex. */
CBAT_API CBatError cbat_url_encode(const char *text, CBatString **out);
/** Same with spaces encoded as '+'. Literal '+' is encoded as %2B. */
CBAT_API CBatError cbat_url_encode_form(const char *text, CBatString **out);
/**
 * Strict percent decoding (PARSE on bad/truncated escapes). '+' stays '+'.
 * Result is an owned byte string and may contain NULs/non-UTF-8 bytes.
 * NULL arguments are invalid. All codec outputs stay unchanged on failure.
 */
CBAT_API CBatError cbat_url_decode(const char *text, CBatString **out);

/** Owned components; missing fields are empty strings. Free with cbat_url_free. */
typedef struct CBatUrl {
    CBatString *scheme;
    CBatString *user;    /* raw userinfo, possibly including ':password' */
    CBatString *host;    /* raw host, without enclosing IPv6/IPvFuture brackets */
    CBatString *path;
    CBatString *query;   /* excludes '?' */
    CBatString *fragment; /* excludes '#' */
    int port;           /* -1 when absent/empty, otherwise 0..65535 */
} CBatUrl;

/**
 * Parses an absolute or relative ASCII URI reference. Preserves component
 * case, percent escapes and dot segments. Validates scheme, generic component
 * syntax, bracketed IPv6/IPvFuture and numeric port. Zone identifiers/IRIs
 * are not supported; encode non-ASCII bytes before parsing. Does not validate
 * scheme-specific rules (for example, whether an HTTP host is empty).
 * PARSE fills optional info; outputs unchanged on parse/OOM errors.
 */
CBAT_API CBatError cbat_url_parse(const char *text, CBatUrl **out, CBatParseInfo *info);
/** Frees all components and wrapper; NULL is ignored. */
CBAT_API void cbat_url_free(CBatUrl *url);
/**
 * Form query without leading '?': '&'-separated key[=value], '+' = space.
 * Empty segments ignored; empty keys/values allowed. Repeated keys: last
 * value wins, first insertion position retained. NUL in decoded keys is
 * PARSE; values can contain binary bytes. Owned dict of owned CBatString *.
 */
CBAT_API CBatError cbat_url_parse_query(const char *query, CBatDict **out);
/**
 * Form-encodes a dict of CBatString * in insertion order. NULL values mean
 * empty strings; binary value bytes are percent-encoded. TYPE for wrong
 * value size. New owned string; output unchanged on error.
 */
CBAT_API CBatError cbat_url_build_query(const CBatDict *params, CBatString **out);
CBAT_END_DECLS
#endif /* CBAT_URL_H */
