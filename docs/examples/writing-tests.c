/*
 * Recipe 13: Writing your own tests
 *
 * PROBLEM
 *   Test a small application helper, including invalid input and allocation
 *   failure, and make the executable return failure if a check fails.
 *
 * SOLUTION
 *   Declare cases with CBAT_TEST, register them with CBAT_TEST_ENTRY, and
 *   use CBAT_TEST_MAIN for a runner with filtering and leak detection.
 *
 * DISCUSSION
 *   CHECK records a failure and continues; REQUIRE returns immediately.
 *   After acquiring resources, use CHECK with explicit cleanup rather than
 *   an early REQUIRE that could leak. Allocation failure injection applies
 *   to cbat allocations in the current test; disable it before continuing.
 *   Run this executable with --list to list cases, or pass a substring such
 *   as allocation to run only matching cases. Every test must free what it
 *   owns: the runner reports outstanding cbat allocations as failures.
 */
#include "cbat/cbat.h"

/* On failure, leave the caller's output unchanged. */
static CBatError make_squares(int count, CBatList **out) {
    CBatList *values;
    int i;
    if (out == NULL || count < 0 || count > 100) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    values = cbat_list_new(sizeof(int));
    if (values == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    for (i = 0; i < count; i++) {
        CBatError err = CBAT_LIST_PUSH(values, int, i * i);
        if (err != CBAT_OK) {
            cbat_list_free(values);
            return err;
        }
    }
    *out = values;
    return CBAT_OK;
}

CBAT_TEST(test_squares) {
    CBatList *values = NULL;
    CBAT_REQUIRE_OK(make_squares(5, &values));
    if (CBAT_CHECK_EQ_SIZE(cbat_list_length(values), 5)) {
        CBAT_CHECK_EQ_INT(CBAT_LIST_GET(values, int, 0), 0);
        CBAT_CHECK_EQ_INT(CBAT_LIST_GET(values, int, 4), 16);
    }
    cbat_list_free(values);
}

CBAT_TEST(test_invalid_input) {
    CBatList *values = NULL;
    CBAT_CHECK_ERR(make_squares(-1, &values), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_NULL(values);
    CBAT_CHECK_ERR(make_squares(101, &values), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_NULL(values);
    CBAT_CHECK_ERR(make_squares(5, NULL), CBAT_ERR_INVALID_ARGUMENT);
    cbat_list_free(values);
}

CBAT_TEST(test_allocation_failures) {
    size_t allowed;
    bool completed = false;
    /* Sweep successive failure points, including growth after construction.
     * The small bound exceeds the allocations needed for 100 integers. */
    for (allowed = 0; allowed < 32; allowed++) {
        CBatList *values = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(allowed);
        err = make_squares(100, &values);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) {
            CBAT_CHECK_EQ_SIZE(cbat_list_length(values), 100);
            completed = true;
        } else {
            CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
            CBAT_CHECK_NULL(values);
        }
        cbat_list_free(values);
        CBAT_CHECK_EQ_SIZE(cbat_test_live_allocations(), 0);
        if (completed) {
            break;
        }
    }
    CBAT_CHECK(completed);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_squares),
    CBAT_TEST_ENTRY(test_invalid_input),
    CBAT_TEST_ENTRY(test_allocation_failures)
};

CBAT_TEST_MAIN("squares", tests)
