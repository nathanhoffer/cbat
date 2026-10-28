#include "cbat/set.h"
#include "cbat/test.h"

static CBatSet *make_set(const char *const *items, size_t count) {
    CBatSet *set = cbat_set_new();
    size_t i;
    for (i = 0; set != NULL && i < count; i++) {
        if (cbat_set_add(set, items[i]) != CBAT_OK) {
            cbat_set_free(set);
            return NULL;
        }
    }
    return set;
}

CBAT_TEST(test_add_contains_remove) {
    CBatSet *set = cbat_set_new();
    CBAT_REQUIRE_NOT_NULL(set);
    CBAT_CHECK_OK(cbat_set_add(set, "a"));
    CBAT_CHECK_OK(cbat_set_add(set, "a"));
    CBAT_CHECK_OK(cbat_set_add(set, "b"));
    CBAT_CHECK_EQ_SIZE(cbat_set_length(set), 2);
    CBAT_CHECK(cbat_set_contains(set, "a"));
    CBAT_CHECK(!cbat_set_contains(set, "c"));
    CBAT_CHECK_OK(cbat_set_remove(set, "a"));
    CBAT_CHECK_ERR(cbat_set_remove(set, "a"), CBAT_ERR_NOT_FOUND);
    cbat_set_discard(set, "a");
    cbat_set_discard(set, "b");
    CBAT_CHECK_EQ_SIZE(cbat_set_length(set), 0);
    CBAT_CHECK_ERR(cbat_set_add(set, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_set_add(NULL, "x"), CBAT_ERR_INVALID_ARGUMENT);
    cbat_set_free(set);
    cbat_set_free(NULL);
}

CBAT_TEST(test_iteration_order_and_items) {
    const char *items[] = {"pear", "apple", "fig"};
    CBatSet *set = make_set(items, 3);
    CBatList *list = NULL;
    size_t it = 0;
    const char *item;
    int i = 0;
    CBAT_REQUIRE_NOT_NULL(set);
    while (cbat_set_next(set, &it, &item)) {
        CBAT_CHECK_EQ_STR(item, items[i]);
        i++;
    }
    CBAT_CHECK_EQ_INT(i, 3);
    CBAT_REQUIRE_OK(cbat_set_items(set, &list));
    CBAT_CHECK_EQ_STR(CBAT_LIST_GET(list, const char *, 2), "fig");
    cbat_list_free(list);
    cbat_set_clear(set);
    CBAT_CHECK_EQ_SIZE(cbat_set_length(set), 0);
    cbat_set_free(set);
}

CBAT_TEST(test_union_intersection_difference) {
    const char *xs[] = {"a", "b", "c"};
    const char *ys[] = {"b", "c", "d"};
    CBatSet *x = make_set(xs, 3);
    CBatSet *y = make_set(ys, 3);
    CBatSet *result = NULL;
    CBAT_REQUIRE_NOT_NULL(x);
    CBAT_REQUIRE_NOT_NULL(y);

    CBAT_REQUIRE_OK(cbat_set_union(x, y, &result));
    CBAT_CHECK_EQ_SIZE(cbat_set_length(result), 4);
    CBAT_CHECK(cbat_set_contains(result, "d"));
    cbat_set_free(result);

    CBAT_REQUIRE_OK(cbat_set_intersection(x, y, &result));
    CBAT_CHECK_EQ_SIZE(cbat_set_length(result), 2);
    CBAT_CHECK(cbat_set_contains(result, "b"));
    CBAT_CHECK(!cbat_set_contains(result, "a"));
    cbat_set_free(result);

    CBAT_REQUIRE_OK(cbat_set_difference(x, y, &result));
    CBAT_CHECK_EQ_SIZE(cbat_set_length(result), 1);
    CBAT_CHECK(cbat_set_contains(result, "a"));
    cbat_set_free(result);

    CBAT_CHECK_ERR(cbat_set_union(x, NULL, &result), CBAT_ERR_INVALID_ARGUMENT);
    cbat_set_free(x);
    cbat_set_free(y);
}

CBAT_TEST(test_subset_and_equals) {
    const char *small[] = {"a"};
    const char *big[] = {"b", "a"};
    const char *same[] = {"a", "b"};
    CBatSet *s = make_set(small, 1);
    CBatSet *b = make_set(big, 2);
    CBatSet *t = make_set(same, 2);
    CBAT_REQUIRE_NOT_NULL(s);
    CBAT_REQUIRE_NOT_NULL(b);
    CBAT_REQUIRE_NOT_NULL(t);
    CBAT_CHECK(cbat_set_is_subset(s, b));
    CBAT_CHECK(!cbat_set_is_subset(b, s));
    CBAT_CHECK(cbat_set_equals(b, t));
    CBAT_CHECK(!cbat_set_equals(s, t));
    CBAT_CHECK(!cbat_set_is_subset(NULL, t));
    cbat_set_free(s);
    cbat_set_free(b);
    cbat_set_free(t);
}

CBAT_TEST(test_copy_and_symmetric_difference) {
    const char *xs[] = {"a", "b", "c"};
    const char *ys[] = {"b", "d"};
    CBatSet *a = make_set(xs, 3), *b = make_set(ys, 2), *copy = NULL, *result = NULL;
    size_t it = 0;
    const char *item;
    CBAT_REQUIRE_NOT_NULL(a); CBAT_REQUIRE_NOT_NULL(b);
    CBAT_REQUIRE_OK(cbat_set_copy(a, &copy));
    CBAT_CHECK(cbat_set_equals(copy, a));
    CBAT_REQUIRE_OK(cbat_set_remove(copy, "a"));
    CBAT_CHECK(cbat_set_contains(a, "a"));
    CBAT_REQUIRE_OK(cbat_set_symmetric_difference(a, b, &result));
    CBAT_CHECK_EQ_SIZE(cbat_set_length(result), 3);
    CBAT_REQUIRE(cbat_set_next(result, &it, &item)); CBAT_CHECK_EQ_STR(item, "a");
    CBAT_REQUIRE(cbat_set_next(result, &it, &item)); CBAT_CHECK_EQ_STR(item, "c");
    CBAT_REQUIRE(cbat_set_next(result, &it, &item)); CBAT_CHECK_EQ_STR(item, "d");
    cbat_set_free(result); result = NULL;
    CBAT_REQUIRE_OK(cbat_set_symmetric_difference(a, a, &result));
    CBAT_CHECK_EQ_SIZE(cbat_set_length(result), 0);
    cbat_set_free(result); result = NULL;
    CBAT_CHECK_ERR(cbat_set_copy(NULL, &result), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_set_copy(a, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_set_symmetric_difference(NULL, b, &result), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_set_symmetric_difference(a, NULL, &result), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_set_symmetric_difference(a, b, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_NULL(result);
    cbat_set_free(copy); cbat_set_free(b); cbat_set_free(a);
}

CBAT_TEST(test_copy_and_symmetric_difference_oom) {
    const char *xs[] = {"a", "b", "c"}, *ys[] = {"b", "d"};
    CBatSet *a = make_set(xs, 3), *b = make_set(ys, 2);
    size_t allowed;
    bool copied = false, combined = false;
    CBAT_REQUIRE_NOT_NULL(a); CBAT_REQUIRE_NOT_NULL(b);
    for (allowed = 0; allowed < 40; allowed++) {
        CBatSet *result = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_set_copy(a, &result);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) { copied = true; CBAT_CHECK(cbat_set_equals(a, result)); }
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(result); }
        cbat_set_free(result); result = NULL;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_set_symmetric_difference(a, b, &result);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) { combined = true; CBAT_CHECK_EQ_SIZE(cbat_set_length(result), 3); }
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(result); }
        cbat_set_free(result);
        if (copied && combined) break;
    }
    CBAT_CHECK(copied && combined);
    cbat_set_free(b); cbat_set_free(a);
}

CBAT_TEST(test_out_of_memory) {
    size_t n;
    const char *xs[] = {"a", "b", "c", "d", "e"};
    for (n = 0; n < 10; n++) {
        CBatSet *x;
        CBatSet *result = NULL;
        CBatError err;
        x = make_set(xs, 5);
        if (x == NULL) {
            continue;
        }
        cbat_test_fail_allocations_after(n);
        err = cbat_set_union(x, x, &result);
        cbat_test_stop_failing_allocations();
        CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
        if (err == CBAT_OK) {
            CBAT_CHECK_EQ_SIZE(cbat_set_length(result), 5);
            cbat_set_free(result);
        }
        cbat_set_free(x);
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_add_contains_remove),
    CBAT_TEST_ENTRY(test_iteration_order_and_items),
    CBAT_TEST_ENTRY(test_union_intersection_difference),
    CBAT_TEST_ENTRY(test_subset_and_equals),
    CBAT_TEST_ENTRY(test_copy_and_symmetric_difference),
    CBAT_TEST_ENTRY(test_copy_and_symmetric_difference_oom),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("set", tests)
