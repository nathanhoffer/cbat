/*
 * cbat/hash.h - checksums and hashes: CRC-32, FNV-1a and SHA-256
 * (cbat's answer to Python's zlib.crc32 and hashlib.sha256).
 *
 *     char hex[CBAT_SHA256_HEX_SIZE];
 *     cbat_sha256_hex("abc", 3, hex);   // "ba7816bf..."
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_HASH_H
#define CBAT_HASH_H

#include "cbat/base.h"

CBAT_BEGIN_DECLS

/* ---- CRC-32 (IEEE 802.3, same as zlib/PNG/zip) -------------------------- */

/** CRC-32 of a buffer. */
CBAT_API uint32_t cbat_crc32(const void *data, size_t length);

/** Continues a CRC: crc = cbat_crc32_update(crc, more, n). Start with 0. */
CBAT_API uint32_t cbat_crc32_update(uint32_t crc, const void *data, size_t length);

/* ---- FNV-1a (fast, non-cryptographic; good for hash tables) ------------- */

CBAT_API uint32_t cbat_fnv1a32(const void *data, size_t length);
CBAT_API uint64_t cbat_fnv1a64(const void *data, size_t length);

/** FNV-1a 64 of a NUL-terminated string. */
CBAT_API uint64_t cbat_hash_cstring(const char *s);

/* ---- SHA-256 ------------------------------------------------------------ */

#define CBAT_SHA256_SIZE 32
#define CBAT_SHA256_HEX_SIZE 65 /* 64 hex digits + NUL */

/** Incremental SHA-256 state. Treat the fields as private. */
typedef struct CBatSha256 {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char buffer[64];
    size_t buffer_length;
} CBatSha256;

CBAT_API void cbat_sha256_init(CBatSha256 *ctx);
CBAT_API void cbat_sha256_update(CBatSha256 *ctx, const void *data, size_t length);
/** Writes the 32-byte digest. The context must be re-initialized to reuse it. */
CBAT_API void cbat_sha256_final(CBatSha256 *ctx, unsigned char digest[CBAT_SHA256_SIZE]);

/** One-shot digest of a buffer. */
CBAT_API void cbat_sha256(const void *data, size_t length, unsigned char digest[CBAT_SHA256_SIZE]);

/** One-shot lowercase hex digest, NUL-terminated. */
CBAT_API void cbat_sha256_hex(const void *data, size_t length, char hex[CBAT_SHA256_HEX_SIZE]);

/** HMAC-SHA256 (RFC 4231), 32-byte output. NULL input means empty; NULL output ignored. */
CBAT_API void cbat_hmac_sha256(const void *key, size_t key_length, const void *data, size_t length, uint8_t out[32]);
/** SHA-1, 20 bytes. Legacy checksums only: unsuitable for security. NULL input means empty; NULL output ignored. */
CBAT_API void cbat_sha1(const void *data, size_t length, uint8_t out[20]);
/** MD5, 16 bytes. Legacy checksums only: unsuitable for security. NULL input means empty; NULL output ignored. */
CBAT_API void cbat_md5(const void *data, size_t length, uint8_t out[16]);

CBAT_END_DECLS

#endif /* CBAT_HASH_H */
