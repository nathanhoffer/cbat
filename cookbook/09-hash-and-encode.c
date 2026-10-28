/*
 * Recipe 9: Checksums, hashes and Base64
 *
 * PROBLEM
 *   Verify a download, fingerprint some data, or put binary bytes into a
 *   text format such as JSON or a URL.
 *
 * SOLUTION
 *   cbat_sha256_hex() for a fingerprint, cbat_crc32() for a cheap
 *   corruption check, cbat_base64_encode()/cbat_base64url_encode() for
 *   text-safe bytes, and incremental CBatSha256 for streams.
 *
 * DISCUSSION
 *   CRC-32 detects accidents, not attackers; use SHA-256 when it matters.
 *   The URL-safe Base64 alphabet avoids '+' and '/', and drops padding,
 *   which is what JWTs and most web APIs expect.
 */
#include <stdio.h>
#include <string.h>

#include "cbat/cbat.h"

int main(void) {
    const char *message = "C Batteries Included";
    const unsigned char binary[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
    char hex[CBAT_SHA256_HEX_SIZE];
    CBatSha256 stream;
    unsigned char digest[CBAT_SHA256_SIZE];
    CBatString *b64 = NULL;
    CBatString *url = NULL;
    CBatString *back = NULL;
    CBatString *digest_hex = NULL;

    cbat_sha256_hex(message, strlen(message), hex);
    printf("sha256(\"%s\") = %s\n", message, hex);
    printf("crc32 = %08lx\n", (unsigned long)cbat_crc32(message, strlen(message)));

    /* Hashing in pieces gives the same digest. */
    cbat_sha256_init(&stream);
    cbat_sha256_update(&stream, "C Batteries ", 12);
    cbat_sha256_update(&stream, "Included", 8);
    cbat_sha256_final(&stream, digest);
    cbat_hex_encode(digest, sizeof(digest), &digest_hex);
    printf("streamed matches: %s\n", cbat_string_equals(digest_hex, hex) ? "yes" : "no");

    cbat_base64_encode(binary, sizeof(binary), &b64);
    cbat_base64url_encode(binary, sizeof(binary), &url);
    cbat_base64_decode(b64->data, b64->length, &back);
    printf("base64     = %s\n", cbat_string_cstr(b64));
    printf("base64url  = %s\n", cbat_string_cstr(url));
    printf("round trip = %s\n",
           back->length == sizeof(binary) && memcmp(back->data, binary, sizeof(binary)) == 0
               ? "ok" : "MISMATCH");

    cbat_string_free(b64);
    cbat_string_free(url);
    cbat_string_free(back);
    cbat_string_free(digest_hex);
    return 0;
}
