/*
 * cbat/stats.h - descriptive statistics over arrays of doubles (cbat's
 * answer to Python's statistics module).
 *
 *     double data[] = {2.5, 3.0, 4.5};
 *     double mean;
 *     if (cbat_stats_mean(data, 3, &mean) == CBAT_OK) { ... }
 *
 * Functions return CBAT_ERR_EMPTY for empty input, CBAT_ERR_INVALID_ARGUMENT
 * when a function needs more points (e.g. sample variance needs 2).
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_STATS_H
#define CBAT_STATS_H

#include "cbat/base.h"

CBAT_BEGIN_DECLS

/** Compensated (Neumaier) sum: accurate even for mixed magnitudes. 0 if empty. */
CBAT_API double cbat_stats_sum(const double *values, size_t count);

CBAT_API CBatError cbat_stats_mean(const double *values, size_t count, double *out);
CBAT_API CBatError cbat_stats_min(const double *values, size_t count, double *out);
CBAT_API CBatError cbat_stats_max(const double *values, size_t count, double *out);

/** Median (mean of the middle two for an even count). Does not modify input. */
CBAT_API CBatError cbat_stats_median(const double *values, size_t count, double *out);

/**
 * Percentile p in [0, 100] with linear interpolation between closest ranks
 * (numpy's default). Does not modify input.
 */
CBAT_API CBatError cbat_stats_percentile(const double *values, size_t count, double p,
                                         double *out);

/** Sample variance / standard deviation (n - 1 denominator; needs 2+ values). */
CBAT_API CBatError cbat_stats_variance(const double *values, size_t count, double *out);
CBAT_API CBatError cbat_stats_stdev(const double *values, size_t count, double *out);

/** Population variance / standard deviation (n denominator). */
CBAT_API CBatError cbat_stats_pvariance(const double *values, size_t count, double *out);
CBAT_API CBatError cbat_stats_pstdev(const double *values, size_t count, double *out);

/** Pearson correlation of x and y (needs 2+ points and non-constant inputs). */
CBAT_API CBatError cbat_stats_correlation(const double *x, const double *y, size_t count,
                                          double *out);

/** Least-squares line y = slope * x + intercept. */
CBAT_API CBatError cbat_stats_linear_regression(const double *x, const double *y, size_t count,
                                                double *slope, double *intercept);

CBAT_END_DECLS

#endif /* CBAT_STATS_H */
