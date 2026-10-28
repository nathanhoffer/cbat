/* Recipe 28: Fitting, clustering and classifying a tiny table.
 * PROBLEM: Explore numeric data without an external ML dependency.
 * SOLUTION: Fit a line with QR, cluster rows, classify a query and split indices.
 * DISCUSSION: These are dense, in-memory algorithms for small datasets. Keep
 * train/query preprocessing consistent. PCG seeds make experiments repeatable;
 * k-means may find a local optimum and rank-deficient regression returns an error.
 */
#include "cbat/ml.h"
#include <stdio.h>

int main(void) {
    size_t shape[] = {6, 1}, rows = 6, query_size = 1, i;
    double features[] = {0, 1, 2, 8, 9, 10}, targets[] = {1, 3, 5, 17, 19, 21},
           query_value[] = {9.2};
    CBatArray *x = NULL, *y = NULL, *coefficients = NULL, *centers = NULL, *query = NULL;
    CBatList *labels = NULL, *classes = NULL, *train = NULL, *test = NULL;
    CBatRandom rng;
    double intercept = 0;
    int predicted = 0;
    CBatError err = cbat_array_from_values(2, shape, features, &x);
    if (err != CBAT_OK)
        goto cleanup;
    err = cbat_array_from_values(1, &rows, targets, &y);
    if (err != CBAT_OK)
        goto cleanup;
    err = cbat_ml_linear_regression_fit(x, y, &coefficients, &intercept);
    if (err != CBAT_OK)
        goto cleanup;
    printf("Line: y = %.3f*x + %.3f\n", cbat_array_data(coefficients)[0], intercept);
    cbat_random_seed(&rng, 42);
    err = cbat_ml_kmeans(x, 2, &rng, 100, &centers, &labels);
    if (err != CBAT_OK)
        goto cleanup;
    printf("Cluster centers: %.3f, %.3f\n", cbat_array_data(centers)[0],
           cbat_array_data(centers)[1]);
    classes = cbat_list_new(sizeof(int));
    if (classes == NULL) {
        err = CBAT_ERR_NO_MEMORY;
        goto cleanup;
    }
    for (i = 0; i < rows; i++) {
        int value = i < 3 ? 0 : 1;
        err = cbat_list_push(classes, &value);
        if (err != CBAT_OK)
            goto cleanup;
    }
    err = cbat_array_from_values(1, &query_size, query_value, &query);
    if (err != CBAT_OK)
        goto cleanup;
    err = cbat_ml_knn_classify(x, classes, query, 3, &predicted);
    if (err != CBAT_OK)
        goto cleanup;
    err = cbat_ml_train_test_split(x, 0.3, &rng, &train, &test);
    if (err != CBAT_OK)
        goto cleanup;
    printf("Query class: %d; train/test: %lu/%lu rows\n", predicted, (unsigned long)train->length,
           (unsigned long)test->length);
    err = cbat_ml_normalize_columns(x);
cleanup:
    if (err != CBAT_OK)
        fprintf(stderr, "%s\n", cbat_error_str(err));
    cbat_array_free(x);
    cbat_array_free(y);
    cbat_array_free(coefficients);
    cbat_array_free(centers);
    cbat_array_free(query);
    cbat_list_free(labels);
    cbat_list_free(classes);
    cbat_list_free(train);
    cbat_list_free(test);
    return err == CBAT_OK ? 0 : 1;
}
