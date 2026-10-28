/* Tests for the test framework itself, using nested silent runs. */
#include <string.h>

#include "cbat/test.h"

CBAT_TEST(inner_passing) {
    CBAT_CHECK(1 + 1 == 2);
    CBAT_CHECK_EQ_INT(-3, -3);
    CBAT_CHECK_EQ_UINT(7u, 7u);
    CBAT_CHECK_EQ_STR("a", "a");
    CBAT_CHECK_EQ_STR(NULL, NULL);
    CBAT_CHECK_EQ_MEM("abc", "abc", 3);
    CBAT_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-12);
    CBAT_CHECK_OK(CBAT_OK);
    CBAT_CHECK_ERR(CBAT_ERR_EMPTY, CBAT_ERR_EMPTY);
}

CBAT_TEST(inner_failing_int) {
    CBAT_CHECK_EQ_INT(1, 2);
}

CBAT_TEST(inner_failing_str) {
    CBAT_CHECK_EQ_STR("a", NULL);
}

CBAT_TEST(inner_failing_mem) {
    CBAT_CHECK_EQ_MEM("abc", "abd", 3);
}

CBAT_TEST(inner_failing_near) {
    CBAT_CHECK_NEAR(1.0, 1.1, 0.01);
}

CBAT_TEST(inner_failing_err) {
    CBAT_CHECK_OK(CBAT_ERR_IO);
}

static int after_require_reached = 0;

CBAT_TEST(inner_require_stops) {
    CBAT_REQUIRE(false);
    after_require_reached = 1;
}

static void *leaked_block = NULL;

CBAT_TEST(inner_leaks) {
    /* Deliberately not released inside the test; the outer test frees it. */
    leaked_block = cbat_alloc(NULL, 10);
}

static const CBatTestCase inner_cases[] = {
    CBAT_TEST_ENTRY(inner_passing),
    CBAT_TEST_ENTRY(inner_failing_int),
    CBAT_TEST_ENTRY(inner_failing_str),
    CBAT_TEST_ENTRY(inner_failing_mem),
    CBAT_TEST_ENTRY(inner_failing_near),
    CBAT_TEST_ENTRY(inner_failing_err),
    CBAT_TEST_ENTRY(inner_require_stops),
    CBAT_TEST_ENTRY(inner_leaks),
};

CBAT_TEST(test_run_counts_pass_and_fail) {
    CBatTestSummary s = cbat_test_run("inner", inner_cases, CBAT_ARRAY_LEN(inner_cases), NULL, NULL);
    CBAT_CHECK_EQ_SIZE(s.passed, 1);
    CBAT_CHECK_EQ_SIZE(s.failed, 7);
    CBAT_CHECK_EQ_SIZE(s.skipped, 0);
    CBAT_CHECK_EQ_INT(after_require_reached, 0);
    CBAT_CHECK_NOT_NULL(leaked_block);
    cbat_free(leaked_block); /* keep the outer test leak-free */
    leaked_block = NULL;
}

CBAT_TEST(test_run_filter) {
    CBatTestSummary s = cbat_test_run("inner", inner_cases, CBAT_ARRAY_LEN(inner_cases),
                                      "passing", NULL);
    CBAT_CHECK_EQ_SIZE(s.passed, 1);
    CBAT_CHECK_EQ_SIZE(s.failed, 0);
    CBAT_CHECK_EQ_SIZE(s.skipped, 7);
}

CBAT_TEST(test_live_allocation_count) {
    void *p;
    CBAT_CHECK_EQ_SIZE(cbat_test_live_allocations(), 0);
    p = cbat_alloc(NULL, 4);
    CBAT_CHECK_EQ_SIZE(cbat_test_live_allocations(), 1);
    cbat_free(p);
    CBAT_CHECK_EQ_SIZE(cbat_test_live_allocations(), 0);
}

CBAT_TEST(test_fail_allocations_after) {
    void *a;
    void *b;
    void *c;
    cbat_test_fail_allocations_after(2);
    a = cbat_alloc(NULL, 4);
    b = cbat_alloc(NULL, 4);
    c = cbat_alloc(NULL, 4);
    CBAT_CHECK_NOT_NULL(a);
    CBAT_CHECK_NOT_NULL(b);
    CBAT_CHECK_NULL(c);
    CBAT_CHECK_NULL(cbat_resize(NULL, a, 4, 8));
    cbat_test_stop_failing_allocations();
    c = cbat_alloc(NULL, 4);
    CBAT_CHECK_NOT_NULL(c);
    cbat_free(a);
    cbat_free(b);
    cbat_free(c);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_run_counts_pass_and_fail),
    CBAT_TEST_ENTRY(test_run_filter),
    CBAT_TEST_ENTRY(test_live_allocation_count),
    CBAT_TEST_ENTRY(test_fail_allocations_after),
};

CBAT_TEST_MAIN("test", tests)
