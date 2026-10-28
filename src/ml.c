/* Dense ML helpers, independent implementations. MIT License. */
#include "cbat/ml.h"
#include "cbat/heap.h"
#include "cbat/map.h"
#include <float.h>
#include <math.h>
#include <string.h>

static bool cbat__ml_finite(const CBatArray *a) {
    size_t i;
    if (a == NULL)
        return false;
    for (i = 0; i < cbat_array_size(a); i++)
        if (!isfinite(cbat_array_const_data(a)[i]))
            return false;
    return true;
}
static double cbat__ml_scale(const CBatArray *a) {
    double maximum = 0;
    size_t i;
    for (i = 0; i < cbat_array_size(a); i++)
        maximum = fmax(maximum, fabs(cbat_array_const_data(a)[i]));
    return maximum == 0 ? 1 : maximum;
}
static double cbat__ml_distance(const double *a, const double *b, size_t features, double scale) {
    size_t j;
    double sum = 0;
    for (j = 0; j < features; j++) {
        double difference = a[j] - b[j];
        difference = isfinite(difference) ? difference / scale : a[j] / scale - b[j] / scale;
        sum += difference * difference;
    }
    return sum;
}
/* Rescale a*b/c without avoidable intermediate overflow/underflow. */
static double cbat__ml_rescale(double a, double b, double c) {
    int ae, be, ce, normal;
    double mantissa;
    if (a == 0 || b == 0)
        return copysign(0, a);
    mantissa = frexp(a, &ae) * frexp(b, &be) / frexp(c, &ce);
    mantissa = frexp(mantissa, &normal);
    return ldexp(mantissa, ae + be - ce + normal);
}

