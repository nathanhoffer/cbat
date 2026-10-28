/*
 * cbat/random.h - pseudo-random numbers (PCG32) (cbat's answer to Python's
 * random module).
 *
 *     int die = (int)cbat_random_range(NULL, 1, 6);    // global generator
 *
 *     CBatRandom rng;
 *     cbat_random_seed(&rng, 42);                      // reproducible
 *     double x = cbat_random_double(&rng);             // [0, 1)
 *
 * Every function accepts NULL for the generator to use a global one that
 * is seeded from the clock on first use (not thread-safe). NOT suitable for
 * cryptography.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_RANDOM_H
#define CBAT_RANDOM_H

#include "cbat/base.h"
#include "cbat/list.h"

CBAT_BEGIN_DECLS

/** Generator state (PCG32 XSH-RR). Treat fields as private. */
typedef struct CBatRandom {
    uint64_t state;
    uint64_t increment;
} CBatRandom;

/** Seeds deterministically: the same seed always gives the same sequence. */
CBAT_API void cbat_random_seed(CBatRandom *rng, uint64_t seed);

/** Seeds with an explicit stream selector (independent sequences per stream). */
CBAT_API void cbat_random_seed_stream(CBatRandom *rng, uint64_t seed, uint64_t stream);

/** Seeds from the clock and addresses (different every run). */
CBAT_API void cbat_random_seed_from_time(CBatRandom *rng);

CBAT_API uint32_t cbat_random_u32(CBatRandom *rng);
CBAT_API uint64_t cbat_random_u64(CBatRandom *rng);

/** Uniform integer in [low, high], both inclusive (like random.randint). */
CBAT_API int64_t cbat_random_range(CBatRandom *rng, int64_t low, int64_t high);

/** Uniform double in [0, 1) with 53 random bits. */
CBAT_API double cbat_random_double(CBatRandom *rng);

/** Uniform double in [low, high). */
CBAT_API double cbat_random_uniform(CBatRandom *rng, double low, double high);

/** Normally distributed value (Box-Muller). */
CBAT_API double cbat_random_gaussian(CBatRandom *rng, double mean, double stddev);

/** True with probability `p` (clamped to [0, 1]). */
CBAT_API bool cbat_random_chance(CBatRandom *rng, double p);

/** Fills a buffer with random bytes. */
CBAT_API void cbat_random_bytes(CBatRandom *rng, void *buffer, size_t length);

/** Shuffles an array in place (Fisher-Yates). */
CBAT_API void cbat_random_shuffle(CBatRandom *rng, void *base, size_t count, size_t element_size);

/** Shuffles a list in place. */
CBAT_API void cbat_random_shuffle_list(CBatRandom *rng, CBatList *list);

/** Pointer to a random element of the list, or NULL if it is empty. */
CBAT_API void *cbat_random_choice(CBatRandom *rng, const CBatList *list);

CBAT_END_DECLS

#endif /* CBAT_RANDOM_H */
