#include <string.h>

#include "cbat/list.h"
#include "cbat/sort.h"
#include "cbat/test.h"

typedef struct Point {
    int x;
    int y;
} Point;

CBAT_TEST(test_new_and_free) {
    CBatList *list = cbat_list_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(list);
    CBAT_CHECK_EQ_SIZE(cbat_list_length(list), 0);
    CBAT_CHECK(cbat_list_is_empty(list));
    CBAT_CHECK_NULL(cbat_list_new(0));
    cbat_list_free(list);
    cbat_list_free(NULL);
}

CBAT_TEST(test_push_get_and_macros) {
    CBatList *list = cbat_list_new(sizeof(int));
    int i;
    CBAT_REQUIRE_NOT_NULL(list);
    for (i = 0; i < 100; i++) {
        CBAT_REQUIRE_OK(CBAT_LIST_PUSH(list, int, i * i));
    }
    CBAT_CHECK_EQ_SIZE(cbat_list_length(list), 100);
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 0), 0);
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 99), 9801);
    CBAT_CHECK_NULL(cbat_list_get(list, 100));
    CBAT_CHECK_EQ_INT(*(int *)cbat_list_first(list), 0);
    CBAT_CHECK_EQ_INT(*(int *)cbat_list_last(list), 9801);
    cbat_list_free(list);
}

CBAT_TEST(test_struct_elements) {
    CBatList *list = cbat_list_new(sizeof(Point));
    Point p = {3, 4};
    Point *got;
    CBAT_REQUIRE_NOT_NULL(list);
    CBAT_REQUIRE_OK(cbat_list_push(list, &p));
    got = (Point *)cbat_list_get(list, 0);
    CBAT_REQUIRE_NOT_NULL(got);
    CBAT_CHECK_EQ_INT(got->x, 3);
    CBAT_CHECK_EQ_INT(got->y, 4);
    cbat_list_free(list);
}

CBAT_TEST(test_set_insert_remove) {
    CBatList *list = cbat_list_new(sizeof(int));
    int out = 0;
    CBAT_REQUIRE_NOT_NULL(list);
    CBAT_LIST_PUSH(list, int, 1);
    CBAT_LIST_PUSH(list, int, 3);
    CBAT_CHECK_OK(CBAT_LIST_INSERT(list, int, 1, 2));
    CBAT_CHECK_OK(CBAT_LIST_INSERT(list, int, 0, 0));
    CBAT_CHECK_OK(CBAT_LIST_INSERT(list, int, 4, 4));
    CBAT_CHECK_ERR(CBAT_LIST_INSERT(list, int, 9, 9), CBAT_ERR_OUT_OF_RANGE);
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(list), 5);
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 2), 2);
    CBAT_CHECK_OK(CBAT_LIST_SET(list, int, 2, 20));
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 2), 20);
    CBAT_CHECK_ERR(CBAT_LIST_SET(list, int, 5, 1), CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_OK(cbat_list_remove(list, 0, &out));
    CBAT_CHECK_EQ_INT(out, 0);
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 0), 1);
    CBAT_CHECK_ERR(cbat_list_remove(list, 10, NULL), CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_OK(cbat_list_pop(list, &out));
    CBAT_CHECK_EQ_INT(out, 4);
    CBAT_CHECK_EQ_SIZE(cbat_list_length(list), 3);
    cbat_list_free(list);
}

CBAT_TEST(test_pop_empty) {
    CBatList *list = cbat_list_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(list);
    CBAT_CHECK_ERR(cbat_list_pop(list, NULL), CBAT_ERR_EMPTY);
    CBAT_CHECK_ERR(cbat_list_remove(list, 0, NULL), CBAT_ERR_EMPTY);
    CBAT_CHECK_NULL(cbat_list_first(list));
    CBAT_CHECK_NULL(cbat_list_last(list));
    cbat_list_free(list);
}