CBatError cbat_ml_linear_regression_fit(const CBatArray *x, const CBatArray *y,
                                        CBatArray **coefficients, double *intercept) {
    CBatArray *work = NULL, *target = NULL, *stats = NULL, *result = NULL;
    size_t *permutation = NULL;
    size_t rows, features, shape[2], i, j, k;
    double *a, *b, *s, *c, yscale, ymean = 0, offset;
    CBatError err = CBAT_OK;
    if (x == NULL || y == NULL || coefficients == NULL || intercept == NULL ||
        cbat_array_dimensions(x) != 2 || !cbat__ml_finite(x) || !cbat__ml_finite(y))
        return CBAT_ERR_INVALID_ARGUMENT;
    rows = cbat_array_shape(x, 0);
    features = cbat_array_shape(x, 1);
    if (rows == 0 ||
        (cbat_array_dimensions(y) != 1 &&
         !(cbat_array_dimensions(y) == 2 && cbat_array_shape(y, 1) == 1)) ||
        cbat_array_shape(y, 0) != rows)
        return CBAT_ERR_INVALID_ARGUMENT;
    if (features >= rows)
        return CBAT_ERR_OUT_OF_RANGE;
    shape[0] = rows;
    shape[1] = features;
    err = cbat_array_new(2, shape, &work);
    if (err != CBAT_OK)
        goto done;
    err = cbat_array_new(1, &rows, &target);
    if (err != CBAT_OK)
        goto done;
    shape[0] = 3;
    shape[1] = features;
    err = cbat_array_new(2, shape, &stats);
    if (err != CBAT_OK)
        goto done;
    err = cbat_array_new(1, &features, &result);
    if (err != CBAT_OK)
        goto done;
    if (features != 0) {
        permutation = (size_t *)cbat_alloc(NULL, features * sizeof(size_t));
        if (permutation == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            goto done;
        }
    }
    a = cbat_array_data(work);
    b = cbat_array_data(target);
    s = cbat_array_data(stats);
    c = cbat_array_data(result);
    yscale = cbat__ml_scale(y);
    {
        long double sum = 0;
        for (i = 0; i < rows; i++)
            sum += cbat_array_const_data(y)[i] / yscale;
        ymean = (double)(sum / (long double)rows);
    }
    for (i = 0; i < rows; i++)
        b[i] = cbat_array_const_data(y)[i] / yscale - ymean;
    for (j = 0; j < features; j++) {
        double maximum = 0, mean, norm = 0;
        long double sum = 0;
        for (i = 0; i < rows; i++)
            maximum = fmax(maximum, fabs(cbat_array_const_data(x)[i * features + j]));
        if (maximum == 0) {
            err = CBAT_ERR_OUT_OF_RANGE;
            goto done;
        }
        for (i = 0; i < rows; i++)
            sum += cbat_array_const_data(x)[i * features + j] / maximum;
        mean = (double)(sum / (long double)rows);
        for (i = 0; i < rows; i++) {
            a[i * features + j] = cbat_array_const_data(x)[i * features + j] / maximum - mean;
            norm = hypot(norm, a[i * features + j]);
        }
        if (norm == 0) {
            err = CBAT_ERR_OUT_OF_RANGE;
            goto done;
        }
        for (i = 0; i < rows; i++)
            a[i * features + j] /= norm;
        s[j] = maximum;
        s[features + j] = mean;
        s[2 * features + j] = norm;
        permutation[j] = j;
    }
    for (k = 0; k < features; k++) {
        size_t pivot = k;
        double largest = 0, norm, alpha, v0, tau;
        for (j = k; j < features; j++) {
            double candidate = 0;
            for (i = k; i < rows; i++)
                candidate = hypot(candidate, a[i * features + j]);
            if (candidate > largest) {
                largest = candidate;
                pivot = j;
            }
        }
        if (largest <= DBL_EPSILON * (double)rows) {
            err = CBAT_ERR_OUT_OF_RANGE;
            goto done;
        }
        if (pivot != k) {
            size_t p = permutation[k];
            permutation[k] = permutation[pivot];
            permutation[pivot] = p;
            for (i = 0; i < rows; i++) {
                double swap = a[i * features + k];
                a[i * features + k] = a[i * features + pivot];
                a[i * features + pivot] = swap;
            }
        }
        norm = 0;
        for (i = k; i < rows; i++)
            norm = hypot(norm, a[i * features + k]);
        alpha = -copysign(norm, a[k * features + k]);
        v0 = a[k * features + k] - alpha;
        tau = (alpha - a[k * features + k]) / alpha;
        for (i = k + 1; i < rows; i++)
            a[i * features + k] /= v0;
        a[k * features + k] = alpha;
        for (j = k + 1; j < features; j++) {
            double dot = a[k * features + j];
            for (i = k + 1; i < rows; i++)
                dot += a[i * features + k] * a[i * features + j];
            dot *= tau;
            a[k * features + j] -= dot;
            for (i = k + 1; i < rows; i++)
                a[i * features + j] -= a[i * features + k] * dot;
        }
        {
            double dot = b[k];
            for (i = k + 1; i < rows; i++)
                dot += a[i * features + k] * b[i];
            dot *= tau;
            b[k] -= dot;
            for (i = k + 1; i < rows; i++)
                b[i] -= a[i * features + k] * dot;
        }
    }
    for (i = features; i != 0; i--) {
        double value = b[i - 1];
        for (j = i; j < features; j++)
            value -= a[(i - 1) * features + j] * b[j];
        b[i - 1] = value / a[(i - 1) * features + i - 1];
    }
    offset = ymean;
    for (j = 0; j < features; j++) {
        size_t original = permutation[j];
        double scaled = b[j] / s[2 * features + original];
        c[original] = cbat__ml_rescale(scaled, yscale, s[original]);
        offset -= scaled * s[features + original];
        if (!isfinite(c[original])) {
            err = CBAT_ERR_OVERFLOW;
            goto done;
        }
    }
    offset *= yscale;
    if (!isfinite(offset)) {
        err = CBAT_ERR_OVERFLOW;
        goto done;
    }
    *coefficients = result;
    result = NULL;
    *intercept = offset;
done:
    cbat_array_free(work);
    cbat_array_free(target);
    cbat_array_free(stats);
    cbat_array_free(result);
    cbat_free(permutation);
    return err;
}

CBatError cbat_ml_normalize_columns(CBatArray *x) {
    CBatArray *staged = NULL;
    size_t rows, features, shape[2], i, j;
    CBatError err;
    if (x == NULL || cbat_array_dimensions(x) != 2 || !cbat__ml_finite(x))
        return CBAT_ERR_INVALID_ARGUMENT;
    rows = shape[0] = cbat_array_shape(x, 0);
    features = shape[1] = cbat_array_shape(x, 1);
    if (cbat_array_size(x) == 0)
        return CBAT_OK;
    err = cbat_array_new(2, shape, &staged);
    if (err != CBAT_OK)
        return err;
    for (j = 0; j < features; j++) {
        double maximum = 0, mean, sd;
        long double sum = 0, variance = 0;
        for (i = 0; i < rows; i++)
            maximum = fmax(maximum, fabs(cbat_array_const_data(x)[i * features + j]));
        if (maximum == 0)
            continue;
        for (i = 0; i < rows; i++)
            sum += cbat_array_const_data(x)[i * features + j] / maximum;
        mean = (double)(sum / (long double)rows);
        for (i = 0; i < rows; i++) {
            double difference = cbat_array_const_data(x)[i * features + j] / maximum - mean;
            variance += (long double)difference * difference;
        }
        sd = sqrt((double)(variance / (long double)rows));
        for (i = 0; i < rows; i++)
            cbat_array_data(staged)[i * features + j] =
                sd == 0 ? 0 : (cbat_array_const_data(x)[i * features + j] / maximum - mean) / sd;
    }
    memcpy(cbat_array_data(x), cbat_array_const_data(staged), cbat_array_size(x) * sizeof(double));
    cbat_array_free(staged);
    return CBAT_OK;
}

