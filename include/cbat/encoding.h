/*
 * cbat/encoding.h - Base64 and hexadecimal encoding (cbat's answer to
 * Python's base64 and binascii.hexlify).
 *
 *     CBatString *b64 = NULL;
 *     cbat_base64_encode("hi!", 3, &b64);           // "aGkh"
 *
 * Decoders return the bytes in a CBatString (which may contain NULs).
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_ENCODING_H
#define CBAT_ENCODING_H

#include "cbat/base.h"
#include "cbat/string.h"

CBAT_BEGIN_DECLS

/** Standard Base64 (RFC 4648, '+' and '/', with '=' padding). */
CBAT_API CBatError cbat_base64_encode(const void *data, size_t length, CBatString **out);

/** URL-safe Base64 ('-' and '_', no padding). */
CBAT_API CBatError cbat_base64url_encode(const void *data, size_t length, CBatString **out);

/**
 * Decodes standard or URL-safe Base64. Padding is optional and ASCII
 * whitespace is ignored. CBAT_ERR_PARSE for any other character or a
 * malformed length.
 */
CBAT_API CBatError cbat_base64_decode(const char *text, size_t length, CBatString **out);

/** Lowercase hex, two digits per byte. */
CBAT_API CBatError cbat_hex_encode(const void *data, size_t length, CBatString **out);

/** Decodes hex (either case). CBAT_ERR_PARSE on odd length or bad digits. */
CBAT_API CBatError cbat_hex_decode(const char *text, size_t length, CBatString **out);

CBAT_END_DECLS

#endif /* CBAT_ENCODING_H */
