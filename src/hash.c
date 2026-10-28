/*
 * cbat/hash.c - CRC-32, FNV-1a and SHA-256 (FIPS 180-4).
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/hash.h"

#include <string.h>

void cbat_hmac_sha256(const void *key, size_t key_length, const void *data, size_t length, uint8_t out[32]) {
    unsigned char pad[64] = {0}, inner[32];
    CBatSha256 ctx;
    size_t i;
    if (out == NULL) return;
    if (key == NULL) key_length = 0;
    if (key_length > sizeof(pad)) cbat_sha256(key, key_length, pad);
    else if (key_length != 0) memcpy(pad, key, key_length);
    for (i = 0; i < sizeof(pad); i++) pad[i] ^= 0x36;
    cbat_sha256_init(&ctx); cbat_sha256_update(&ctx, pad, sizeof(pad));
    cbat_sha256_update(&ctx, data, length); cbat_sha256_final(&ctx, inner);
    for (i = 0; i < sizeof(pad); i++) pad[i] ^= 0x36 ^ 0x5c;
    cbat_sha256_init(&ctx); cbat_sha256_update(&ctx, pad, sizeof(pad));
    cbat_sha256_update(&ctx, inner, sizeof(inner)); cbat_sha256_final(&ctx, out);
}

static uint32_t cbat__hash_left(uint32_t x, unsigned n) { return (x << n) | (x >> (32 - n)); }

/* SHA-1 compression, FIPS 180 / RFC 3174. */
static void cbat__sha1_block(uint32_t *state, const unsigned char *block) {
    uint32_t w[80], a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    unsigned i;
    for (i = 0; i < 16; i++) w[i] = (uint32_t)block[4*i] << 24 | (uint32_t)block[4*i+1] << 16 |
        (uint32_t)block[4*i+2] << 8 | block[4*i+3];
    for (; i < 80; i++) w[i] = cbat__hash_left(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    for (i = 0; i < 80; i++) {
        uint32_t f, k, temp;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999u; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcu; }
        else { f = b ^ c ^ d; k = 0xca62c1d6u; }
        temp = cbat__hash_left(a, 5) + f + e + k + w[i];
        e = d; d = c; c = cbat__hash_left(b, 30); b = a; a = temp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

/* MD5 compression, RFC 1321; constants floor(2^32 * abs(sin(i+1))). */
static void cbat__md5_block(uint32_t *state, const unsigned char *block) {
    static const uint32_t k[64] = {
        0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,
        0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,
        0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,0xd62f105du,0x02441453u,0xd8a1e681u,0xe7d3fbc8u,
        0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,
        0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,
        0x289b7ec6u,0xeaa127fau,0xd4ef3085u,0x04881d05u,0xd9d4d039u,0xe6db99e5u,0x1fa27cf8u,0xc4ac5665u,
        0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,0x655b59c3u,0x8f0ccc92u,0xffeff47du,0x85845dd1u,
        0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,0xf7537e82u,0xbd3af235u,0x2ad7d2bbu,0xeb86d391u
    };
    static const unsigned shifts[16] = {7,12,17,22, 5,9,14,20, 4,11,16,23, 6,10,15,21};
    uint32_t w[16], a = state[0], b = state[1], c = state[2], d = state[3];
    unsigned i;
    for (i = 0; i < 16; i++) w[i] = (uint32_t)block[4*i] | (uint32_t)block[4*i+1] << 8 |
        (uint32_t)block[4*i+2] << 16 | (uint32_t)block[4*i+3] << 24;
    for (i = 0; i < 64; i++) {
        uint32_t f, old = d;
        unsigned g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5*i + 1) % 16; }
        else if (i < 48) { f = b ^ c ^ d; g = (3*i + 5) % 16; }
        else { f = c ^ (b | ~d); g = (7*i) % 16; }
        d = c; c = b; b += cbat__hash_left(a + f + k[i] + w[g], shifts[(i/16)*4 + i%4]); a = old;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static void cbat__legacy_digest(const void *data, size_t length, unsigned char *out, bool sha1) {
    uint32_t state[5] = {0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u,0xc3d2e1f0u};
    unsigned char tail[128] = {0};
    const unsigned char *bytes = (const unsigned char *)data;
    size_t full, rest, padded, i;
    uint64_t bits;
    if (out == NULL) return;
    if (data == NULL) length = 0;
    full = length / 64; rest = length % 64; bits = (uint64_t)length * 8;
    for (i = 0; i < full; i++) {
        if (sha1) cbat__sha1_block(state, bytes + i*64);
        else cbat__md5_block(state, bytes + i*64);
    }
    if (rest != 0) memcpy(tail, bytes + full*64, rest);
    tail[rest] = 0x80; padded = rest < 56 ? 64 : 128;
    for (i = 0; i < 8; i++) tail[padded - 8 + i] = (unsigned char)(bits >> (sha1 ? 56 - 8*i : 8*i));
    for (i = 0; i < padded; i += 64) {
        if (sha1) cbat__sha1_block(state, tail + i); else cbat__md5_block(state, tail + i);
    }
    for (i = 0; i < (sha1 ? 20u : 16u); i++) out[i] = (unsigned char)(state[i/4] >> (sha1 ? 24 - 8*(i%4) : 8*(i%4)));
}

void cbat_sha1(const void *data, size_t length, uint8_t out[20]) { cbat__legacy_digest(data, length, out, true); }
void cbat_md5(const void *data, size_t length, uint8_t out[16]) { cbat__legacy_digest(data, length, out, false); }

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
