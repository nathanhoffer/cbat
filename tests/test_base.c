#include <string.h>

#include "cbat/test.h"

/* ---- Errors ------------------------------------------------------------- */

CBAT_TEST(test_error_str_covers_every_code) {
    int i;
    for (i = 0; i < CBAT_ERROR_COUNT; i++) {
        const char *s = cbat_error_str((CBatError)i);
        CBAT_CHECK_NOT_NULL(s);
        CBAT_CHECK(strlen(s) > 0);
        CBAT_CHECK(strncmp(cbat_error_name((CBatError)i), "CBAT_", 5) == 0);
    }
    CBAT_CHECK_EQ_STR(cbat_error_str(CBAT_OK), "success");
    CBAT_CHECK_EQ_STR(cbat_error_name(CBAT_ERR_NOT_FOUND), "CBAT_ERR_NOT_FOUND");
}

CBAT_TEST(test_error_str_unknown_codes) {
    CBAT_CHECK_EQ_STR(cbat_error_str(CBAT_ERROR_COUNT), "unknown error");
    CBAT_CHECK_EQ_STR(cbat_error_str((CBatError)-1), "unknown error");
    CBAT_CHECK_EQ_STR(cbat_error_name((CBatError)999), "CBAT_ERR_UNKNOWN");
}

/* ---- Allocation --------------------------------------------------------- */

CBAT_TEST(test_alloc_and_release) {
    unsigned char *p = (unsigned char *)cbat_alloc(NULL, 16);
    CBAT_REQUIRE_NOT_NULL(p);
    memset(p, 0xAB, 16);
    cbat_release(NULL, p);
    cbat_release(NULL, NULL); /* no-op */
    cbat_free(NULL);
}

CBAT_TEST(test_alloc_zero_size_returns_pointer) {
    void *p = cbat_alloc(NULL, 0);
    CBAT_CHECK_NOT_NULL(p);
    cbat_free(p);
}

CBAT_TEST(test_alloc_zeroed) {
    size_t i;
    unsigned char *p = (unsigned char *)cbat_alloc_zeroed(NULL, 64);
    CBAT_REQUIRE_NOT_NULL(p);
    for (i = 0; i < 64; i++) {
        CBAT_CHECK_EQ_INT(p[i], 0);
    }
    cbat_free(p);
}

CBAT_TEST(test_resize_preserves_contents) {
    char *p = (char *)cbat_resize(NULL, NULL, 0, 4);
    CBAT_REQUIRE_NOT_NULL(p);
    memcpy(p, "abc", 4);
    p = (char *)cbat_resize(NULL, p, 4, 1024);
    CBAT_REQUIRE_NOT_NULL(p);
    CBAT_CHECK_EQ_STR(p, "abc");
    cbat_free(p);
}

CBAT_TEST(test_strdup_and_strndup) {
    char *a = cbat_strdup(NULL, "hello");
    char *b = cbat_strndup(NULL, "hello world", 5);
    char *c = cbat_strndup(NULL, "hi", 100);
    CBAT_CHECK_EQ_STR(a, "hello");
    CBAT_CHECK_EQ_STR(b, "hello");
    CBAT_CHECK_EQ_STR(c, "hi");
    CBAT_CHECK_NULL(cbat_strdup(NULL, NULL));
    CBAT_CHECK_NULL(cbat_strndup(NULL, NULL, 3));
    cbat_free(a);
    cbat_free(b);
    cbat_free(c);
}

CBAT_TEST(test_strdup_out_of_memory) {
    cbat_test_fail_allocations_after(0);
    CBAT_CHECK_NULL(cbat_strdup(NULL, "x"));
    CBAT_CHECK_NULL(cbat_alloc(NULL, 8));
    cbat_test_stop_failing_allocations();
}

static size_t custom_allocs = 0;

static void *custom_alloc(void *ctx, size_t size) {
    CBAT_UNUSED(ctx);
    custom_allocs++;
    return cbat_heap_allocator()->alloc(NULL, size);
}

static void *custom_resize(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    CBAT_UNUSED(ctx);
    return cbat_heap_allocator()->resize(NULL, ptr, old_size, new_size);
}

static void custom_release(void *ctx, void *ptr) {
    CBAT_UNUSED(ctx);
    cbat_heap_allocator()->release(NULL, ptr);
}

