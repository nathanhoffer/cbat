#include <stdio.h>
#include <string.h>

#include "cbat/dict.h"
#include "cbat/string.h"
#include "cbat/test.h"

CBAT_TEST(test_set_get_contains) {
    CBatDict *d = cbat_dict_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(d);
    CBAT_CHECK_OK(CBAT_DICT_SET(d, "one", int, 1));
    CBAT_CHECK_OK(CBAT_DICT_SET(d, "two", int, 2));
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(d), 2);
    CBAT_CHECK_EQ_INT(*(int *)cbat_dict_get(d, "one"), 1);
    CBAT_CHECK_EQ_INT(CBAT_DICT_GET(d, "two", int, -1), 2);
    CBAT_CHECK_EQ_INT(CBAT_DICT_GET(d, "three", int, -1), -1);
    CBAT_CHECK_NULL(cbat_dict_get(d, "three"));
    CBAT_CHECK(cbat_dict_contains(d, "one"));
    CBAT_CHECK(!cbat_dict_contains(d, "ONE"));
    CBAT_CHECK_EQ_SIZE(cbat_dict_value_size(d), sizeof(int));
    cbat_dict_free(d);
}

CBAT_TEST(test_replace_keeps_order) {
    CBatDict *d = cbat_dict_new(sizeof(int));
    size_t it = 0;
    const char *key;
    void *value;
    CBAT_REQUIRE_NOT_NULL(d);
    CBAT_DICT_SET(d, "a", int, 1);
    CBAT_DICT_SET(d, "b", int, 2);
    CBAT_DICT_SET(d, "a", int, 10);
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(d), 2);
    CBAT_REQUIRE(cbat_dict_next(d, &it, &key, &value));
    CBAT_CHECK_EQ_STR(key, "a");
    CBAT_CHECK_EQ_INT(*(int *)value, 10);
    CBAT_REQUIRE(cbat_dict_next(d, &it, &key, &value));
    CBAT_CHECK_EQ_STR(key, "b");
    CBAT_CHECK(!cbat_dict_next(d, &it, &key, &value));
    cbat_dict_free(d);
}

CBAT_TEST(test_many_keys_and_insertion_order) {
    CBatDict *d = cbat_dict_new(sizeof(int));
    char key[32];
    int i;
    size_t it = 0;
    const char *k;
    void *v;
    int expected = 0;
    CBAT_REQUIRE_NOT_NULL(d);
    for (i = 0; i < 5000; i++) {
        snprintf(key, sizeof key, "key-%d", i);
        CBAT_REQUIRE_OK(CBAT_DICT_SET(d, key, int, i));
    }
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(d), 5000);
    for (i = 0; i < 5000; i += 97) {
        snprintf(key, sizeof key, "key-%d", i);
        CBAT_REQUIRE_EQ_INT(CBAT_DICT_GET(d, key, int, -1), i);
    }
    while (cbat_dict_next(d, &it, &k, &v)) {
        CBAT_REQUIRE_EQ_INT(*(int *)v, expected);
        expected++;
    }
    CBAT_CHECK_EQ_INT(expected, 5000);
    cbat_dict_free(d);
}

CBAT_TEST(test_remove_and_reinsert) {
    CBatDict *d = cbat_dict_new(sizeof(int));
    char key[32];
    int i;
    int out = 0;
    CBAT_REQUIRE_NOT_NULL(d);
    for (i = 0; i < 1000; i++) {
        snprintf(key, sizeof key, "k%d", i);
        CBAT_REQUIRE_OK(CBAT_DICT_SET(d, key, int, i));
    }
    for (i = 0; i < 1000; i += 2) {
        snprintf(key, sizeof key, "k%d", i);
        CBAT_REQUIRE_OK(cbat_dict_remove(d, key, NULL));
    }
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(d), 500);
    CBAT_CHECK_ERR(cbat_dict_remove(d, "k0", NULL), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_OK(cbat_dict_remove(d, "k1", &out));
    CBAT_CHECK_EQ_INT(out, 1);
    /* Churn: many insert/remove cycles must not grow without bound or lose keys. */
    for (i = 0; i < 20000; i++) {
        snprintf(key, sizeof key, "temp%d", i % 7);
        CBAT_REQUIRE_OK(CBAT_DICT_SET(d, key, int, i));
        CBAT_REQUIRE_OK(cbat_dict_remove(d, key, NULL));
    }
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(d), 499);
    for (i = 3; i < 1000; i += 2) {
        snprintf(key, sizeof key, "k%d", i);
        CBAT_REQUIRE_EQ_INT(CBAT_DICT_GET(d, key, int, -1), i);
    }
    cbat_dict_free(d);
}

CBAT_TEST(test_iteration_skips_removed) {
    CBatDict *d = cbat_dict_new(sizeof(int));
    size_t it = 0;
    const char *k;
    CBatList *keys = NULL;
    CBAT_REQUIRE_NOT_NULL(d);
    CBAT_DICT_SET(d, "x", int, 1);
    CBAT_DICT_SET(d, "y", int, 2);
    CBAT_DICT_SET(d, "z", int, 3);
    cbat_dict_remove(d, "y", NULL);
    CBAT_REQUIRE(cbat_dict_next(d, &it, &k, NULL));
    CBAT_CHECK_EQ_STR(k, "x");
    CBAT_REQUIRE(cbat_dict_next(d, &it, &k, NULL));
    CBAT_CHECK_EQ_STR(k, "z");
    CBAT_CHECK(!cbat_dict_next(d, &it, &k, NULL));
    CBAT_REQUIRE_OK(cbat_dict_keys(d, &keys));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(keys), 2);
    CBAT_CHECK_EQ_STR(CBAT_LIST_GET(keys, const char *, 1), "z");
    cbat_list_free(keys);
    cbat_dict_free(d);
}

