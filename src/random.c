/*
 * cbat/random.c - PCG32 (M.E. O'Neill, pcg-random.org, Apache-2.0/MIT
 * reference algorithm reimplemented here).
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/random.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include "cbat/sort.h"

#define CBAT__PCG_MULTIPLIER 6364136223846793005ull
#define CBAT__PCG_DEFAULT_STREAM 0xda3e39cb94b95bdbull

static CBatRandom cbat__global_rng;
static bool cbat__global_seeded = false;

static CBatRandom *cbat__rng(CBatRandom *rng) {
    if (rng != NULL) {
        return rng;
    }
    if (!cbat__global_seeded) {
        cbat_random_seed_from_time(&cbat__global_rng);
        cbat__global_seeded = true;
    }
    return &cbat__global_rng;
}

static uint32_t cbat__pcg_next(CBatRandom *rng) {
    uint64_t old = rng->state;
    uint32_t xorshifted;
    uint32_t rotation;
    rng->state = old * CBAT__PCG_MULTIPLIER + rng->increment;
    xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    rotation = (uint32_t)(old >> 59u);
    return (xorshifted >> rotation) | (xorshifted << ((0u - rotation) & 31u));
}

void cbat_random_seed_stream(CBatRandom *rng, uint64_t seed, uint64_t stream) {
    rng = cbat__rng(rng);
    rng->state = 0u;
    rng->increment = (stream << 1u) | 1u;
    cbat__pcg_next(rng);
    rng->state += seed;
    cbat__pcg_next(rng);
}

void cbat_random_seed(CBatRandom *rng, uint64_t seed) {
    cbat_random_seed_stream(rng, seed, CBAT__PCG_DEFAULT_STREAM);
}

void cbat_random_seed_from_time(CBatRandom *rng) {
    static unsigned long calls = 0;
    uint64_t seed = (uint64_t)time(NULL);
    int local;
    /* Mix in sub-second clock ticks and stack/heap-ish addresses (ASLR). */
    seed ^= (uint64_t)clock() << 32;
    seed ^= (uint64_t)(uintptr_t)&local;
    seed ^= (uint64_t)(uintptr_t)rng * 0x9E3779B97F4A7C15ull;
    seed ^= (uint64_t)++calls * 0xBF58476D1CE4E5B9ull;
    if (rng == NULL) {
        rng = &cbat__global_rng;
        cbat__global_seeded = true;
    }
    cbat_random_seed_stream(rng, seed, (uint64_t)(uintptr_t)rng ^ CBAT__PCG_DEFAULT_STREAM);
}

uint32_t cbat_random_u32(CBatRandom *rng) {
    return cbat__pcg_next(cbat__rng(rng));
}

uint64_t cbat_random_u64(CBatRandom *rng) {
    uint64_t high;
    rng = cbat__rng(rng);
    high = cbat__pcg_next(rng);
    return (high << 32) | cbat__pcg_next(rng);
}

int64_t cbat_random_range(CBatRandom *rng, int64_t low, int64_t high) {
    uint64_t span;
    uint64_t threshold;
    uint64_t r;
    if (low > high) {
        int64_t tmp = low;
        low = high;
        high = tmp;
    }
    span = (uint64_t)high - (uint64_t)low + 1u; /* 0 means the full 64-bit range */
    if (span == 0) {
        return (int64_t)cbat_random_u64(rng);
    }
    /* Reject the biased tail so every value is equally likely. */
    threshold = (0u - span) % span;
    do {
        r = cbat_random_u64(rng);
    } while (r < threshold);
    return (int64_t)((uint64_t)low + r % span);
}

double cbat_random_double(CBatRandom *rng) {
    return (double)(cbat_random_u64(rng) >> 11) * (1.0 / 9007199254740992.0);
}

double cbat_random_uniform(CBatRandom *rng, double low, double high) {
    return low + (high - low) * cbat_random_double(rng);
}