CBAT_TEST(test_custom_allocator_is_used) {
    CBatAllocator custom = { custom_alloc, custom_resize, custom_release, NULL };
    void *p;
    custom_allocs = 0;
    p = cbat_alloc(&custom, 10);
    CBAT_CHECK_NOT_NULL(p);
    CBAT_CHECK_EQ_SIZE(custom_allocs, 1);
    cbat_release(&custom, p);
}

CBAT_TEST(test_set_default_allocator_round_trip) {
    const CBatAllocator *before = cbat_default_allocator();
    CBatAllocator custom = { custom_alloc, custom_resize, custom_release, NULL };
    void *p;
    custom_allocs = 0;
    cbat_set_default_allocator(&custom);
    CBAT_CHECK(cbat_default_allocator() == &custom);
    p = cbat_alloc(NULL, 4);
    cbat_free(p);
    CBAT_CHECK_EQ_SIZE(custom_allocs, 1);
    cbat_set_default_allocator(NULL);
    CBAT_CHECK(cbat_default_allocator() == cbat_heap_allocator());
    cbat_set_default_allocator(before);
}

/* ---- Checked arithmetic ------------------------------------------------- */

CBAT_TEST(test_size_add) {
    size_t out = 0;
    CBAT_CHECK(cbat_size_add(2, 3, &out));
    CBAT_CHECK_EQ_SIZE(out, 5);
    CBAT_CHECK(cbat_size_add(SIZE_MAX - 1, 1, &out));
    CBAT_CHECK_EQ_SIZE(out, SIZE_MAX);
    CBAT_CHECK(!cbat_size_add(SIZE_MAX, 1, &out));
}

CBAT_TEST(test_size_mul) {
    size_t out = 0;
    CBAT_CHECK(cbat_size_mul(6, 7, &out));
    CBAT_CHECK_EQ_SIZE(out, 42);
    CBAT_CHECK(cbat_size_mul(0, SIZE_MAX, &out));
    CBAT_CHECK_EQ_SIZE(out, 0);
    CBAT_CHECK(!cbat_size_mul(SIZE_MAX / 2 + 1, 2, &out));
}

/* ---- Arena -------------------------------------------------------------- */

CBAT_TEST(test_arena_alloc_aligned_and_distinct) {
    CBatArena *arena = cbat_arena_new(128);
    char *a;
    double *b;
    CBAT_REQUIRE_NOT_NULL(arena);
    a = (char *)cbat_arena_alloc(arena, 3);
    b = (double *)cbat_arena_alloc(arena, sizeof(double));
    CBAT_REQUIRE_NOT_NULL(a);
    CBAT_REQUIRE_NOT_NULL(b);
    CBAT_CHECK((void *)a != (void *)b);
    CBAT_CHECK(((uintptr_t)b % sizeof(double)) == 0);
    *b = 3.5;
    memcpy(a, "hi", 3);
    CBAT_CHECK_EQ_STR(a, "hi");
    CBAT_CHECK_EQ_SIZE(cbat_arena_bytes_used(arena), 3 + sizeof(double));
    cbat_arena_free(arena);
}

CBAT_TEST(test_arena_grows_new_blocks_and_large_allocations) {
    CBatArena *arena = cbat_arena_new(64);
    int i;
    char *big;
    CBAT_REQUIRE_NOT_NULL(arena);
    for (i = 0; i < 100; i++) {
        CBAT_REQUIRE_NOT_NULL(cbat_arena_alloc(arena, 24));
    }
    big = (char *)cbat_arena_alloc(arena, 10000);
    CBAT_REQUIRE_NOT_NULL(big);
    memset(big, 1, 10000);
    cbat_arena_free(arena);
}

CBAT_TEST(test_arena_reset_reuses_memory) {
    CBatArena *arena = cbat_arena_new(256);
    void *first;
    void *again;
    int i;
    CBAT_REQUIRE_NOT_NULL(arena);
    first = cbat_arena_alloc(arena, 16);
    for (i = 0; i < 50; i++) {
        cbat_arena_alloc(arena, 100);
    }
    cbat_arena_reset(arena);
    CBAT_CHECK_EQ_SIZE(cbat_arena_bytes_used(arena), 0);
    again = cbat_arena_alloc(arena, 16);
    CBAT_CHECK_NOT_NULL(again);
    CBAT_CHECK(first != NULL);
    cbat_arena_free(arena);
}