CBAT_TEST(test_clear) {
    CBatDict *d = cbat_dict_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(d);
    CBAT_DICT_SET(d, "a", int, 1);
    cbat_dict_clear(d);
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(d), 0);
    CBAT_CHECK(!cbat_dict_contains(d, "a"));
    CBAT_CHECK_OK(CBAT_DICT_SET(d, "a", int, 2));
    CBAT_CHECK_EQ_INT(CBAT_DICT_GET(d, "a", int, 0), 2);
    cbat_dict_free(d);
}

CBAT_TEST(test_value_destructor_with_strings) {
    CBatDict *d = cbat_dict_new(sizeof(CBatString *));
    CBatString *s;
    CBatString *taken = NULL;
    CBAT_REQUIRE_NOT_NULL(d);
    cbat_dict_set_destructor(d, cbat_string_destructor);
    s = cbat_string_from("first");
    CBAT_REQUIRE_OK(cbat_dict_set(d, "k", &s));
    s = cbat_string_from("second"); /* replaces and frees "first" */
    CBAT_REQUIRE_OK(cbat_dict_set(d, "k", &s));
    s = cbat_string_from("other");
    CBAT_REQUIRE_OK(cbat_dict_set(d, "o", &s));
    CBAT_REQUIRE_OK(cbat_dict_remove(d, "o", &taken)); /* ownership moves to us */
    CBAT_CHECK_EQ_STR(cbat_string_cstr(taken), "other");
    cbat_string_free(taken);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(*(CBatString **)cbat_dict_get(d, "k")), "second");
    cbat_dict_free(d); /* frees "second" */
}

CBAT_TEST(test_zero_value_size_and_bad_args) {
    CBatDict *d = cbat_dict_new(0);
    int x = 1;
    CBAT_REQUIRE_NOT_NULL(d);
    CBAT_CHECK_OK(cbat_dict_set(d, "present", NULL));
    CBAT_CHECK(cbat_dict_contains(d, "present"));
    CBAT_CHECK_ERR(cbat_dict_set(d, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_dict_set(NULL, "k", &x), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_NULL(cbat_dict_get(NULL, "k"));
    CBAT_CHECK_NULL(cbat_dict_get(d, NULL));
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(NULL), 0);
    cbat_dict_free(d);
    d = cbat_dict_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(d);
    CBAT_CHECK_ERR(cbat_dict_set(d, "k", NULL), CBAT_ERR_INVALID_ARGUMENT);
    cbat_dict_free(d);
    cbat_dict_free(NULL);
}

CBAT_TEST(test_empty_key) {
    CBatDict *d = cbat_dict_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(d);
    CBAT_CHECK_OK(CBAT_DICT_SET(d, "", int, 7));
    CBAT_CHECK_EQ_INT(CBAT_DICT_GET(d, "", int, 0), 7);
    cbat_dict_free(d);
}

CBAT_TEST(test_dict_in_arena) {
    CBatArena *arena = cbat_arena_new(0);
    CBatDict *d;
    char key[16];
    int i;
    CBAT_REQUIRE_NOT_NULL(arena);
    d = cbat_dict_new_with(sizeof(int), cbat_arena_allocator(arena));
    CBAT_REQUIRE_NOT_NULL(d);
    for (i = 0; i < 300; i++) {
        snprintf(key, sizeof key, "%d", i);
        CBAT_REQUIRE_OK(CBAT_DICT_SET(d, key, int, i));
    }
    CBAT_CHECK_EQ_INT(CBAT_DICT_GET(d, "299", int, -1), 299);
    cbat_arena_free(arena);
}

CBAT_TEST(test_out_of_memory) {
    size_t n;
    for (n = 0; n < 12; n++) {
        CBatDict *d;
        char key[16];
        int i;
        CBatError err = CBAT_OK;
        cbat_test_fail_allocations_after(n);
        d = cbat_dict_new(sizeof(int));
        if (d == NULL) {
            cbat_test_stop_failing_allocations();
            continue;
        }
        for (i = 0; i < 50 && err == CBAT_OK; i++) {
            snprintf(key, sizeof key, "%d", i);
            err = CBAT_DICT_SET(d, key, int, i);
        }
        cbat_test_stop_failing_allocations();
        CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
        /* Everything inserted before the failure is still there. */
        for (i = i - 2; i >= 0; i--) {
            snprintf(key, sizeof key, "%d", i);
            CBAT_CHECK_EQ_INT(CBAT_DICT_GET(d, key, int, -1), i);
        }
        CBAT_CHECK_OK(CBAT_DICT_SET(d, "after", int, 1));
        cbat_dict_free(d);
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_set_get_contains),
    CBAT_TEST_ENTRY(test_replace_keeps_order),
    CBAT_TEST_ENTRY(test_many_keys_and_insertion_order),
    CBAT_TEST_ENTRY(test_remove_and_reinsert),
    CBAT_TEST_ENTRY(test_iteration_skips_removed),
    CBAT_TEST_ENTRY(test_clear),
    CBAT_TEST_ENTRY(test_value_destructor_with_strings),
    CBAT_TEST_ENTRY(test_zero_value_size_and_bad_args),
    CBAT_TEST_ENTRY(test_empty_key),
    CBAT_TEST_ENTRY(test_dict_in_arena),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("dict", tests)