CBAT_TEST(test_null_arguments) {
    int x = 1;
    CBAT_CHECK_ERR(cbat_list_push(NULL, &x), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_list_pop(NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_EQ_SIZE(cbat_list_length(NULL), 0);
    CBAT_CHECK_NULL(cbat_list_get(NULL, 0));
    CBAT_CHECK_NULL(cbat_list_copy(NULL));
    cbat_list_clear(NULL);
    cbat_list_reverse(NULL);
}

static int destroyed = 0;

static void count_destroy(void *element) {
    CBAT_UNUSED(element);
    destroyed++;
}

CBAT_TEST(test_destructor_runs_on_owned_removals) {
    CBatList *list = cbat_list_new(sizeof(int));
    int out;
    CBAT_REQUIRE_NOT_NULL(list);
    destroyed = 0;
    cbat_list_set_destructor(list, count_destroy);
    CBAT_LIST_PUSH(list, int, 1);
    CBAT_LIST_PUSH(list, int, 2);
    CBAT_LIST_PUSH(list, int, 3);
    CBAT_LIST_PUSH(list, int, 4);
    CBAT_LIST_SET(list, int, 0, 10);       /* destroys old 1 */
    cbat_list_pop(list, NULL);             /* destroys 4 */
    cbat_list_pop(list, &out);             /* moved out: no destroy */
    CBAT_CHECK_EQ_INT(destroyed, 2);
    cbat_list_resize(list, 1);             /* destroys 2 */
    CBAT_CHECK_EQ_INT(destroyed, 3);
    cbat_list_free(list);                  /* destroys 10 */
    CBAT_CHECK_EQ_INT(destroyed, 4);
}

CBAT_TEST(test_owning_list_of_strings) {
    CBatList *names = cbat_list_new(sizeof(char *));
    char *name;
    CBAT_REQUIRE_NOT_NULL(names);
    cbat_list_set_destructor(names, cbat_destroy_pointer);
    name = cbat_strdup(NULL, "ada");
    CBAT_REQUIRE_OK(cbat_list_push(names, &name));
    name = cbat_strdup(NULL, "grace");
    CBAT_REQUIRE_OK(cbat_list_push(names, &name));
    CBAT_CHECK_EQ_STR(CBAT_LIST_GET(names, char *, 1), "grace");
    cbat_list_free(names); /* leak checker verifies both strings were freed */
}

CBAT_TEST(test_extend_reserve_resize_shrink) {
    CBatList *list = cbat_list_new(sizeof(int));
    int values[] = {1, 2, 3};
    CBAT_REQUIRE_NOT_NULL(list);
    CBAT_CHECK_OK(cbat_list_extend(list, values, 3));
    CBAT_CHECK_OK(cbat_list_extend(list, NULL, 0));
    CBAT_CHECK_ERR(cbat_list_extend(list, NULL, 2), CBAT_ERR_INVALID_ARGUMENT);
    /* Extending a list from its own storage. */
    CBAT_CHECK_OK(cbat_list_extend(list, list->data, list->length));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(list), 6);
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 5), 3);
    CBAT_CHECK_OK(cbat_list_reserve(list, 100));
    CBAT_CHECK(list->capacity >= 106);
    CBAT_CHECK_OK(cbat_list_resize(list, 8));
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 7), 0);
    CBAT_CHECK_OK(cbat_list_shrink_to_fit(list));
    CBAT_CHECK_EQ_SIZE(list->capacity, 8);
    cbat_list_clear(list);
    CBAT_CHECK_OK(cbat_list_shrink_to_fit(list));
    CBAT_CHECK_EQ_SIZE(list->capacity, 0);
    CBAT_CHECK_ERR(cbat_list_reserve(list, SIZE_MAX), CBAT_ERR_OVERFLOW);
    cbat_list_free(list);
}

CBAT_TEST(test_push_element_from_own_storage) {
    CBatList *list = cbat_list_new(sizeof(int));
    int i;
    CBAT_REQUIRE_NOT_NULL(list);
    CBAT_LIST_PUSH(list, int, 42);
    for (i = 0; i < 20; i++) {
        CBAT_REQUIRE_OK(cbat_list_push(list, cbat_list_get(list, 0)));
    }
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 20), 42);
    cbat_list_free(list);
}

