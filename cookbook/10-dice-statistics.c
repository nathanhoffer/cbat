/*
 * Recipe 10: Simulating dice and summarizing the results
 *
 * PROBLEM
 *   Run a quick Monte Carlo simulation and describe the outcome with
 *   mean, standard deviation, median and a text histogram.
 *
 * SOLUTION
 *   A seeded CBatRandom makes the run reproducible; cbat_random_range()
 *   gives unbiased die rolls; cbat_stats_* summarizes the samples.
 *
 * DISCUSSION
 *   Seed with a constant while developing so results repeat, then switch
 *   to cbat_random_seed_from_time() (or pass NULL to use the global
 *   generator). Two dice sum to 7 most often - the histogram shows it.
 */
#include <stdio.h>

#include "cbat/cbat.h"

#define ROLLS 6000

int main(void) {
    static double samples[ROLLS];
    int histogram[13] = {0};
    CBatRandom rng;
    double mean = 0;
    double stdev = 0;
    double median = 0;
    int i;
    int face;

    cbat_random_seed(&rng, 2026);
    for (i = 0; i < ROLLS; i++) {
        int total = (int)cbat_random_range(&rng, 1, 6) + (int)cbat_random_range(&rng, 1, 6);
        samples[i] = total;
        histogram[total]++;
    }
    cbat_stats_mean(samples, ROLLS, &mean);
    cbat_stats_stdev(samples, ROLLS, &stdev);
    cbat_stats_median(samples, ROLLS, &median);
    printf("two dice, %d rolls: mean %.3f, stdev %.3f, median %.0f\n", ROLLS, mean, stdev, median);
    for (face = 2; face <= 12; face++) {
        int bar;
        printf("%2d | ", face);
        for (bar = 0; bar < histogram[face] / 25; bar++) {
            putchar('#');
        }
        printf(" %d\n", histogram[face]);
    }
    return mean > 6.8 && mean < 7.2 ? 0 : 1;
}