CBatError cbat_ml_train_test_split(const CBatArray *x, double fraction, CBatRandom *rng,
                                   CBatList **train_indices, CBatList **test_indices) {
    CBatList *indices = NULL, *train = NULL, *test = NULL;
    size_t rows, test_count, i;
    CBatError err = CBAT_OK;
    if (x == NULL || cbat_array_dimensions(x) == 0 || train_indices == NULL ||
        test_indices == NULL || train_indices == test_indices || !isfinite(fraction) ||
        fraction < 0 || fraction > 1)
        return CBAT_ERR_INVALID_ARGUMENT;
    rows = cbat_array_shape(x, 0);
    if (rows > INT64_MAX)
        return CBAT_ERR_OUT_OF_RANGE;
    {
        long double count = ceill((long double)rows * fraction);
        test_count = count >= (long double)rows ? rows : (size_t)count;
    }
    indices = cbat_list_new(sizeof(size_t));
    train = cbat_list_new(sizeof(size_t));
    test = cbat_list_new(sizeof(size_t));
    if (indices == NULL || train == NULL || test == NULL) {
        err = CBAT_ERR_NO_MEMORY;
        goto done;
    }
    err = cbat_list_reserve(indices, rows);
    if (err == CBAT_OK)
        err = cbat_list_reserve(train, rows - test_count);
    if (err == CBAT_OK)
        err = cbat_list_reserve(test, test_count);
    if (err != CBAT_OK)
        goto done;
    for (i = 0; i < rows; i++) {
        err = cbat_list_push(indices, &i);
        if (err != CBAT_OK)
            goto done;
    }
    cbat_random_shuffle_list(rng, indices);
    for (i = 0; i < rows; i++) {
        err = cbat_list_push(i < test_count ? test : train, cbat_list_get(indices, i));
        if (err != CBAT_OK)
            goto done;
    }
    *train_indices = train;
    *test_indices = test;
    train = NULL;
    test = NULL;
done:
    cbat_list_free(indices);
    cbat_list_free(train);
    cbat_list_free(test);
    return err;
}

