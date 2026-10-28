/* cbat/ml.h - small dense-double ML helpers. MIT License.
 * Optional CBAT_WITH_ML (default ON) requires ndarray. Include explicitly.
 * Results use the default allocator. Fallible APIs leave outputs unchanged
 * on error. Finite numeric data required except for index-only splitting.
 * No trained-model object, implicit global state, or external dependencies.
 */
#ifndef CBAT_ML_H
#define CBAT_ML_H
#include "cbat/ndarray.h"
#include "cbat/random.h"
CBAT_BEGIN_DECLS
/** Ordinary least squares with intercept. X: rows*sources (2D); Y: rows
 * vector or rows*1. Returns coefficients vector and scalar intercept.
 * Centered/scaled, column-pivoted Householder QR; numerical rank deficiency
 * (including fewer than features+1 rows) gives OUT_OF_RANGE. Nonfinite
 * results give OVERFLOW. Zero features fits only the mean of Y. */
CBAT_API CBatError cbat_ml_linear_regression_fit(const CBatArray *x, const CBatArray *y,
    CBatArray **coefficients, double *intercept);
/** Lloyd k-means on a nonempty rows*features table. 1<=k<=rows, iterations>0.
 * Uniform distinct-row initialization; empty clusters reseed to farthest rows.
 * Returns k*features centroids and owned list of size_t labels (nearest final
 * center, ties choose smallest cluster index). Reproducible for same RNG/data.
 * NULL rng uses global PCG. RNG may advance even if a later operation fails. */
CBAT_API CBatError cbat_ml_kmeans(const CBatArray *points, size_t k, CBatRandom *rng, size_t max_iterations,
    CBatArray **centroids, CBatList **labels);
/** Unweighted kNN majority vote. train_x: rows*features; train_labels: int
 * elements, one per row. query: feature vector or 1*features table. Distance
 * ties choose earlier training row; vote ties choose smallest integer label.
 * 1<=k<=rows. Inputs unchanged; label unchanged on failure. */
CBAT_API CBatError cbat_ml_knn_classify(const CBatArray *train_x, const CBatList *train_labels,
    const CBatArray *query, size_t k, int *label);
/** Shuffled disjoint lists of size_t row indices. test count = ceil(rows*
 * fraction); fraction in [0,1], scalar X invalid. Empty tables allowed.
 * Outputs must be distinct. Array values need not be finite. */
CBAT_API CBatError cbat_ml_train_test_split(const CBatArray *x, double test_fraction, CBatRandom *rng,
    CBatList **train_indices, CBatList **test_indices);
/** In-place population z-scores per column (2D). Constant columns become zero;
 * empty tables unchanged. Stages results, preserving X on any failure. */
CBAT_API CBatError cbat_ml_normalize_columns(CBatArray *x);
CBAT_END_DECLS
#endif
