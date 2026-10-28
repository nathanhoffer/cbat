/*
 * cbat/hash.c - CRC-32, FNV-1a and SHA-256 (FIPS 180-4).
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/hash.h"

#include <string.h>

/* ---- CRC-32 ------------------------------------------------------------- */

/* Bitwise-by-nibble table: small (16 entries) and needs no initialization. */
static const uint32_t cbat__crc32_nibble[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
};

uint32_t cbat_crc32_update(uint32_t crc, const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    if (data == NULL) {
        return crc;
    }
    crc = ~crc;
    for (i = 0; i < length; i++) {
        crc ^= bytes[i];
        crc = (crc >> 4) ^ cbat__crc32_nibble[crc & 0x0F];
        crc = (crc >> 4) ^ cbat__crc32_nibble[crc & 0x0F];
    }
    return ~crc;
}

uint32_t cbat_crc32(const void *data, size_t length) {
    return cbat_crc32_update(0, data, length);
}

/* ---- FNV-1a ------------------------------------------------------------- */

uint32_t cbat_fnv1a32(const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint32_t hash = 0x811C9DC5u;
    size_t i;
    for (i = 0; data != NULL && i < length; i++) {
        hash ^= bytes[i];
        hash *= 0x01000193u;
    }
    return hash;
}

uint64_t cbat_fnv1a64(const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint64_t hash = 0xCBF29CE484222325ull;
    size_t i;
    for (i = 0; data != NULL && i < length; i++) {
        hash ^= bytes[i];
        hash *= 0x00000100000001B3ull;
    }
    return hash;
}

uint64_t cbat_hash_cstring(const char *s) {
    return s == NULL ? cbat_fnv1a64(NULL, 0) : cbat_fnv1a64(s, strlen(s));
}

/* ---- SHA-256 ------------------------------------------------------------ */

static const uint32_t cbat__sha256_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

#define CBAT__ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void cbat__sha256_block(CBatSha256 *ctx, const unsigned char *block) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = CBAT__ROTR32(w[i - 15], 7) ^ CBAT__ROTR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = CBAT__ROTR32(w[i - 2], 17) ^ CBAT__ROTR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; i++) {
        uint32_t s1 = CBAT__ROTR32(e, 6) ^ CBAT__ROTR32(e, 11) ^ CBAT__ROTR32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + cbat__sha256_k[i] + w[i];
        uint32_t s0 = CBAT__ROTR32(a, 2) ^ CBAT__ROTR32(a, 13) ^ CBAT__ROTR32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void cbat_sha256_init(CBatSha256 *ctx) {
    static const uint32_t initial[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    if (ctx == NULL) {
        return;
    }
    memcpy(ctx->state, initial, sizeof(initial));
    ctx->bit_count = 0;
    ctx->buffer_length = 0;
}

void cbat_sha256_update(CBatSha256 *ctx, const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    if (ctx == NULL || data == NULL) {
        return;
    }
    ctx->bit_count += (uint64_t)length * 8u;
    while (length > 0) {
        size_t take = CBAT_MIN(length, 64 - ctx->buffer_length);
        memcpy(ctx->buffer + ctx->buffer_length, bytes, take);
        ctx->buffer_length += take;
        bytes += take;
        length -= take;
        if (ctx->buffer_length == 64) {
            cbat__sha256_block(ctx, ctx->buffer);
            ctx->buffer_length = 0;
        }
    }
}

void cbat_sha256_final(CBatSha256 *ctx, unsigned char digest[CBAT_SHA256_SIZE]) {
    uint64_t bits;
    int i;
    if (ctx == NULL || digest == NULL) {
        return;
    }
    bits = ctx->bit_count;
    ctx->buffer[ctx->buffer_length++] = 0x80;
    if (ctx->buffer_length > 56) {
        memset(ctx->buffer + ctx->buffer_length, 0, 64 - ctx->buffer_length);
        cbat__sha256_block(ctx, ctx->buffer);
        ctx->buffer_length = 0;
    }
    memset(ctx->buffer + ctx->buffer_length, 0, 56 - ctx->buffer_length);
    for (i = 0; i < 8; i++) {
        ctx->buffer[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
    }
    cbat__sha256_block(ctx, ctx->buffer);
    for (i = 0; i < 8; i++) {
        digest[i * 4] = (unsigned char)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)ctx->state[i];
    }
}

void cbat_sha256(const void *data, size_t length, unsigned char digest[CBAT_SHA256_SIZE]) {
    CBatSha256 ctx;
    cbat_sha256_init(&ctx);
    cbat_sha256_update(&ctx, data, length);
    cbat_sha256_final(&ctx, digest);
}

void cbat_sha256_hex(const void *data, size_t length, char hex[CBAT_SHA256_HEX_SIZE]) {
    static const char digits[] = "0123456789abcdef";
    unsigned char digest[CBAT_SHA256_SIZE];
    int i;
    if (hex == NULL) {
        return;
    }
    cbat_sha256(data, length, digest);
    for (i = 0; i < CBAT_SHA256_SIZE; i++) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 0x0F];
    }
    hex[CBAT_SHA256_HEX_SIZE - 1] = '\0';
}
