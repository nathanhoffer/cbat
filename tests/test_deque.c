#include "cbat/deque.h"
#include "cbat/test.h"

CBAT_TEST(test_push_pop_both_ends) {
    CBatDeque *q = cbat_deque_new(sizeof(int));
    int out = 0;
    CBAT_REQUIRE_NOT_NULL(q);
    CBAT_CHECK_OK(CBAT_DEQUE_PUSH_BACK(q, int, 2));
    CBAT_CHECK_OK(CBAT_DEQUE_PUSH_BACK(q, int, 3));
    CBAT_CHECK_OK(CBAT_DEQUE_PUSH_FRONT(q, int, 1));
    CBAT_CHECK_OK(CBAT_DEQUE_PUSH_FRONT(q, int, 0));
    CBAT_REQUIRE_EQ_SIZE(cbat_deque_length(q), 4);
    CBAT_CHECK_EQ_INT(*(int *)cbat_deque_front(q), 0);
    CBAT_CHECK_EQ_INT(*(int *)cbat_deque_back(q), 3);
    CBAT_CHECK_EQ_INT(*(int *)cbat_deque_get(q, 2), 2);
    CBAT_CHECK_NULL(cbat_deque_get(q, 4));
    CBAT_CHECK_OK(cbat_deque_pop_front(q, &out));
    CBAT_CHECK_EQ_INT(out, 0);
    CBAT_CHECK_OK(cbat_deque_pop_back(q, &out));
    CBAT_CHECK_EQ_INT(out, 3);
    CBAT_CHECK_EQ_SIZE(cbat_deque_length(q), 2);
    cbat_deque_free(q);
}

CBAT_TEST(test_empty_behaviour) {
    CBatDeque *q = cbat_deque_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(q);
    CBAT_CHECK(cbat_deque_is_empty(q));
    CBAT_CHECK_ERR(cbat_deque_pop_front(q, NULL), CBAT_ERR_EMPTY);
    CBAT_CHECK_ERR(cbat_deque_pop_back(q, NULL), CBAT_ERR_EMPTY);
    CBAT_CHECK_NULL(cbat_deque_front(q));
    CBAT_CHECK_NULL(cbat_deque_back(q));
    CBAT_CHECK_NULL(cbat_deque_new(0));
    CBAT_CHECK_ERR(cbat_deque_push_back(NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    cbat_deque_free(q);
    cbat_deque_free(NULL);
}

CBAT_TEST(test_wraparound_as_queue) {
    CBatDeque *q = cbat_deque_new(sizeof(int));
    int i;
    int expected = 0;
    int out;
    CBAT_REQUIRE_NOT_NULL(q);
    /* Interleave so head walks around the ring many times while growing. */
    for (i = 0; i < 10000; i++) {
        CBAT_REQUIRE_OK(CBAT_DEQUE_PUSH_BACK(q, int, i));
        if (i % 3 == 0) {
            CBAT_REQUIRE_OK(cbat_deque_pop_front(q, &out));
            CBAT_REQUIRE_EQ_INT(out, expected);
            expected++;
        }
    }
    while (cbat_deque_pop_front(q, &out) == CBAT_OK) {
        CBAT_REQUIRE_EQ_INT(out, expected);
        expected++;
    }
    CBAT_CHECK_EQ_INT(expected, 10000);
    cbat_deque_free(q);
}

CBAT_TEST(test_push_front_many_and_get) {
    CBatDeque *q = cbat_deque_new(sizeof(int));
    int i;
    CBAT_REQUIRE_NOT_NULL(q);
    for (i = 0; i < 100; i++) {
        CBAT_REQUIRE_OK(CBAT_DEQUE_PUSH_FRONT(q, int, i));
    }
    for (i = 0; i < 100; i++) {
        CBAT_REQUIRE_EQ_INT(*(int *)cbat_deque_get(q, (size_t)i), 99 - i);
    }
    cbat_deque_free(q);
}

CBAT_TEST(test_push_back_from_own_storage) {
    CBatDeque *q = cbat_deque_new(sizeof(int));
    int i;
    CBAT_REQUIRE_NOT_NULL(q);
    CBAT_DEQUE_PUSH_BACK(q, int, 5);
    for (i = 0; i < 40; i++) {
        CBAT_REQUIRE_OK(cbat_deque_push_back(q, cbat_deque_front(q)));
    }
    CBAT_CHECK_EQ_INT(*(int *)cbat_deque_back(q), 5);
    cbat_deque_free(q);
}

static int destroyed = 0;

static void count_destroy(void *element) {
    CBAT_UNUSED(element);
    destroyed++;
}

CBAT_TEST(test_destructor_and_clear) {
    CBatDeque *q = cbat_deque_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(q);
    destroyed = 0;
    cbat_deque_set_destructor(q, count_destroy);
    CBAT_DEQUE_PUSH_BACK(q, int, 1);
    CBAT_DEQUE_PUSH_BACK(q, int, 2);
    CBAT_DEQUE_PUSH_BACK(q, int, 3);
    cbat_deque_pop_front(q, NULL);
    CBAT_CHECK_EQ_INT(destroyed, 1);
    cbat_deque_clear(q);
    CBAT_CHECK_EQ_INT(destroyed, 3);
    CBAT_DEQUE_PUSH_BACK(q, int, 4);
    cbat_deque_free(q);
    CBAT_CHECK_EQ_INT(destroyed, 4);
}

CBAT_TEST(test_reserve_and_out_of_memory) {
    CBatDeque *q = cbat_deque_new(sizeof(int));
    size_t n;
    CBAT_REQUIRE_NOT_NULL(q);
    CBAT_CHECK_OK(cbat_deque_reserve(q, 100));
    CBAT_CHECK_ERR(cbat_deque_reserve(q, SIZE_MAX), CBAT_ERR_OVERFLOW);
    cbat_deque_free(q);
    for (n = 0; n < 3; n++) {
        int i;
        CBatError err = CBAT_OK;
        cbat_test_fail_allocations_after(n);
        q = cbat_deque_new(sizeof(int));
        if (q == NULL) {
            cbat_test_stop_failing_allocations();
            continue;
        }
        for (i = 0; i < 100 && err == CBAT_OK; i++) {
            err = CBAT_DEQUE_PUSH_BACK(q, int, i);
        }
        cbat_test_stop_failing_allocations();
        CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
        cbat_deque_free(q);
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_push_pop_both_ends),
    CBAT_TEST_ENTRY(test_empty_behaviour),
    CBAT_TEST_ENTRY(test_wraparound_as_queue),
    CBAT_TEST_ENTRY(test_push_front_many_and_get),
    CBAT_TEST_ENTRY(test_push_back_from_own_storage),
    CBAT_TEST_ENTRY(test_destructor_and_clear),
    CBAT_TEST_ENTRY(test_reserve_and_out_of_memory),
};

CBAT_TEST_MAIN("deque", tests)
