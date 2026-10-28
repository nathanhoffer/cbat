#include "cbat/ml.h"
#include "cbat/test.h"
#include <float.h>
#include <math.h>

CBAT_TEST(test_regression_and_normalization) {
    size_t shape[] = {6, 2}, ys = 6;
    double xv[] = {0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2}, yv[] = {3, 5, 2, 4, 6, 3};
    CBatArray *x = NULL, *y = NULL, *coefficients = NULL;
    double intercept = 99;
    CBAT_REQUIRE_OK(cbat_array_from_values(2, shape, xv, &x));
    CBAT_REQUIRE_OK(cbat_array_from_values(1, &ys, yv, &y));
    CBAT_REQUIRE_OK(cbat_ml_linear_regression_fit(x, y, &coefficients, &intercept));
    CBAT_CHECK_NEAR(intercept, 3, 1e-12);
    CBAT_CHECK_NEAR(cbat_array_data(coefficients)[0], 2, 1e-12);
    CBAT_CHECK_NEAR(cbat_array_data(coefficients)[1], -1, 1e-12);
    CBAT_REQUIRE_OK(cbat_ml_normalize_columns(x));
    {
        size_t j, i;
        for (j = 0; j < 2; j++) {
            double sum = 0, squares = 0;
            for (i = 0; i < 6; i++) {
                double v = cbat_array_data(x)[2 * i + j];
                sum += v;
                squares += v * v;
            }
            CBAT_CHECK_NEAR(sum, 0, 1e-12);
            CBAT_CHECK_NEAR(squares / 6, 1, 1e-12);
        }
    }
    cbat_array_free(x);
    cbat_array_free(y);
    cbat_array_free(coefficients);
}
CBAT_TEST(test_knn_ties_and_split) {
    size_t shape[] = {4, 1}, qs = 1, i;
    double xv[] = {0, 1, 9, 10}, qv[] = {0.5};
    int classes[] = {5, -2, 7, 7}, label = 99;
    CBatArray *x = NULL, *query = NULL;
    CBatList *labels = cbat_list_new(sizeof(int)), *train = NULL, *test = NULL;
    CBatRandom rng;
    bool seen[4] = {false};
    CBAT_REQUIRE_NOT_NULL(labels);
    for (i = 0; i < 4; i++)
        CBAT_REQUIRE_OK(cbat_list_push(labels, &classes[i]));
    CBAT_REQUIRE_OK(cbat_array_from_values(2, shape, xv, &x));
    CBAT_REQUIRE_OK(cbat_array_from_values(1, &qs, qv, &query));
    CBAT_REQUIRE_OK(cbat_ml_knn_classify(x, labels, query, 1, &label));
    CBAT_CHECK_EQ_INT(label, 5);
    CBAT_REQUIRE_OK(cbat_ml_knn_classify(x, labels, query, 2, &label));
    CBAT_CHECK_EQ_INT(label, -2);
    cbat_random_seed(&rng, 42);
    CBAT_REQUIRE_OK(cbat_ml_train_test_split(x, 0.3, &rng, &train, &test));
    CBAT_CHECK_EQ_INT(train->length, 2);
    CBAT_CHECK_EQ_INT(test->length, 2);
    for (i = 0; i < 4; i++) {
        size_t ix = *(size_t *)cbat_list_get(i < 2 ? train : test, i % 2);
        CBAT_CHECK(ix < 4);
        CBAT_CHECK(!seen[ix]);
        seen[ix] = true;
    }
    cbat_list_free(train);
    cbat_list_free(test);
    cbat_list_free(labels);
    cbat_array_free(x);
    cbat_array_free(query);
}
CBAT_TEST(test_kmeans_reproducibility_and_dynamic_range) {
    size_t shape[] = {4, 2}, i;
    double values[] = {1e300, 0, 1e300, 1e-300, 1e300, 9e-300, 1e300, 10e-300};
    CBatArray *x = NULL, *centers = NULL, *again = NULL;
    CBatList *labels = NULL, *again_labels = NULL;
    CBatRandom rng;
    CBAT_REQUIRE_OK(cbat_array_from_values(2, shape, values, &x));
    cbat_random_seed(&rng, 42);
    CBAT_REQUIRE_OK(cbat_ml_kmeans(x, 2, &rng, 50, &centers, &labels));
    cbat_random_seed(&rng, 42);
    CBAT_REQUIRE_OK(cbat_ml_kmeans(x, 2, &rng, 50, &again, &again_labels));
    CBAT_CHECK_EQ_MEM(cbat_array_data(centers), cbat_array_data(again), 4 * sizeof(double));
    CBAT_CHECK_EQ_MEM(labels->data, again_labels->data, 4 * sizeof(size_t));
    CBAT_CHECK(*(size_t *)cbat_list_get(labels, 0) == *(size_t *)cbat_list_get(labels, 1));
    CBAT_CHECK(*(size_t *)cbat_list_get(labels, 0) != *(size_t *)cbat_list_get(labels, 2));
    for (i = 0; i < 2; i++)
        CBAT_CHECK_NEAR(cbat_array_data(centers)[2 * i], 1e300, 0);
    cbat_array_free(x);
    cbat_array_free(centers);
    cbat_array_free(again);
    cbat_list_free(labels);
    cbat_list_free(again_labels);
}
CBAT_TEST(test_errors_and_all_oom_paths) {
    size_t shape[] = {6, 2}, ys = 6, qs = 2, n, op, i;
    double xv[] = {0, 0, 1, 0, 0, 1, 1, 1, 2, 1, 1, 2}, yv[] = {3, 5, 2, 4, 6, 3}, qv[] = {0, 0};
    CBatArray *x = NULL, *y = NULL, *query = NULL;
    CBatList *classes = cbat_list_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(classes);
    for (i = 0; i < 6; i++) {
        int value = (int)(i % 2);
        CBAT_REQUIRE_OK(cbat_list_push(classes, &value));
    }
    CBAT_REQUIRE_OK(cbat_array_from_values(2, shape, xv, &x));
    CBAT_REQUIRE_OK(cbat_array_from_values(1, &ys, yv, &y));
    CBAT_REQUIRE_OK(cbat_array_from_values(1, &qs, qv, &query));
    for (op = 0; op < 5; op++) {
        bool success = false;
        for (n = 0; n < 100; n++) {
            CBatArray *out = NULL;
            CBatList *one = NULL, *two = NULL;
            double intercept = 99;
            int label = 99;
            CBatError err;
            CBatRandom rng;
            cbat_random_seed(&rng, 42);
            cbat_test_fail_allocations_after(n);
            switch (op) {
            case 0:
                err = cbat_ml_linear_regression_fit(x, y, &out, &intercept);
                break;
            case 1:
                err = cbat_ml_kmeans(x, 2, &rng, 10, &out, &one);
                break;
            case 2:
                err = cbat_ml_knn_classify(x, classes, query, 3, &label);
                break;
            case 3:
                err = cbat_ml_train_test_split(x, 0.3, &rng, &one, &two);
                break;
            default:
                err = cbat_ml_normalize_columns(x);
                break;
            }
            cbat_test_stop_failing_allocations();
            if (err == CBAT_OK)
                success = true;
            else {
                CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
                CBAT_CHECK_NULL(out);
                CBAT_CHECK_NULL(one);
                CBAT_CHECK_NULL(two);
                CBAT_CHECK_NEAR(intercept, 99, 0);
                CBAT_CHECK_EQ_INT(label, 99);
                if (op == 4)
                    CBAT_CHECK_EQ_MEM(cbat_array_data(x), xv, sizeof(xv));
            }
            cbat_array_free(out);
            cbat_list_free(one);
            cbat_list_free(two);
            if (success)
                break;
        }
        CBAT_CHECK(success);
    }
    {
        CBatArray *out = NULL;
        CBatList *list = NULL;
        double intercept = 0;
        int label = 0;
        CBAT_CHECK_ERR(cbat_ml_linear_regression_fit(NULL, y, &out, &intercept),
                       CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_ml_kmeans(x, 0, NULL, 10, &out, &list), CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_ml_knn_classify(x, classes, query, 0, &label),
                       CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_ml_train_test_split(x, -1, NULL, &list, &list),
                       CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_ml_normalize_columns(NULL), CBAT_ERR_INVALID_ARGUMENT);
    }
    cbat_list_free(classes);
    cbat_array_free(x);
    cbat_array_free(y);
    cbat_array_free(query);
}
CBAT_TEST(test_rank_constant_empty_and_extreme_inputs) {
    size_t shape[] = {4, 1}, ys = 4, empty_shape[] = {3, 0};
    double xv[] = {0, 1e300, 2e300, 3e300}, yv[] = {3, 5, 7, 9}, intercept = 99;
    CBatArray *x = NULL, *y = NULL, *out = NULL;
    CBatList *one = NULL, *two = NULL;
    CBatRandom rng;
    CBAT_REQUIRE_OK(cbat_array_from_values(2, shape, xv, &x));
    CBAT_REQUIRE_OK(cbat_array_from_values(1, &ys, yv, &y));
    CBAT_REQUIRE_OK(cbat_ml_linear_regression_fit(x, y, &out, &intercept));
    CBAT_CHECK_NEAR(cbat_array_data(out)[0] / 1e-300, 2, 1e-12);
    CBAT_CHECK_NEAR(intercept, 3, 1e-12);
    cbat_array_free(out);
    out = NULL;
    cbat_array_data(x)[0] = cbat_array_data(x)[1] = cbat_array_data(x)[2] = cbat_array_data(x)[3] =
        DBL_MAX;
    CBAT_CHECK_ERR(cbat_ml_linear_regression_fit(x, y, &out, &intercept), CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_NULL(out);
    CBAT_REQUIRE_OK(cbat_ml_normalize_columns(x));
    CBAT_CHECK_NEAR(cbat_array_data(x)[0], 0, 0);
    cbat_array_data(x)[0] = NAN;
    CBAT_CHECK_ERR(cbat_ml_normalize_columns(x), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK(isnan(cbat_array_data(x)[0]));
    cbat_array_free(x);
    cbat_array_free(y);
    x = NULL;
    y = NULL;
    CBAT_REQUIRE_OK(cbat_array_new(2, empty_shape, &x));
    ys = 3;
    CBAT_REQUIRE_OK(cbat_array_from_values(1, &ys, yv, &y));
    CBAT_REQUIRE_OK(cbat_ml_linear_regression_fit(x, y, &out, &intercept));
    CBAT_CHECK_EQ_INT(cbat_array_size(out), 0);
    CBAT_CHECK_NEAR(intercept, 5, 1e-12);
    cbat_array_free(out);
    out = NULL;
    cbat_random_seed(&rng, 1);
    CBAT_REQUIRE_OK(cbat_ml_train_test_split(x, 0, &rng, &one, &two));
    CBAT_CHECK_EQ_INT(one->length, 3);
    CBAT_CHECK_EQ_INT(two->length, 0);
    cbat_list_free(one);
    cbat_list_free(two);
    one = NULL;
    two = NULL;
    CBAT_REQUIRE_OK(cbat_ml_train_test_split(x, 1, &rng, &one, &two));
    CBAT_CHECK_EQ_INT(one->length, 0);
    CBAT_CHECK_EQ_INT(two->length, 3);
    cbat_list_free(one);
    cbat_list_free(two);
    cbat_array_free(x);
    cbat_array_free(y);
}
CBAT_TEST(test_identical_clusters_and_tiny_knn) {
    size_t shape[] = {3, 2}, qs = 2, i;
    double values[] = {1e300, 0, 1e300, 1e-300, 1e300, 2e-300}, q[] = {1e300, 1.1e-300};
    CBatArray *x = NULL, *query = NULL, *centers = NULL;
    CBatList *classes = cbat_list_new(sizeof(int)), *labels = NULL;
    CBatRandom rng;
    int label = 99;
    CBAT_REQUIRE_NOT_NULL(classes);
    for (i = 0; i < 3; i++) {
        int value = (int)i;
        CBAT_REQUIRE_OK(cbat_list_push(classes, &value));
    }
    CBAT_REQUIRE_OK(cbat_array_from_values(2, shape, values, &x));
    CBAT_REQUIRE_OK(cbat_array_from_values(1, &qs, q, &query));
    CBAT_REQUIRE_OK(cbat_ml_knn_classify(x, classes, query, 1, &label));
    CBAT_CHECK_EQ_INT(label, 1);
    for (i = 0; i < 6; i++)
        cbat_array_data(x)[i] = 5;
    cbat_random_seed(&rng, 42);
    CBAT_REQUIRE_OK(cbat_ml_kmeans(x, 3, &rng, 1000, &centers, &labels));
    for (i = 0; i < 6; i++)
        CBAT_CHECK_NEAR(cbat_array_data(centers)[i], 5, 0);
    for (i = 0; i < 3; i++)
        CBAT_CHECK_EQ_INT(*(size_t *)cbat_list_get(labels, i), 0);
    cbat_array_free(x);
    cbat_array_free(query);
    cbat_array_free(centers);
    cbat_list_free(classes);
    cbat_list_free(labels);
}
static const CBatTestCase tests[] = {CBAT_TEST_ENTRY(test_regression_and_normalization),
                                     CBAT_TEST_ENTRY(test_knn_ties_and_split),
                                     CBAT_TEST_ENTRY(test_kmeans_reproducibility_and_dynamic_range),
                                     CBAT_TEST_ENTRY(test_errors_and_all_oom_paths),
                                     CBAT_TEST_ENTRY(test_rank_constant_empty_and_extreme_inputs),
                                     CBAT_TEST_ENTRY(test_identical_clusters_and_tiny_knn)};
CBAT_TEST_MAIN("ml", tests)