CBAT_TEST(test_arena_strdup) {
    CBatArena *arena = cbat_arena_new(0);
    CBAT_REQUIRE_NOT_NULL(arena);
    CBAT_CHECK_EQ_STR(cbat_arena_strdup(arena, "arena!"), "arena!");
    CBAT_CHECK_NULL(cbat_arena_strdup(arena, NULL));
    cbat_arena_free(arena);
}

CBAT_TEST(test_arena_as_allocator) {
    CBatArena *arena = cbat_arena_new(0);
    const CBatAllocator *a;
    char *p;
    char *q;
    CBAT_REQUIRE_NOT_NULL(arena);
    a = cbat_arena_allocator(arena);
    CBAT_REQUIRE_NOT_NULL(a);
    p = (char *)cbat_alloc(a, 8);
    CBAT_REQUIRE_NOT_NULL(p);
    memcpy(p, "1234567", 8);
    /* Last allocation grows in place. */
    q = (char *)cbat_resize(a, p, 8, 64);
    CBAT_CHECK(p == q);
    CBAT_CHECK_EQ_STR(q, "1234567");
    /* Not the last allocation any more: resize copies. */
    CBAT_REQUIRE_NOT_NULL(cbat_alloc(a, 4));
    p = (char *)cbat_resize(a, q, 64, 128);
    CBAT_REQUIRE_NOT_NULL(p);
    CBAT_CHECK(p != q);
    CBAT_CHECK_EQ_STR(p, "1234567");
    cbat_release(a, p); /* no-op, must not crash */
    cbat_arena_free(arena);
}

CBAT_TEST(test_arena_null_safety) {
    cbat_arena_free(NULL);
    cbat_arena_reset(NULL);
    CBAT_CHECK_NULL(cbat_arena_alloc(NULL, 4));
    CBAT_CHECK_EQ_SIZE(cbat_arena_bytes_used(NULL), 0);
    CBAT_CHECK_NULL(cbat_arena_allocator(NULL));
}

CBAT_TEST(test_arena_out_of_memory) {
    size_t n;
    for (n = 0; n < 3; n++) {
        CBatArena *arena;
        cbat_test_fail_allocations_after(n);
        arena = cbat_arena_new(32);
        if (arena != NULL) {
            /* Fails once the arena needs a block it cannot get. */
            void *p = cbat_arena_alloc(arena, 16);
            CBAT_CHECK(n >= 2 ? p != NULL : p == NULL);
            cbat_test_stop_failing_allocations();
            cbat_arena_free(arena);
        } else {
            CBAT_CHECK_EQ_SIZE(n, 0);
        }
        cbat_test_stop_failing_allocations();
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_error_str_covers_every_code),
    CBAT_TEST_ENTRY(test_error_str_unknown_codes),
    CBAT_TEST_ENTRY(test_alloc_and_release),
    CBAT_TEST_ENTRY(test_alloc_zero_size_returns_pointer),
    CBAT_TEST_ENTRY(test_alloc_zeroed),
    CBAT_TEST_ENTRY(test_resize_preserves_contents),
    CBAT_TEST_ENTRY(test_strdup_and_strndup),
    CBAT_TEST_ENTRY(test_strdup_out_of_memory),
    CBAT_TEST_ENTRY(test_custom_allocator_is_used),
    CBAT_TEST_ENTRY(test_set_default_allocator_round_trip),
    CBAT_TEST_ENTRY(test_size_add),
    CBAT_TEST_ENTRY(test_size_mul),
    CBAT_TEST_ENTRY(test_arena_alloc_aligned_and_distinct),
    CBAT_TEST_ENTRY(test_arena_grows_new_blocks_and_large_allocations),
    CBAT_TEST_ENTRY(test_arena_reset_reuses_memory),
    CBAT_TEST_ENTRY(test_arena_strdup),
    CBAT_TEST_ENTRY(test_arena_as_allocator),
    CBAT_TEST_ENTRY(test_arena_null_safety),
    CBAT_TEST_ENTRY(test_arena_out_of_memory),
};

CBAT_TEST_MAIN("base", tests)
