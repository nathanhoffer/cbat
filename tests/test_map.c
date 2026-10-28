#include <stdint.h>
#include <string.h>
#include "cbat/map.h"
#include "cbat/hash.h"
#include "cbat/test.h"

static uint64_t collide(const void *key) { CBAT_UNUSED(key); return 1; }
static bool equal_int(const void *a, const void *b) { return *(const int *)a == *(const int *)b; }

CBAT_TEST(test_basics_and_invalid_arguments) {
    CBatMap *map = cbat_map_new(sizeof(int), sizeof(int));
    int key = 5, value = 25, fallback = 99, key_out = -1, value_out = -1;
    size_t it = 0;
    CBAT_REQUIRE_NOT_NULL(map);
    CBAT_CHECK_EQ_SIZE(cbat_map_length(map), 0);
    CBAT_CHECK_EQ_SIZE(cbat_map_length(NULL), 0);
    CBAT_CHECK_NULL(cbat_map_new(0, 4));
    CBAT_CHECK_NULL(cbat_map_new(SIZE_MAX, 4));
    CBAT_CHECK_NULL(cbat_map_new(4, SIZE_MAX));
    CBAT_CHECK_NULL(cbat_map_get(NULL, &key));
    CBAT_CHECK_NULL(cbat_map_get(map, NULL));
    CBAT_CHECK(cbat_map_get_or(map, &key, &fallback) == &fallback);
    CBAT_CHECK(!cbat_map_contains(map, &key));
    CBAT_CHECK_ERR(cbat_map_set(NULL, &key, &value), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_map_set(map, NULL, &value), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_map_set(map, &key, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_map_remove(NULL, &key, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_map_remove(map, NULL, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_map_remove(map, &key, &key_out, &value_out), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_EQ_INT(key_out, -1);
    CBAT_CHECK_EQ_INT(value_out, -1);
    CBAT_CHECK(!cbat_map_next(map, NULL, NULL, NULL));
    CBAT_CHECK(!cbat_map_next(NULL, &it, NULL, NULL));
    CBAT_CHECK(!cbat_map_next(map, &it, NULL, NULL));
    CBAT_REQUIRE_OK(cbat_map_set(map, &key, &value));
    CBAT_CHECK(cbat_map_contains(map, &key));
    CBAT_CHECK_EQ_INT(*(int *)cbat_map_get(map, &key), 25);
    CBAT_REQUIRE_OK(CBAT_MAP_SET(map, int, 5, int, 36));
    CBAT_CHECK_EQ_INT(CBAT_MAP_GET(map, int, 5, int, -1), 36);
    CBAT_CHECK_EQ_INT(CBAT_MAP_GET(map, int, 999, int, -1), -1);
    CBAT_CHECK_EQ_SIZE(cbat_map_length(map), 1);
    CBAT_CHECK_ERR(cbat_map_set_hash(map, collide, equal_int), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_REQUIRE_OK(cbat_map_remove(map, &key, &key_out, &value_out));
    CBAT_CHECK_EQ_INT(key_out, key);
    CBAT_CHECK_EQ_INT(value_out, 36);
    CBAT_CHECK_EQ_SIZE(cbat_map_length(map), 0);
    cbat_map_clear(map);
    cbat_map_free(map);
    cbat_map_clear(NULL);
    cbat_map_free(NULL);
    cbat_map_set_key_destructor(NULL, NULL);
    cbat_map_set_value_destructor(NULL, NULL);
    CBAT_CHECK_ERR(cbat_map_set_hash(NULL, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_collisions_order_and_compaction) {
    CBatMap *map = cbat_map_new(sizeof(int), sizeof(int));
    int i;
    size_t it = 0;
    const void *key;
    void *value;
    CBAT_REQUIRE_NOT_NULL(map);
    CBAT_CHECK_ERR(cbat_map_set_hash(map, collide, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_map_set_hash(map, NULL, equal_int), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_REQUIRE_OK(cbat_map_set_hash(map, collide, equal_int));
    for (i = 0; i < 100; i++) CBAT_REQUIRE_OK(cbat_map_set(map, &i, &i));
    for (i = 0; i < 100; i += 2) CBAT_REQUIRE_OK(cbat_map_remove(map, &i, NULL, NULL));
    for (i = 100; i < 200; i++) CBAT_REQUIRE_OK(cbat_map_set(map, &i, &i));
    i = 1;
    while (cbat_map_next(map, &it, &key, &value)) {
        CBAT_CHECK_EQ_INT(*(const int *)key, i);
        CBAT_CHECK_EQ_INT(*(int *)value, i);
        i += i < 99 ? 2 : 1;
    }
    CBAT_CHECK_EQ_INT(i, 200);
    i = 99;
    CBAT_REQUIRE_OK(cbat_map_remove(map, &i, NULL, NULL));
    CBAT_REQUIRE_OK(cbat_map_set(map, &i, &i));
    it = 0;
    key = NULL;
    while (cbat_map_next(map, &it, &key, NULL)) { /* last key is reinserted */ }
    CBAT_CHECK_EQ_INT(*(const int *)key, 99);
    cbat_map_clear(map);
    CBAT_CHECK_EQ_SIZE(cbat_map_length(map), 0);
    CBAT_REQUIRE_OK(cbat_map_set_hash(map, NULL, NULL));
    CBAT_REQUIRE_OK(CBAT_MAP_SET(map, int, 1, int, 2));
    CBAT_CHECK_EQ_INT(CBAT_MAP_GET(map, int, 1, int, -1), 2);
    cbat_map_free(map);
}

CBAT_TEST(test_large_growth) {
    CBatMap *map = cbat_map_new(sizeof(uint64_t), sizeof(uint64_t));
    uint64_t i;
    size_t it = 0, seen = 0;
    const void *key;
    void *value;
    CBAT_REQUIRE_NOT_NULL(map);
    for (i = 0; i < 1000000; i++) {
        uint64_t square = i * i;
        CBAT_REQUIRE_OK(cbat_map_set(map, &i, &square));
    }
    CBAT_CHECK_EQ_SIZE(cbat_map_length(map), 1000000);
    for (i = 0; i < 1000000; i++) {
        uint64_t *found = (uint64_t *)cbat_map_get(map, &i);
        CBAT_REQUIRE_NOT_NULL(found);
        CBAT_CHECK_EQ_UINT(*found, i * i);
    }
    for (i = 0; i < 1000000; i += 2) CBAT_REQUIRE_OK(cbat_map_remove(map, &i, NULL, NULL));
    while (cbat_map_next(map, &it, &key, &value)) {
        uint64_t expected = (uint64_t)seen * 2 + 1;
        CBAT_CHECK_EQ_UINT(*(const uint64_t *)key, expected);
        CBAT_CHECK_EQ_UINT(*(uint64_t *)value, expected * expected);
        seen++;
    }
    CBAT_CHECK_EQ_SIZE(seen, 500000);
    cbat_map_free(map);
}

CBAT_TEST(test_representations_and_alignment) {
    struct Key { char tag; uint64_t number; } key;
    CBatMap *map = cbat_map_new(sizeof(key), sizeof(long double));
    CBatMap *floats = cbat_map_new(sizeof(double), sizeof(int));
    CBatMap *integers = cbat_map_new(sizeof(int64_t), 0);
    long double value = 1.25L;
    double positive_zero = 0.0, negative_zero = -0.0, nan_a, nan_b;
    uint64_t nan_bits_a = UINT64_C(0x7ff8000000000001), nan_bits_b = UINT64_C(0x7ff8000000000002);
    int64_t low = INT64_MIN, high = INT64_MAX;
    int one = 1, two = 2;
    CBAT_REQUIRE_NOT_NULL(map);
    CBAT_REQUIRE_NOT_NULL(floats);
    CBAT_REQUIRE_NOT_NULL(integers);
    memset(&key, 0, sizeof(key));
    key.tag = 'x'; key.number = UINT64_C(0xfedcba9876543210);
    CBAT_REQUIRE_OK(cbat_map_set(map, &key, &value));
    CBAT_CHECK(*(long double *)cbat_map_get(map, &key) == value);
    CBAT_REQUIRE_OK(cbat_map_set(floats, &positive_zero, &one));
    CBAT_REQUIRE_OK(cbat_map_set(floats, &negative_zero, &two));
    CBAT_CHECK_EQ_SIZE(cbat_map_length(floats), 2);
    CBAT_CHECK_EQ_INT(*(int *)cbat_map_get(floats, &positive_zero), 1);
    CBAT_CHECK_EQ_INT(*(int *)cbat_map_get(floats, &negative_zero), 2);
    memcpy(&nan_a, &nan_bits_a, sizeof(nan_a));
    memcpy(&nan_b, &nan_bits_b, sizeof(nan_b));
    CBAT_REQUIRE_OK(cbat_map_set(floats, &nan_a, &one));
    CBAT_REQUIRE_OK(cbat_map_set(floats, &nan_b, &two));
    CBAT_CHECK_EQ_INT(*(int *)cbat_map_get(floats, &nan_a), 1);
    CBAT_CHECK_EQ_INT(*(int *)cbat_map_get(floats, &nan_b), 2);
    CBAT_REQUIRE_OK(cbat_map_set(integers, &low, NULL));
    CBAT_REQUIRE_OK(cbat_map_set(integers, &high, NULL));
    CBAT_CHECK(cbat_map_contains(integers, &low));
    CBAT_CHECK(cbat_map_contains(integers, &high));
    CBAT_REQUIRE_OK(cbat_map_remove(integers, &low, NULL, NULL));
    cbat_map_free(integers);
    cbat_map_free(floats);
    cbat_map_free(map);
}

static int key_destroys, value_destroys;
static uint64_t string_hash(const void *key) { return cbat_hash_cstring(*(const char *const *)key); }
static bool string_equal(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b) == 0;
}
static void destroy_key(void *key) { key_destroys++; cbat_destroy_pointer(key); }
static void destroy_value(void *value) { value_destroys++; cbat_destroy_pointer(value); }

CBAT_TEST(test_ownership_and_custom_pointer_keys) {
    CBatMap *map = cbat_map_new(sizeof(char *), sizeof(char *));
    char *key = cbat_strdup(NULL, "name");
    char *first = cbat_strdup(NULL, "first");
    char *second = cbat_strdup(NULL, "second");
    char *candidate = cbat_strdup(NULL, "name");
    char *key_out = NULL;
    char *value_out = NULL;
    CBAT_REQUIRE_NOT_NULL(map);
    CBAT_REQUIRE_NOT_NULL(key);
    CBAT_REQUIRE_NOT_NULL(first);
    CBAT_REQUIRE_NOT_NULL(second);
    CBAT_REQUIRE_NOT_NULL(candidate);
    key_destroys = value_destroys = 0;
    CBAT_REQUIRE_OK(cbat_map_set_hash(map, string_hash, string_equal));
    cbat_map_set_key_destructor(map, destroy_key);
    cbat_map_set_value_destructor(map, destroy_value);
    CBAT_REQUIRE_OK(cbat_map_set(map, &key, &first));
    CBAT_REQUIRE_OK(cbat_map_set(map, &candidate, &second));
    CBAT_CHECK_EQ_INT(key_destroys, 0);
    CBAT_CHECK_EQ_INT(value_destroys, 1);
    CBAT_CHECK_EQ_STR(*(char **)cbat_map_get(map, &candidate), "second");
    CBAT_REQUIRE_OK(cbat_map_set(map, &candidate, cbat_map_get(map, &candidate)));
    CBAT_CHECK_EQ_INT(value_destroys, 1); /* exact self-assignment is harmless */
    CBAT_REQUIRE_OK(cbat_map_remove(map, &candidate, &key_out, &value_out));
    CBAT_CHECK(key_out == key);
    CBAT_CHECK(value_out == second);
    CBAT_CHECK_EQ_INT(value_destroys, 1);
    cbat_free(key_out); cbat_free(value_out); cbat_free(candidate);
    key = cbat_strdup(NULL, "another");
    first = cbat_strdup(NULL, "owned");
    CBAT_REQUIRE_NOT_NULL(key);
    CBAT_REQUIRE_NOT_NULL(first);
    CBAT_REQUIRE_OK(cbat_map_set(map, &key, &first));
    cbat_map_clear(map);
    CBAT_CHECK_EQ_INT(key_destroys, 1);
    CBAT_CHECK_EQ_INT(value_destroys, 2);
    cbat_map_free(map);
}

CBAT_TEST(test_independent_ownership_transfer) {
    CBatMap *map = cbat_map_new(sizeof(char *), sizeof(char *));
    char *key, *value, *out = NULL;
    CBAT_REQUIRE_NOT_NULL(map);
    CBAT_REQUIRE_OK(cbat_map_set_hash(map, string_hash, string_equal));
    cbat_map_set_key_destructor(map, destroy_key);
    cbat_map_set_value_destructor(map, destroy_value);
    key_destroys = value_destroys = 0;
    key = cbat_strdup(NULL, "key-out"); value = cbat_strdup(NULL, "destroyed");
    CBAT_REQUIRE_NOT_NULL(key); CBAT_REQUIRE_NOT_NULL(value);
    CBAT_REQUIRE_OK(cbat_map_set(map, &key, &value));
    CBAT_REQUIRE_OK(cbat_map_remove(map, &key, &out, NULL));
    CBAT_CHECK(out == key);
    CBAT_CHECK_EQ_INT(key_destroys, 0);
    CBAT_CHECK_EQ_INT(value_destroys, 1);
    cbat_free(out);
    key = cbat_strdup(NULL, "value-out"); value = cbat_strdup(NULL, "transferred");
    CBAT_REQUIRE_NOT_NULL(key); CBAT_REQUIRE_NOT_NULL(value);
    CBAT_REQUIRE_OK(cbat_map_set(map, &key, &value));
    CBAT_REQUIRE_OK(cbat_map_remove(map, &key, NULL, &out));
    CBAT_CHECK(out == value);
    CBAT_CHECK_EQ_INT(key_destroys, 1);
    CBAT_CHECK_EQ_INT(value_destroys, 1);
    cbat_free(out);
    key = cbat_strdup(NULL, "free"); value = cbat_strdup(NULL, "both");
    CBAT_REQUIRE_NOT_NULL(key); CBAT_REQUIRE_NOT_NULL(value);
    CBAT_REQUIRE_OK(cbat_map_set(map, &key, &value));
    cbat_map_free(map);
    CBAT_CHECK_EQ_INT(key_destroys, 2);
    CBAT_CHECK_EQ_INT(value_destroys, 2);
}

CBAT_TEST(test_borrowed_inputs_survive_growth) {
    CBatMap *map = cbat_map_new(sizeof(int), sizeof(int));
    int i;
    CBAT_REQUIRE_NOT_NULL(map);
    for (i = 0; i < 4; i++) CBAT_REQUIRE_OK(CBAT_MAP_SET(map, int, i, int, i + 1000));
    {
        int a = 3, b = 1;
        void *key = cbat_map_get(map, &a);
        void *value = cbat_map_get(map, &b);
        CBAT_REQUIRE_OK(cbat_map_set(map, key, value));
    }
    CBAT_CHECK_EQ_INT(CBAT_MAP_GET(map, int, 1003, int, -1), 1001);
    cbat_map_free(map);
}

CBAT_TEST(test_allocators_and_failures) {
    size_t allowed;
    bool completed = false;
    CBatArena *arena = cbat_arena_new(4096);
    CBatMap *in_arena;
    CBAT_REQUIRE_NOT_NULL(arena);
    in_arena = cbat_map_new_with(sizeof(int), sizeof(int), cbat_arena_allocator(arena));
    CBAT_REQUIRE_NOT_NULL(in_arena);
    CBAT_REQUIRE_OK(CBAT_MAP_SET(in_arena, int, 1, int, 2));
    CBAT_CHECK_EQ_INT(CBAT_MAP_GET(in_arena, int, 1, int, -1), 2);
    cbat_map_free(in_arena);
    cbat_arena_free(arena);
    for (allowed = 0; allowed < 16; allowed++) {
        CBatMap *map;
        CBatError err = CBAT_ERR_NO_MEMORY;
        int key = 1, value = 2;
        cbat_test_fail_allocations_after(allowed);
        map = cbat_map_new(sizeof(int), sizeof(int));
        if (map != NULL) err = cbat_map_set(map, &key, &value);
        cbat_test_stop_failing_allocations();
        CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
        if (err == CBAT_OK) completed = true;
        else if (map != NULL) {
            CBAT_CHECK_EQ_SIZE(cbat_map_length(map), 0);
            CBAT_REQUIRE_OK(cbat_map_set(map, &key, &value));
        }
        cbat_map_free(map);
        CBAT_CHECK_EQ_SIZE(cbat_test_live_allocations(), 0);
        if (completed) break;
    }
    CBAT_CHECK(completed);
    completed = false;
    for (allowed = 0; allowed < 16; allowed++) {
        unsigned char payload[300];
        int i, key = 4;
        CBatMap *map = cbat_map_new(sizeof(int), sizeof(payload));
        CBatError err;
        CBAT_REQUIRE_NOT_NULL(map);
        memset(payload, 0x5a, sizeof(payload));
        for (i = 0; i < 4; i++) CBAT_REQUIRE_OK(cbat_map_set(map, &i, payload));
        cbat_test_fail_allocations_after(allowed);
        err = cbat_map_set(map, &key, payload);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) completed = true;
        else {
            CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
            CBAT_CHECK_EQ_SIZE(cbat_map_length(map), 4);
            CBAT_CHECK_NULL(cbat_map_get(map, &key));
            for (i = 0; i < 4; i++) CBAT_CHECK_EQ_MEM(cbat_map_get(map, &i), payload, sizeof(payload));
            CBAT_REQUIRE_OK(cbat_map_set(map, &key, payload));
        }
        CBAT_CHECK_EQ_MEM(cbat_map_get(map, &key), payload, sizeof(payload));
        cbat_map_free(map);
        CBAT_CHECK_EQ_SIZE(cbat_test_live_allocations(), 0);
        if (completed) break;
    }
    CBAT_CHECK(completed);
}

CBAT_TEST(test_compaction_allocation_failure) {
    size_t allowed;
    bool completed = false;
    for (allowed = 0; allowed < 16; allowed++) {
        CBatMap *map = cbat_map_new(sizeof(int), sizeof(int));
        CBatError err;
        int i, key = 100, value = 1000;
        CBAT_REQUIRE_NOT_NULL(map);
        CBAT_REQUIRE_OK(cbat_map_set_hash(map, collide, equal_int));
        for (i = 0; i < 6; i++) CBAT_REQUIRE_OK(cbat_map_set(map, &i, &i));
        for (i = 0; i < 4; i++) CBAT_REQUIRE_OK(cbat_map_remove(map, &i, NULL, NULL));
        cbat_test_fail_allocations_after(allowed);
        err = cbat_map_set(map, &key, &value);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) completed = true;
        else {
            CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
            CBAT_CHECK_EQ_SIZE(cbat_map_length(map), 2);
            for (i = 4; i < 6; i++) CBAT_CHECK_EQ_INT(*(int *)cbat_map_get(map, &i), i);
            CBAT_REQUIRE_OK(cbat_map_set(map, &key, &value));
        }
        CBAT_CHECK_EQ_INT(*(int *)cbat_map_get(map, &key), value);
        cbat_map_free(map);
        CBAT_CHECK_EQ_SIZE(cbat_test_live_allocations(), 0);
        if (completed) break;
    }
    CBAT_CHECK(completed);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_basics_and_invalid_arguments),
    CBAT_TEST_ENTRY(test_collisions_order_and_compaction),
    CBAT_TEST_ENTRY(test_large_growth),
    CBAT_TEST_ENTRY(test_representations_and_alignment),
    CBAT_TEST_ENTRY(test_ownership_and_custom_pointer_keys),
    CBAT_TEST_ENTRY(test_independent_ownership_transfer),
    CBAT_TEST_ENTRY(test_borrowed_inputs_survive_growth),
    CBAT_TEST_ENTRY(test_allocators_and_failures),
    CBAT_TEST_ENTRY(test_compaction_allocation_failure)
};
CBAT_TEST_MAIN("map", tests)
