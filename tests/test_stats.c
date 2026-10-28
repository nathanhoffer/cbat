#include "cbat/stats.h"
#include "cbat/test.h"

static const double data[] = {2, 4, 4, 4, 5, 5, 7, 9};

CBAT_TEST(test_sum_is_compensated) {
    double values[] = {1e16, 1.0, -1e16};
    CBAT_CHECK_NEAR(cbat_stats_sum(values, 3), 1.0, 0.0);
    CBAT_CHECK_NEAR(cbat_stats_sum(data, 8), 40.0, 0.0);
    CBAT_CHECK_NEAR(cbat_stats_sum(NULL, 5), 0.0, 0.0);
}

CBAT_TEST(test_mean_min_max) {
    double out = 0;
    CBAT_CHECK_OK(cbat_stats_mean(data, 8, &out));
    CBAT_CHECK_NEAR(out, 5.0, 1e-12);
    CBAT_CHECK_OK(cbat_stats_min(data, 8, &out));
    CBAT_CHECK_NEAR(out, 2.0, 0);
    CBAT_CHECK_OK(cbat_stats_max(data, 8, &out));
    CBAT_CHECK_NEAR(out, 9.0, 0);
    CBAT_CHECK_ERR(cbat_stats_mean(data, 0, &out), CBAT_ERR_EMPTY);
    CBAT_CHECK_ERR(cbat_stats_mean(NULL, 3, &out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_stats_mean(data, 3, NULL), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_median_and_percentile) {
    double odd[] = {3, 1, 2};
    double out = 0;
    CBAT_CHECK_OK(cbat_stats_median(odd, 3, &out));
    CBAT_CHECK_NEAR(out, 2.0, 0);
    CBAT_CHECK_NEAR(odd[0], 3.0, 0); /* input untouched */
    CBAT_CHECK_OK(cbat_stats_median(data, 8, &out));
    CBAT_CHECK_NEAR(out, 4.5, 1e-12);
    CBAT_CHECK_OK(cbat_stats_percentile(data, 8, 0, &out));
    CBAT_CHECK_NEAR(out, 2.0, 0);
    CBAT_CHECK_OK(cbat_stats_percentile(data, 8, 100, &out));
    CBAT_CHECK_NEAR(out, 9.0, 0);
    CBAT_CHECK_OK(cbat_stats_percentile(data, 8, 25, &out));
    CBAT_CHECK_NEAR(out, 4.0, 1e-12);
    CBAT_CHECK_ERR(cbat_stats_percentile(data, 8, 101, &out), CBAT_ERR_OUT_OF_RANGE);
    cbat_test_fail_allocations_after(0);
    CBAT_CHECK_ERR(cbat_stats_median(data, 8, &out), CBAT_ERR_NO_MEMORY);
    cbat_test_stop_failing_allocations();
}

CBAT_TEST(test_variance_and_stdev) {
    double out = 0;
    double one = 3.0;
    CBAT_CHECK_OK(cbat_stats_pvariance(data, 8, &out));
    CBAT_CHECK_NEAR(out, 4.0, 1e-12);
    CBAT_CHECK_OK(cbat_stats_pstdev(data, 8, &out));
    CBAT_CHECK_NEAR(out, 2.0, 1e-12);
    CBAT_CHECK_OK(cbat_stats_variance(data, 8, &out));
    CBAT_CHECK_NEAR(out, 32.0 / 7.0, 1e-12);
    CBAT_CHECK_OK(cbat_stats_stdev(data, 8, &out));
    CBAT_CHECK_NEAR(out * out, 32.0 / 7.0, 1e-12);
    CBAT_CHECK_ERR(cbat_stats_variance(&one, 1, &out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_OK(cbat_stats_pvariance(&one, 1, &out));
    CBAT_CHECK_NEAR(out, 0.0, 0);
}

CBAT_TEST(test_variance_is_numerically_stable) {
    double shifted[] = {1e9 + 4, 1e9 + 7, 1e9 + 13, 1e9 + 16};
    double out = 0;
    CBAT_CHECK_OK(cbat_stats_variance(shifted, 4, &out));
    CBAT_CHECK_NEAR(out, 30.0, 1e-6);
}

CBAT_TEST(test_correlation_and_regression) {
    double x[] = {1, 2, 3, 4, 5};
    double y[] = {3, 5, 7, 9, 11}; /* y = 2x + 1 */
    double flat[] = {1, 1, 1, 1, 1};
    double r = 0;
    double slope = 0;
    double intercept = 0;
    CBAT_CHECK_OK(cbat_stats_correlation(x, y, 5, &r));
    CBAT_CHECK_NEAR(r, 1.0, 1e-12);
    CBAT_CHECK_OK(cbat_stats_linear_regression(x, y, 5, &slope, &intercept));
    CBAT_CHECK_NEAR(slope, 2.0, 1e-12);
    CBAT_CHECK_NEAR(intercept, 1.0, 1e-12);
    CBAT_CHECK_ERR(cbat_stats_correlation(x, flat, 5, &r), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_stats_linear_regression(flat, y, 5, &slope, &intercept),
                   CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_stats_correlation(x, y, 1, &r), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_stats_correlation(x, y, 0, &r), CBAT_ERR_EMPTY);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_sum_is_compensated),
    CBAT_TEST_ENTRY(test_mean_min_max),
    CBAT_TEST_ENTRY(test_median_and_percentile),
    CBAT_TEST_ENTRY(test_variance_and_stdev),
    CBAT_TEST_ENTRY(test_variance_is_numerically_stable),
    CBAT_TEST_ENTRY(test_correlation_and_regression),
};

CBAT_TEST_MAIN("stats", tests)
