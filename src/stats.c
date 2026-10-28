/*
 * cbat/stats.c - descriptive statistics.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/stats.h"

#include <math.h>
#include <string.h>

#include "cbat/sort.h"

double cbat_stats_sum(const double *values, size_t count) {
    double sum = 0.0;
    double compensation = 0.0;
    size_t i;
    if (values == NULL) {
        return 0.0;
    }
    for (i = 0; i < count; i++) {
        double t = sum + values[i];
        if (fabs(sum) >= fabs(values[i])) {
            compensation += (sum - t) + values[i];
        } else {
            compensation += (values[i] - t) + sum;
        }
        sum = t;
    }
    return sum + compensation;
}

static CBatError cbat__stats_check(const double *values, size_t count, const double *out) {
    if (out == NULL || (values == NULL && count > 0)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return count == 0 ? CBAT_ERR_EMPTY : CBAT_OK;
}

CBatError cbat_stats_mean(const double *values, size_t count, double *out) {
    CBatError err = cbat__stats_check(values, count, out);
    if (err != CBAT_OK) {
        return err;
    }
    *out = cbat_stats_sum(values, count) / (double)count;
    return CBAT_OK;
}

CBatError cbat_stats_min(const double *values, size_t count, double *out) {
    CBatError err = cbat__stats_check(values, count, out);
    size_t i;
    double best;
    if (err != CBAT_OK) {
        return err;
    }
    best = values[0];
    for (i = 1; i < count; i++) {
        if (values[i] < best) {
            best = values[i];
        }
    }
    *out = best;
    return CBAT_OK;
}

CBatError cbat_stats_max(const double *values, size_t count, double *out) {
    CBatError err = cbat__stats_check(values, count, out);
    size_t i;
    double best;
    if (err != CBAT_OK) {
        return err;
    }
    best = values[0];
    for (i = 1; i < count; i++) {
        if (values[i] > best) {
            best = values[i];
        }
    }
    *out = best;
    return CBAT_OK;
}

/* Sorted copy of the input, released with cbat_free. */
static CBatError cbat__stats_sorted_copy(const double *values, size_t count, double **out) {
    size_t bytes;
    double *copy;
    CBatError err;
    if (!cbat_size_mul(count, sizeof(double), &bytes)) {
        return CBAT_ERR_OVERFLOW;
    }
    copy = (double *)cbat_alloc(NULL, bytes);
    if (copy == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    memcpy(copy, values, bytes);
    err = cbat_sort(copy, count, sizeof(double), cbat_compare_double);
    if (err != CBAT_OK) {
        cbat_free(copy);
        return err;
    }
    *out = copy;
    return CBAT_OK;
}

CBatError cbat_stats_percentile(const double *values, size_t count, double p, double *out) {
    CBatError err = cbat__stats_check(values, count, out);
    double *sorted = NULL;
    double rank;
    size_t below;
    double fraction;
    if (err != CBAT_OK) {
        return err;
    }
    if (!(p >= 0.0 && p <= 100.0)) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    err = cbat__stats_sorted_copy(values, count, &sorted);
    if (err != CBAT_OK) {
        return err;
    }
    rank = p / 100.0 * (double)(count - 1);
    below = (size_t)floor(rank);
    fraction = rank - (double)below;
    if (below + 1 < count) {
        *out = sorted[below] + (sorted[below + 1] - sorted[below]) * fraction;
    } else {
        *out = sorted[count - 1];
    }
    cbat_free(sorted);
    return CBAT_OK;
}

CBatError cbat_stats_median(const double *values, size_t count, double *out) {
    return cbat_stats_percentile(values, count, 50.0, out);
}

/* Sum of squared deviations from the mean (two-pass for accuracy). */
static double cbat__stats_ss(const double *values, size_t count) {
    double mean = cbat_stats_sum(values, count) / (double)count;
    double ss = 0.0;
    double correction = 0.0;
    size_t i;
    for (i = 0; i < count; i++) {
        double d = values[i] - mean;
        ss += d * d;
        correction += d;
    }
    /* Compensates rounding in the mean (Chan et al.). */
    return ss - correction * correction / (double)count;
}

CBatError cbat_stats_variance(const double *values, size_t count, double *out) {
    CBatError err = cbat__stats_check(values, count, out);
    if (err != CBAT_OK) {
        return err;
    }
    if (count < 2) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    *out = cbat__stats_ss(values, count) / (double)(count - 1);
    return CBAT_OK;
}

CBatError cbat_stats_pvariance(const double *values, size_t count, double *out) {
    CBatError err = cbat__stats_check(values, count, out);
    if (err != CBAT_OK) {
        return err;
    }
    *out = cbat__stats_ss(values, count) / (double)count;
    return CBAT_OK;
}

CBatError cbat_stats_stdev(const double *values, size_t count, double *out) {
    CBatError err = cbat_stats_variance(values, count, out);
    if (err == CBAT_OK) {
        *out = sqrt(*out);
    }
    return err;
}

CBatError cbat_stats_pstdev(const double *values, size_t count, double *out) {
    CBatError err = cbat_stats_pvariance(values, count, out);
    if (err == CBAT_OK) {
        *out = sqrt(*out);
    }
    return err;
}

/* Centered sums for x/y: sxx, syy, sxy. */
static CBatError cbat__stats_moments(const double *x, const double *y, size_t count,
                                     double *sxx, double *syy, double *sxy) {
    double mx;
    double my;
    size_t i;
    if (x == NULL || y == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (count == 0) {
        return CBAT_ERR_EMPTY;
    }
    if (count < 2) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    mx = cbat_stats_sum(x, count) / (double)count;
    my = cbat_stats_sum(y, count) / (double)count;
    *sxx = *syy = *sxy = 0.0;
    for (i = 0; i < count; i++) {
        double dx = x[i] - mx;
        double dy = y[i] - my;
        *sxx += dx * dx;
        *syy += dy * dy;
        *sxy += dx * dy;
    }
    return CBAT_OK;
}

CBatError cbat_stats_correlation(const double *x, const double *y, size_t count, double *out) {
    double sxx;
    double syy;
    double sxy;
    CBatError err;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__stats_moments(x, y, count, &sxx, &syy, &sxy);
    if (err != CBAT_OK) {
        return err;
    }
    if (sxx == 0.0 || syy == 0.0) {
        return CBAT_ERR_INVALID_ARGUMENT; /* a constant input has no correlation */
    }
    *out = sxy / sqrt(sxx * syy);
    return CBAT_OK;
}

CBatError cbat_stats_linear_regression(const double *x, const double *y, size_t count,
                                       double *slope, double *intercept) {
    double sxx;
    double syy;
    double sxy;
    double b;
    CBatError err;
    if (slope == NULL || intercept == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__stats_moments(x, y, count, &sxx, &syy, &sxy);
    if (err != CBAT_OK) {
        return err;
    }
    if (sxx == 0.0) {
        return CBAT_ERR_INVALID_ARGUMENT; /* x is constant: vertical line */
    }
    b = sxy / sxx;
    *slope = b;
    *intercept = cbat_stats_sum(y, count) / (double)count - b * (cbat_stats_sum(x, count) / (double)count);
    return CBAT_OK;
}
