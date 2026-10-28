/*
 * Recipe 16: Counting with integer keys
 *
 * PROBLEM
 *   Build a histogram of two-dice totals without turning integers into
 *   strings or reserving an array for the entire key range.
 *
 * SOLUTION
 *   CBatMap copies fixed-size int keys and int counts. Look up the previous
 *   count with a fallback, then insert the incremented value.
 *
 * DISCUSSION
 *   Map keys may also be int64_t or zero-initialized structs. Default keys
 *   are compared byte for byte; use custom hash/equality for pointer keys
 *   whose pointed-to content matters. Get/next pointers are borrowed and
 *   must be refreshed after modification. Typed macros accept scalars and
 *   pointers; use the address of a local variable for a struct key/value.
 */
#include <stdio.h>
#include "cbat/cbat.h"

int main(void) {
    CBatMap *counts = cbat_map_new(sizeof(int), sizeof(int));
    CBatRandom rng;
    CBatError err = CBAT_OK;
    int roll, sum, total = 0;
    if (counts == NULL) return 1;
    cbat_random_seed(&rng, 42);
    for (roll = 0; roll < 10000; roll++) {
        int count;
        sum = (int)cbat_random_range(&rng, 1, 6) + (int)cbat_random_range(&rng, 1, 6);
        count = CBAT_MAP_GET(counts, int, sum, int, 0);
        err = CBAT_MAP_SET(counts, int, sum, int, count + 1);
        if (err != CBAT_OK) break;
    }
    if (err == CBAT_OK) {
        for (sum = 2; sum <= 12; sum++) {
            int count = CBAT_MAP_GET(counts, int, sum, int, 0);
            printf("%2d: %4d rolls\n", sum, count);
            total += count;
        }
        printf("Total: %d rolls\n", total);
    } else {
        fprintf(stderr, "histogram: %s\n", cbat_error_str(err));
    }
    cbat_map_free(counts);
    return err == CBAT_OK && total == 10000 ? 0 : 1;
}
