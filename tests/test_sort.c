#include <math.h>
#include <string.h>

#include "cbat/sort.h"
#include "cbat/test.h"

typedef struct Pair {
    int key;
    int order;
} Pair;

static int compare_pair_key(const void *a, const void *b) {
    return cbat_compare_int(&((const Pair *)a)->key, &((const Pair *)b)->key);
}

CBAT_TEST(test_sort_small_ints) {
    int values[] = {5, 3, 9, 1, 1, -4};
    int expected[] = {-4, 1, 1, 3, 5, 9};
    CBAT_REQUIRE_OK(cbat_sort(values, 6, sizeof(int), cbat_compare_int));
    CBAT_CHECK_EQ_MEM(values, expected, sizeof(expected));
}

CBAT_TEST(test_sort_large_is_stable) {
    Pair pairs[1000];
    size_t i;
    for (i = 0; i < 1000; i++) {
        pairs[i].key = (int)((i * 7919u) % 13u);
        pairs[i].order = (int)i;
    }
    CBAT_REQUIRE_OK(cbat_sort(pairs, 1000, sizeof(Pair), compare_pair_key));
    for (i = 1; i < 1000; i++) {
        CBAT_REQUIRE(pairs[i - 1].key <= pairs[i].key);
        if (pairs[i - 1].key == pairs[i].key) {
            CBAT_REQUIRE(pairs[i - 1].order < pairs[i].order);
        }
    }
}

CBAT_TEST(test_sort_already_sorted_and_reversed) {
    int values[100];
    int i;
    for (i = 0; i < 100; i++) {
        values[i] = 100 - i;
    }
    CBAT_REQUIRE_OK(cbat_sort(values, 100, sizeof(int), cbat_compare_int));
    CBAT_CHECK(cbat_is_sorted(values, 100, sizeof(int), cbat_compare_int));
    CBAT_REQUIRE_OK(cbat_sort(values, 100, sizeof(int), cbat_compare_int));
    CBAT_CHECK_EQ_INT(values[0], 1);
    CBAT_CHECK_EQ_INT(values[99], 100);
}

CBAT_TEST(test_sort_edge_cases) {
    int one = 1;
    CBAT_CHECK_OK(cbat_sort(NULL, 0, sizeof(int), cbat_compare_int));
    CBAT_CHECK_OK(cbat_sort(&one, 1, sizeof(int), cbat_compare_int));
    CBAT_CHECK_ERR(cbat_sort(NULL, 3, sizeof(int), cbat_compare_int), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_sort(&one, 1, 0, cbat_compare_int), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_sort(&one, 1, sizeof(int), NULL), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_sort_out_of_memory_leaves_array_unchanged) {
    int values[40];
    int copy[40];
    int i;
    for (i = 0; i < 40; i++) {
        values[i] = 40 - i;
    }
    memcpy(copy, values, sizeof(values));
    cbat_test_fail_allocations_after(0);
    CBAT_CHECK_ERR(cbat_sort(values, 40, sizeof(int), cbat_compare_int), CBAT_ERR_NO_MEMORY);
    cbat_test_stop_failing_allocations();
    CBAT_CHECK_EQ_MEM(values, copy, sizeof(values));
}

CBAT_TEST(test_reverse_and_swap) {
    int values[] = {1, 2, 3, 4, 5};
    int expected[] = {5, 4, 3, 2, 1};
    int a = 1;
    int b = 2;
    cbat_reverse(values, 5, sizeof(int));
    CBAT_CHECK_EQ_MEM(values, expected, sizeof(expected));
    cbat_reverse(NULL, 0, sizeof(int));
    cbat_swap(&a, &b, sizeof(int));
    CBAT_CHECK_EQ_INT(a, 2);
    CBAT_CHECK_EQ_INT(b, 1);
}

CBAT_TEST(test_bounds_and_binary_search) {
    int values[] = {1, 3, 3, 3, 7, 9};
    int key = 3;
    int missing = 4;
    int big = 100;
    CBAT_CHECK_EQ_SIZE(cbat_lower_bound(values, 6, sizeof(int), &key, cbat_compare_int), 1);
    CBAT_CHECK_EQ_SIZE(cbat_upper_bound(values, 6, sizeof(int), &key, cbat_compare_int), 4);
    CBAT_CHECK_EQ_SIZE(cbat_lower_bound(values, 6, sizeof(int), &big, cbat_compare_int), 6);
    CBAT_CHECK_EQ_SIZE(cbat_binary_search(values, 6, sizeof(int), &key, cbat_compare_int), 1);
    CBAT_CHECK_EQ_SIZE(cbat_binary_search(values, 6, sizeof(int), &missing, cbat_compare_int), CBAT_NPOS);
    CBAT_CHECK_EQ_SIZE(cbat_binary_search(values, 0, sizeof(int), &key, cbat_compare_int), CBAT_NPOS);
}

CBAT_TEST(test_is_sorted) {
    int up[] = {1, 2, 2, 3};
    int down[] = {3, 1};
    CBAT_CHECK(cbat_is_sorted(up, 4, sizeof(int), cbat_compare_int));
    CBAT_CHECK(!cbat_is_sorted(down, 2, sizeof(int), cbat_compare_int));
    CBAT_CHECK(cbat_is_sorted(NULL, 0, sizeof(int), cbat_compare_int));
}

CBAT_TEST(test_comparators) {
    int64_t i64[] = {INT64_MIN, 5};
    uint64_t u64[] = {1, UINT64_MAX};
    size_t sz[] = {2, 1};
    double d[] = {1.5, NAN, -2.0};
    const char *s[] = {"pear", NULL, "apple"};
    CBAT_CHECK(cbat_compare_int64(&i64[0], &i64[1]) < 0);
    CBAT_CHECK(cbat_compare_uint64(&u64[1], &u64[0]) > 0);
    CBAT_CHECK(cbat_compare_size(&sz[0], &sz[1]) > 0);
    CBAT_CHECK(cbat_compare_size(&sz[0], &sz[0]) == 0);
    CBAT_REQUIRE_OK(cbat_sort(d, 3, sizeof(double), cbat_compare_double));
    CBAT_CHECK_NEAR(d[0], -2.0, 0.0);
    CBAT_CHECK_NEAR(d[1], 1.5, 0.0);
    CBAT_CHECK(d[2] != d[2]); /* NaN last */
    CBAT_REQUIRE_OK(cbat_sort((void *)s, 3, sizeof(const char *), cbat_compare_cstring));
    CBAT_CHECK_NULL(s[0]);
    CBAT_CHECK_EQ_STR(s[1], "apple");
    CBAT_CHECK_EQ_STR(s[2], "pear");
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_sort_small_ints),
    CBAT_TEST_ENTRY(test_sort_large_is_stable),
    CBAT_TEST_ENTRY(test_sort_already_sorted_and_reversed),
    CBAT_TEST_ENTRY(test_sort_edge_cases),
    CBAT_TEST_ENTRY(test_sort_out_of_memory_leaves_array_unchanged),
    CBAT_TEST_ENTRY(test_reverse_and_swap),
    CBAT_TEST_ENTRY(test_bounds_and_binary_search),
    CBAT_TEST_ENTRY(test_is_sorted),
    CBAT_TEST_ENTRY(test_comparators),
};

CBAT_TEST_MAIN("sort", tests)
