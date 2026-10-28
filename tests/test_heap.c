#include <limits.h>
#include <string.h>
#include "cbat/heap.h"
#include "cbat/sort.h"
#include "cbat/random.h"
#include "cbat/test.h"

CBAT_TEST(test_ordering_and_push_pop) {
    CBatHeap *heap = cbat_heap_new(sizeof(int), cbat_compare_int);
    int value, result = -1;
    CBAT_REQUIRE_NOT_NULL(heap);
    CBAT_CHECK(cbat_heap_is_empty(heap));
    CBAT_CHECK_NULL(cbat_heap_peek(heap));
    CBAT_CHECK_ERR(cbat_heap_pop(heap, &result), CBAT_ERR_EMPTY);
    CBAT_CHECK_EQ_INT(result, -1);
    value = 5;
    CBAT_REQUIRE_OK(cbat_heap_push_pop(heap, &value, &result));
    CBAT_CHECK_EQ_INT(result, 5); CBAT_CHECK(cbat_heap_is_empty(heap));
    CBAT_REQUIRE_OK(CBAT_HEAP_PUSH(heap, int, 7));
    CBAT_REQUIRE_OK(CBAT_HEAP_PUSH(heap, int, 3));
    CBAT_REQUIRE_OK(CBAT_HEAP_PUSH(heap, int, 5));
    CBAT_CHECK_EQ_INT(*(int *)cbat_heap_peek(heap), 3);
    value = 2;
    CBAT_REQUIRE_OK(cbat_heap_push_pop(heap, &value, &result));
    CBAT_CHECK_EQ_INT(result, 2); CBAT_CHECK_EQ_SIZE(cbat_heap_length(heap), 3);
    value = 6;
    CBAT_REQUIRE_OK(cbat_heap_push_pop(heap, &value, &result));
    CBAT_CHECK_EQ_INT(result, 3); CBAT_CHECK_EQ_INT(*(int *)cbat_heap_peek(heap), 5);
    CBAT_REQUIRE_OK(cbat_heap_pop(heap, &result)); CBAT_CHECK_EQ_INT(result, 5);
    CBAT_REQUIRE_OK(cbat_heap_pop(heap, &result)); CBAT_CHECK_EQ_INT(result, 6);
    CBAT_REQUIRE_OK(cbat_heap_pop(heap, &result)); CBAT_CHECK_EQ_INT(result, 7);
    CBAT_CHECK(cbat_heap_is_empty(heap));
    cbat_heap_free(heap);
}

CBAT_TEST(test_bulk_and_selections) {
    CBatList *list = cbat_list_new(sizeof(int));
    CBatList *small = NULL, *large = NULL;
    CBatHeap *heap = NULL;
    int i, result;
    CBAT_REQUIRE_NOT_NULL(list);
    for (i = 9999; i >= 0; i--) CBAT_REQUIRE_OK(cbat_list_push(list, &i));
    CBAT_REQUIRE_OK(cbat_heap_from_list(list, cbat_compare_int, &heap));
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 0), 9999);
    for (i = 0; i < 10000; i++) {
        CBAT_REQUIRE_OK(cbat_heap_pop(heap, &result)); CBAT_CHECK_EQ_INT(result, i);
    }
    cbat_heap_free(heap);
    CBAT_REQUIRE_OK(cbat_list_smallest(list, 3, cbat_compare_int, &small));
    CBAT_REQUIRE_OK(cbat_list_largest(list, 3, cbat_compare_int, &large));
    CBAT_REQUIRE_EQ_SIZE(small->length, 3); CBAT_REQUIRE_EQ_SIZE(large->length, 3);
    for (i = 0; i < 3; i++) {
        CBAT_CHECK_EQ_INT(CBAT_LIST_GET(small, int, (size_t)i), i);
        CBAT_CHECK_EQ_INT(CBAT_LIST_GET(large, int, (size_t)i), 9999 - i);
    }
    cbat_list_free(small); cbat_list_free(large);
    CBAT_REQUIRE_OK(cbat_list_smallest(list, 0, cbat_compare_int, &small));
    CBAT_CHECK_EQ_SIZE(small->length, 0); cbat_list_free(small);
    cbat_list_clear(list);
    CBAT_REQUIRE_OK(CBAT_LIST_PUSH(list, int, 2));
    CBAT_REQUIRE_OK(CBAT_LIST_PUSH(list, int, 1));
    CBAT_REQUIRE_OK(cbat_list_largest(list, SIZE_MAX, cbat_compare_int, &large));
    CBAT_CHECK_EQ_SIZE(large->length, 2);
    cbat_list_free(large); cbat_list_clear(list);
    CBAT_REQUIRE_OK(cbat_heap_from_list(list, cbat_compare_int, &heap));
    CBAT_CHECK(cbat_heap_is_empty(heap)); cbat_heap_free(heap);
    cbat_list_free(list);
}