CBAT_TEST(test_copy_index_sort_reverse_swap) {
    CBatList *list = cbat_list_new(sizeof(int));
    CBatList *copy;
    int needle = 7;
    int absent = 99;
    int values[] = {9, 7, 1, 5};
    CBAT_REQUIRE_NOT_NULL(list);
    cbat_list_extend(list, values, 4);
    copy = cbat_list_copy(list);
    CBAT_REQUIRE_NOT_NULL(copy);
    CBAT_CHECK_EQ_SIZE(cbat_list_index_of(list, &needle, cbat_compare_int), 1);
    CBAT_CHECK(cbat_list_contains(list, &needle, cbat_compare_int));
    CBAT_CHECK(!cbat_list_contains(list, &absent, cbat_compare_int));
    CBAT_CHECK_OK(cbat_list_sort(list, cbat_compare_int));
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 0), 1);
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 3), 9);
    cbat_list_reverse(list);
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 0), 9);
    CBAT_CHECK_OK(cbat_list_swap(list, 0, 3));
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 0), 1);
    CBAT_CHECK_ERR(cbat_list_swap(list, 0, 4), CBAT_ERR_OUT_OF_RANGE);
    /* The copy is independent. */
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(copy, int, 0), 9);
    cbat_list_free(copy);
    cbat_list_free(list);
}

CBAT_TEST(test_foreach) {
    CBatList *list = cbat_list_new(sizeof(int));
    CBatList *empty = cbat_list_new(sizeof(int));
    int sum = 0;
    int visits = 0;
    CBAT_REQUIRE_NOT_NULL(list);
    CBAT_REQUIRE_NOT_NULL(empty);
    CBAT_LIST_PUSH(list, int, 1);
    CBAT_LIST_PUSH(list, int, 2);
    CBAT_LIST_PUSH(list, int, 3);
    CBAT_LIST_FOREACH(int, value, list) {
        sum += *value;
    }
    CBAT_LIST_FOREACH(int, value, empty) {
        CBAT_UNUSED(value);
        visits++;
    }
    CBAT_CHECK_EQ_INT(sum, 6);
    CBAT_CHECK_EQ_INT(visits, 0);
    cbat_list_free(list);
    cbat_list_free(empty);
}

CBAT_TEST(test_list_in_arena) {
    CBatArena *arena = cbat_arena_new(0);
    CBatList *list;
    int i;
    CBAT_REQUIRE_NOT_NULL(arena);
    list = cbat_list_new_with(sizeof(int), cbat_arena_allocator(arena));
    CBAT_REQUIRE_NOT_NULL(list);
    for (i = 0; i < 1000; i++) {
        CBAT_REQUIRE_OK(CBAT_LIST_PUSH(list, int, i));
    }
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 999), 999);
    cbat_arena_free(arena); /* frees the list too; no cbat_list_free needed */
}

CBAT_TEST(test_out_of_memory) {
    size_t n;
    for (n = 0; n < 4; n++) {
        CBatList *list;
        int i;
        CBatError err = CBAT_OK;
        cbat_test_fail_allocations_after(n);
        list = cbat_list_new(sizeof(int));
        if (list == NULL) {
            cbat_test_stop_failing_allocations();
            continue;
        }
        for (i = 0; i < 100 && err == CBAT_OK; i++) {
            err = CBAT_LIST_PUSH(list, int, i);
        }
        cbat_test_stop_failing_allocations();
        CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
        /* The list is still usable after a failed push. */
        CBAT_CHECK_OK(CBAT_LIST_PUSH(list, int, 1));
        cbat_list_free(list);
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_new_and_free),
    CBAT_TEST_ENTRY(test_push_get_and_macros),
    CBAT_TEST_ENTRY(test_struct_elements),
    CBAT_TEST_ENTRY(test_set_insert_remove),
    CBAT_TEST_ENTRY(test_pop_empty),
    CBAT_TEST_ENTRY(test_null_arguments),
    CBAT_TEST_ENTRY(test_destructor_runs_on_owned_removals),
    CBAT_TEST_ENTRY(test_owning_list_of_strings),
    CBAT_TEST_ENTRY(test_extend_reserve_resize_shrink),
    CBAT_TEST_ENTRY(test_push_element_from_own_storage),
    CBAT_TEST_ENTRY(test_copy_index_sort_reverse_swap),
    CBAT_TEST_ENTRY(test_foreach),
    CBAT_TEST_ENTRY(test_list_in_arena),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("list", tests)