double cbat_random_gaussian(CBatRandom *rng, double mean, double stddev) {
    double u1;
    double u2;
    do {
        u1 = cbat_random_double(rng);
    } while (u1 <= 0.0);
    u2 = cbat_random_double(rng);
    return mean + stddev * sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

bool cbat_random_chance(CBatRandom *rng, double p) {
    if (p <= 0.0) {
        return false;
    }
    if (p >= 1.0) {
        return true;
    }
    return cbat_random_double(rng) < p;
}

void cbat_random_bytes(CBatRandom *rng, void *buffer, size_t length) {
    unsigned char *bytes = (unsigned char *)buffer;
    size_t i = 0;
    if (buffer == NULL) {
        return;
    }
    rng = cbat__rng(rng);
    while (i < length) {
        uint32_t r = cbat__pcg_next(rng);
        size_t n = CBAT_MIN(length - i, sizeof(r));
        size_t k;
        for (k = 0; k < n; k++) {
            bytes[i++] = (unsigned char)(r >> (8 * k));
        }
    }
}

void cbat_random_shuffle(CBatRandom *rng, void *base, size_t count, size_t element_size) {
    unsigned char *bytes = (unsigned char *)base;
    size_t i;
    if (base == NULL || count < 2) {
        return;
    }
    for (i = count - 1; i > 0; i--) {
        size_t j = (size_t)cbat_random_range(rng, 0, (int64_t)i);
        cbat_swap(bytes + i * element_size, bytes + j * element_size, element_size);
    }
}

void cbat_random_shuffle_list(CBatRandom *rng, CBatList *list) {
    if (list != NULL) {
        cbat_random_shuffle(rng, list->data, list->length, list->element_size);
    }
}

void *cbat_random_choice(CBatRandom *rng, const CBatList *list) {
    if (list == NULL || list->length == 0) {
        return NULL;
    }
    return cbat_list_get(list, (size_t)cbat_random_range(rng, 0, (int64_t)list->length - 1));
}

CBatError cbat_random_sample(CBatRandom *rng, const CBatList *list, size_t count, CBatList **out) {
    CBatList *result;
    CBatError err;
    size_t i;
    if (list == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (count > list->length || list->length > INT64_MAX) return CBAT_ERR_OUT_OF_RANGE;
    result = cbat_list_new(list->element_size);
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat_list_reserve(result, count);
    if (err != CBAT_OK) { cbat_list_free(result); return err; }
    for (i = 0; i < count; i++) {
        err = cbat_list_push(result, cbat_list_get(list, i));
        if (err != CBAT_OK) { cbat_list_free(result); return err; }
    }
    if (count != 0) for (; i < list->length; i++) {
        size_t j = (size_t)cbat_random_range(rng, 0, (int64_t)i);
        if (j < count) memcpy(cbat_list_get(result, j), cbat_list_get(list, i), list->element_size);
    }
    cbat_random_shuffle_list(rng, result);
    *out = result;
    return CBAT_OK;
}

CBatError cbat_random_weighted_index(CBatRandom *rng, const double *weights, size_t count, size_t *out) {
    size_t i, last = 0;
    double maximum = 0, total = 0, draw;
    if (weights == NULL || count == 0 || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    for (i = 0; i < count; i++) {
        if (!isfinite(weights[i]) || weights[i] < 0) return CBAT_ERR_INVALID_ARGUMENT;
        if (weights[i] > maximum) maximum = weights[i];
    }
    if (maximum == 0) return CBAT_ERR_INVALID_ARGUMENT;
    for (i = 0; i < count; i++) total += weights[i] / maximum;
    draw = cbat_random_double(rng) * total;
    for (i = 0; i < count; i++) if (weights[i] > 0) {
        last = i;
        draw -= weights[i] / maximum;
        if (draw < 0) { *out = i; return CBAT_OK; }
    }
    *out = last;
    return CBAT_OK;
}

void cbat_random_uuid4(CBatRandom *rng, char out[37]) {
    static const char hex[] = "0123456789abcdef";
    unsigned char bytes[16];
    size_t i, at = 0;
    if (out == NULL) return;
    cbat_random_bytes(rng, bytes, sizeof(bytes));
    bytes[6] = (unsigned char)((bytes[6] & 15) | 64);
    bytes[8] = (unsigned char)((bytes[8] & 63) | 128);
    for (i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out[at++] = '-';
        out[at++] = hex[bytes[i] >> 4]; out[at++] = hex[bytes[i] & 15];
    }
    out[at] = '\0';
}