static int compare_extreme(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return x < y ? INT_MIN : x > y ? INT_MAX : 0;
}

CBAT_TEST(test_random_pushes_duplicates_and_reverse_compare) {
    CBatHeap *heap = cbat_heap_new(sizeof(int), compare_extreme);
    CBatList *list = cbat_list_new(sizeof(int)), *largest = NULL;
    CBatRandom rng;
    int i, previous = INT_MIN, result;
    CBAT_REQUIRE_NOT_NULL(heap); CBAT_REQUIRE_NOT_NULL(list);
    cbat_random_seed(&rng, 42);
    for (i = 0; i < 10000; i++) {
        int value = (int)cbat_random_range(&rng, -100, 100);
        CBAT_REQUIRE_OK(cbat_heap_push(heap, &value));
        CBAT_REQUIRE_OK(cbat_list_push(list, &value));
    }
    for (i = 0; i < 10000; i++) {
        CBAT_REQUIRE_OK(cbat_heap_pop(heap, &result));
        CBAT_CHECK(result >= previous); previous = result;
    }
    CBAT_REQUIRE_OK(cbat_list_largest(list, 10000, compare_extreme, &largest));
    for (i = 1; i < 10000; i++)
        CBAT_CHECK(CBAT_LIST_GET(largest, int, (size_t)i - 1) >= CBAT_LIST_GET(largest, int, (size_t)i));
    cbat_list_free(largest); cbat_list_free(list); cbat_heap_free(heap);
}

static int destroyed;
static void destroy_string(void *element) { destroyed++; cbat_destroy_pointer(element); }

CBAT_TEST(test_ownership) {
    CBatHeap *heap = cbat_heap_new(sizeof(char *), cbat_compare_cstring);
    char *a = cbat_strdup(NULL, "a"), *b = cbat_strdup(NULL, "b"), *c = cbat_strdup(NULL, "c"), *result = NULL;
    CBAT_REQUIRE_NOT_NULL(heap); CBAT_REQUIRE_NOT_NULL(a); CBAT_REQUIRE_NOT_NULL(b); CBAT_REQUIRE_NOT_NULL(c);
    destroyed = 0;
    cbat_heap_set_destructor(heap, destroy_string);
    CBAT_REQUIRE_OK(cbat_heap_push(heap, &b));
    CBAT_REQUIRE_OK(cbat_heap_push_pop(heap, &a, NULL));
    CBAT_CHECK_EQ_INT(destroyed, 1); /* discarded incoming a */
    CBAT_REQUIRE_OK(cbat_heap_push_pop(heap, &c, &result));
    CBAT_CHECK_EQ_STR(result, "b"); cbat_free(result);
    CBAT_CHECK_EQ_INT(destroyed, 1);
    CBAT_REQUIRE_OK(cbat_heap_pop(heap, NULL));
    CBAT_CHECK_EQ_INT(destroyed, 2); /* c */
    a = cbat_strdup(NULL, "again"); CBAT_REQUIRE_NOT_NULL(a);
    CBAT_REQUIRE_OK(cbat_heap_push(heap, &a));
    cbat_heap_free(heap); CBAT_CHECK_EQ_INT(destroyed, 3);
}