typedef struct cbat__ml_neighbor {
    double distance;
    size_t index;
} cbat__ml_neighbor;
static int cbat__ml_neighbor_compare(const void *a, const void *b) {
    const cbat__ml_neighbor *x = (const cbat__ml_neighbor *)a, *y = (const cbat__ml_neighbor *)b;
    if (x->distance != y->distance)
        return x->distance > y->distance ? -1 : 1;
    return x->index > y->index ? -1 : x->index < y->index ? 1 : 0;
}
CBatError cbat_ml_knn_classify(const CBatArray *x, const CBatList *labels, const CBatArray *query,
                               size_t k, int *label) {
    CBatHeap *heap = NULL;
    CBatMap *votes = NULL;
    size_t rows, features, i, best_count = 0;
    int best = 0;
    double scale;
    CBatError err = CBAT_OK;
    if (x == NULL || labels == NULL || query == NULL || label == NULL ||
        cbat_array_dimensions(x) != 2 || !cbat__ml_finite(x) || !cbat__ml_finite(query) ||
        labels->element_size != sizeof(int))
        return CBAT_ERR_INVALID_ARGUMENT;
    rows = cbat_array_shape(x, 0);
    features = cbat_array_shape(x, 1);
    if (rows == 0 || features == 0 || k == 0 || k > rows || labels->length != rows ||
        !((cbat_array_dimensions(query) == 1 && cbat_array_shape(query, 0) == features) ||
          (cbat_array_dimensions(query) == 2 && cbat_array_shape(query, 0) == 1 &&
           cbat_array_shape(query, 1) == features)))
        return CBAT_ERR_INVALID_ARGUMENT;
    scale = 0;
    for (i = 0; i < rows * features; i++) {
        double difference =
            cbat_array_const_data(x)[i] - cbat_array_const_data(query)[i % features];
        scale = fmax(scale, isfinite(difference) ? fabs(difference) : DBL_MAX);
    }
    if (scale == 0)
        scale = 1;
    heap = cbat_heap_new(sizeof(cbat__ml_neighbor), cbat__ml_neighbor_compare);
    votes = cbat_map_new(sizeof(int), sizeof(size_t));
    if (heap == NULL || votes == NULL) {
        err = CBAT_ERR_NO_MEMORY;
        goto done;
    }
    for (i = 0; i < rows; i++) {
        cbat__ml_neighbor neighbor;
        neighbor.index = i;
        neighbor.distance = cbat__ml_distance(cbat_array_const_data(x) + i * features,
                                              cbat_array_const_data(query), features, scale);
        err = i < k ? cbat_heap_push(heap, &neighbor) : cbat_heap_push_pop(heap, &neighbor, NULL);
        if (err != CBAT_OK)
            goto done;
    }
    for (i = 0; i < k; i++) {
        cbat__ml_neighbor neighbor;
        int value;
        size_t *previous, count;
        err = cbat_heap_pop(heap, &neighbor);
        if (err != CBAT_OK)
            goto done;
        value = *(int *)cbat_list_get(labels, neighbor.index);
        previous = (size_t *)cbat_map_get(votes, &value);
        count = previous == NULL ? 1 : *previous + 1;
        err = cbat_map_set(votes, &value, &count);
        if (err != CBAT_OK)
            goto done;
        if (count > best_count || (count == best_count && value < best)) {
            best = value;
            best_count = count;
        }
    }
    *label = best;
done:
    cbat_heap_free(heap);
    cbat_map_free(votes);
    return err;
}