CBAT_TEST(test_invalid_arguments_and_alias_outputs) {
    CBatHeap *heap = cbat_heap_new(sizeof(int), cbat_compare_int);
    CBatList *list = cbat_list_new(sizeof(int)), *out = NULL;
    CBatHeap *heap_out = NULL;
    int value = 2;
    CBAT_REQUIRE_NOT_NULL(heap); CBAT_REQUIRE_NOT_NULL(list);
    CBAT_CHECK_NULL(cbat_heap_new(0, cbat_compare_int));
    CBAT_CHECK_NULL(cbat_heap_new(4, NULL));
    CBAT_CHECK_EQ_SIZE(cbat_heap_length(NULL), 0); CBAT_CHECK(cbat_heap_is_empty(NULL));
    CBAT_CHECK_NULL(cbat_heap_peek(NULL)); cbat_heap_free(NULL); cbat_heap_set_destructor(NULL, NULL);
    CBAT_CHECK_ERR(cbat_heap_push(NULL, &value), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_heap_push(heap, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_heap_pop(NULL, &value), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_heap_push_pop(NULL, &value, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_heap_push_pop(heap, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_REQUIRE_OK(cbat_heap_push(heap, &value));
    CBAT_CHECK_ERR(cbat_heap_pop(heap, cbat_heap_peek(heap)), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_heap_push_pop(heap, &value, cbat_heap_peek(heap)), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_EQ_SIZE(cbat_heap_length(heap), 1);
    CBAT_CHECK_ERR(cbat_heap_from_list(NULL, cbat_compare_int, &heap_out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_heap_from_list(list, NULL, &heap_out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_heap_from_list(list, cbat_compare_int, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_list_smallest(NULL, 1, cbat_compare_int, &out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_list_smallest(list, 1, NULL, &out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_list_smallest(list, 1, cbat_compare_int, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_list_largest(NULL, 1, cbat_compare_int, &out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_list_largest(list, 1, NULL, &out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_list_largest(list, 1, cbat_compare_int, NULL), CBAT_ERR_INVALID_ARGUMENT);
    cbat_heap_free(heap); cbat_list_free(list);
}

CBAT_TEST(test_oom_and_custom_allocator) {
    CBatList *list = cbat_list_new(sizeof(int));
    size_t allowed;
    bool constructed = false, bulk = false, small = false, large = false;
    CBatArena *arena = cbat_arena_new(4096);
    CBatHeap *heap;
    int i, result;
    CBAT_REQUIRE_NOT_NULL(list); CBAT_REQUIRE_NOT_NULL(arena);
    heap = cbat_heap_new_with(sizeof(int), cbat_compare_int, cbat_arena_allocator(arena));
    CBAT_REQUIRE_NOT_NULL(heap);
    CBAT_REQUIRE_OK(CBAT_HEAP_PUSH(heap, int, 5));
    CBAT_REQUIRE_OK(cbat_heap_pop(heap, &result)); CBAT_CHECK_EQ_INT(result, 5);
    cbat_heap_free(heap); cbat_arena_free(arena);
    for (i = 0; i < 20; i++) CBAT_REQUIRE_OK(cbat_list_push(list, &i));
    for (allowed = 0; allowed < 20; allowed++) {
        CBatHeap *copy = NULL;
        CBatList *selection = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(allowed);
        heap = cbat_heap_new(sizeof(int), cbat_compare_int);
        cbat_test_stop_failing_allocations();
        if (heap != NULL) constructed = true;
        cbat_heap_free(heap);
        cbat_test_fail_allocations_after(allowed);
        err = cbat_heap_from_list(list, cbat_compare_int, &copy);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) bulk = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(copy); }
        cbat_heap_free(copy);
        cbat_test_fail_allocations_after(allowed);
        err = cbat_list_smallest(list, 3, cbat_compare_int, &selection);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) small = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(selection); }
        cbat_list_free(selection); selection = NULL;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_list_largest(list, 3, cbat_compare_int, &selection);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) large = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(selection); }
        cbat_list_free(selection);
        if (constructed && bulk && small && large) break;
    }
    CBAT_CHECK(constructed && bulk && small && large);
    heap = cbat_heap_new(sizeof(int), cbat_compare_int); CBAT_REQUIRE_NOT_NULL(heap);
    for (i = 0; i < 4; i++) CBAT_REQUIRE_OK(cbat_heap_push(heap, &i));
    cbat_test_fail_allocations_after(0);
    {
        int value = 100;
        CBatError failed = cbat_heap_push(heap, &value);
        CBatError succeeded = cbat_heap_push_pop(heap, &value, &result);
        cbat_test_stop_failing_allocations();
        CBAT_CHECK_ERR(failed, CBAT_ERR_NO_MEMORY);
        CBAT_CHECK_OK(succeeded); CBAT_CHECK_EQ_INT(result, 0);
        CBAT_CHECK_EQ_SIZE(cbat_heap_length(heap), 4);
    }
    cbat_heap_free(heap); cbat_list_free(list);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_ordering_and_push_pop),
    CBAT_TEST_ENTRY(test_bulk_and_selections),
    CBAT_TEST_ENTRY(test_random_pushes_duplicates_and_reverse_compare),
    CBAT_TEST_ENTRY(test_ownership),
    CBAT_TEST_ENTRY(test_invalid_arguments_and_alias_outputs),
    CBAT_TEST_ENTRY(test_oom_and_custom_allocator)
};
CBAT_TEST_MAIN("heap", tests)