CBatError cbat_ml_kmeans(const CBatArray *points, size_t k, CBatRandom *rng, size_t max_iterations,
                         CBatArray **centroids, CBatList **labels) {
    CBatArray *normalized = NULL, *centers = NULL, *sums = NULL, *distances = NULL;
    CBatList *assignments = NULL;
    size_t *indices = NULL, *counts = NULL;
    size_t rows, features, shape[2], i, j, cluster, iteration;
    double scale;
    CBatError err = CBAT_OK;
    if (points == NULL || centroids == NULL || labels == NULL ||
        (void *)centroids == (void *)labels || cbat_array_dimensions(points) != 2 ||
        !cbat__ml_finite(points))
        return CBAT_ERR_INVALID_ARGUMENT;
    rows = cbat_array_shape(points, 0);
    features = cbat_array_shape(points, 1);
    if (rows == 0 || features == 0 || k == 0 || k > rows || max_iterations == 0)
        return CBAT_ERR_INVALID_ARGUMENT;
    if (rows > INT64_MAX)
        return CBAT_ERR_OUT_OF_RANGE;
    shape[0] = rows;
    shape[1] = features;
    err = cbat_array_new(2, shape, &normalized);
    if (err != CBAT_OK)
        goto done;
    shape[0] = k;
    err = cbat_array_new(2, shape, &centers);
    if (err != CBAT_OK)
        goto done;
    err = cbat_array_new(2, shape, &sums);
    if (err != CBAT_OK)
        goto done;
    err = cbat_array_new(1, &rows, &distances);
    if (err != CBAT_OK)
        goto done;
    indices = (size_t *)cbat_alloc(NULL, rows * sizeof(size_t));
    counts = (size_t *)cbat_alloc(NULL, k * sizeof(size_t));
    assignments = cbat_list_new(sizeof(size_t));
    if (indices == NULL || counts == NULL || assignments == NULL) {
        err = CBAT_ERR_NO_MEMORY;
        goto done;
    }
    err = cbat_list_reserve(assignments, rows);
    if (err != CBAT_OK)
        goto done;
    for (i = 0; i < rows; i++) {
        size_t unset = SIZE_MAX;
        indices[i] = i;
        err = cbat_list_push(assignments, &unset);
        if (err != CBAT_OK)
            goto done;
    }
    /* Translate before scaling so a huge constant coordinate does not erase
     * tiny variation in another feature. Overflowing differences are scaled
     * before subtraction instead. */
    scale = 0;
    for (i = 0; i < cbat_array_size(points); i++) {
        double difference =
            cbat_array_const_data(points)[i] - cbat_array_const_data(points)[i % features];
        scale = fmax(scale, isfinite(difference) ? fabs(difference) : DBL_MAX);
    }
    if (scale == 0)
        scale = 1;
    for (i = 0; i < cbat_array_size(points); i++) {
        double point = cbat_array_const_data(points)[i],
               origin = cbat_array_const_data(points)[i % features], difference = point - origin;
        cbat_array_data(normalized)[i] =
            isfinite(difference) ? difference / scale : point / scale - origin / scale;
    }
    for (i = 0; i < k; i++) {
        size_t chosen = (size_t)cbat_random_range(rng, (int64_t)i, (int64_t)rows - 1),
               swap = indices[i];
        indices[i] = indices[chosen];
        indices[chosen] = swap;
        memcpy(cbat_array_data(centers) + i * features,
               cbat_array_const_data(normalized) + indices[i] * features,
               features * sizeof(double));
    }
    for (iteration = 0; iteration < max_iterations; iteration++) {
        bool changed = false;
        memset(counts, 0, k * sizeof(size_t));
        memset(cbat_array_data(sums), 0, cbat_array_size(sums) * sizeof(double));
        for (i = 0; i < rows; i++) {
            size_t nearest = 0;
            double distance = INFINITY;
            for (cluster = 0; cluster < k; cluster++) {
                double d = cbat__ml_distance(cbat_array_const_data(normalized) + i * features,
                                             cbat_array_const_data(centers) + cluster * features,
                                             features, 1);
                if (d < distance) {
                    distance = d;
                    nearest = cluster;
                }
            }
            if (*(size_t *)cbat_list_get(assignments, i) != nearest)
                changed = true;
            *(size_t *)cbat_list_get(assignments, i) = nearest;
            cbat_array_data(distances)[i] = distance;
            counts[nearest]++;
            for (j = 0; j < features; j++)
                cbat_array_data(sums)[nearest * features + j] +=
                    cbat_array_const_data(normalized)[i * features + j];
        }
        for (cluster = 0; cluster < k; cluster++) {
            if (counts[cluster] != 0)
                for (j = 0; j < features; j++)
                    cbat_array_data(centers)[cluster * features + j] =
                        cbat_array_const_data(sums)[cluster * features + j] /
                        (double)counts[cluster];
            else {
                size_t farthest = 0;
                for (i = 1; i < rows; i++)
                    if (cbat_array_const_data(distances)[i] >
                        cbat_array_const_data(distances)[farthest])
                        farthest = i;
                if (memcmp(cbat_array_data(centers) + cluster * features,
                           cbat_array_const_data(normalized) + farthest * features,
                           features * sizeof(double)) != 0)
                    changed = true;
                memcpy(cbat_array_data(centers) + cluster * features,
                       cbat_array_const_data(normalized) + farthest * features,
                       features * sizeof(double));
                cbat_array_data(distances)[farthest] = -1;
            }
        }
        if (!changed)
            break;
    }
    for (i = 0; i < rows; i++) {
        size_t nearest = 0;
        double distance = INFINITY;
        for (cluster = 0; cluster < k; cluster++) {
            double d =
                cbat__ml_distance(cbat_array_const_data(normalized) + i * features,
                                  cbat_array_const_data(centers) + cluster * features, features, 1);
            if (d < distance) {
                distance = d;
                nearest = cluster;
            }
        }
        *(size_t *)cbat_list_get(assignments, i) = nearest;
    }
    for (j = 0; j < features; j++) {
        double low = cbat_array_const_data(points)[j], high = low;
        for (i = 1; i < rows; i++) {
            low = fmin(low, cbat_array_const_data(points)[i * features + j]);
            high = fmax(high, cbat_array_const_data(points)[i * features + j]);
        }
        for (cluster = 0; cluster < k; cluster++)
            cbat_array_data(centers)[cluster * features + j] =
                fmax(low, fmin(high, fma(cbat_array_data(centers)[cluster * features + j], scale,
                                         cbat_array_const_data(points)[j])));
    }
    *centroids = centers;
    *labels = assignments;
    centers = NULL;
    assignments = NULL;
done:
    cbat_array_free(normalized);
    cbat_array_free(centers);
    cbat_array_free(sums);
    cbat_array_free(distances);
    cbat_list_free(assignments);
    cbat_free(indices);
    cbat_free(counts);
    return err;
}
