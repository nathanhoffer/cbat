/*
 * cbat/test.h - a small unit-test framework (cbat's answer to unittest).
 *
 *     #include "cbat/test.h"
 *
 *     CBAT_TEST(test_addition) {
 *         CBAT_CHECK_EQ_INT(1 + 1, 2);
 *     }
 *
 *     static const CBatTestCase tests[] = { CBAT_TEST_ENTRY(test_addition) };
 *     CBAT_TEST_MAIN("math", tests)
 *
 * The runner wraps the default allocator around every test and fails any
 * test that leaks cbat allocations. cbat_test_fail_allocations_after() lets a
 * test force allocations to fail so out-of-memory paths can be exercised.
 * Tracking and injection use global state: run tests serially, with no
 * concurrent cbat calls from other threads.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_TEST_H
#define CBAT_TEST_H

#include <stdio.h>

#include "cbat/base.h"

CBAT_BEGIN_DECLS

typedef struct CBatTestContext CBatTestContext;
typedef void (*CBatTestFunc)(void);

typedef struct CBatTestCase {
    const char *name;
    CBatTestFunc func;
} CBatTestCase;

typedef struct CBatTestSummary {
    size_t passed;
    size_t failed;
    size_t skipped; /* filtered out */
} CBatTestSummary;

/** Declares a test function. The body can use the CBAT_CHECK* macros. */
#define CBAT_TEST(name) static void name(void)

/** An entry for a CBatTestCase array, named after the function. */
#define CBAT_TEST_ENTRY(func) { #func, func }

/** Defines main() running every case in `cases` (a static array). */
#define CBAT_TEST_MAIN(suite, cases)                                          \
    int main(int argc, char **argv) {                                         \
        return cbat_test_main((suite), (cases), CBAT_ARRAY_LEN(cases), argc, argv); \
    }

/**
 * Runs the cases whose name contains `filter` (NULL = all), writing a report
 * to `out` (NULL = silent). Returns the pass/fail counts.
 */
CBAT_API CBatTestSummary cbat_test_run(const char *suite, const CBatTestCase *cases,
                                       size_t count, const char *filter, FILE *out);

/**
 * Command-line entry point: `prog [filter]` or `prog --list`.
 * Returns 0 when every selected test passed, 1 otherwise.
 */
CBAT_API int cbat_test_main(const char *suite, const CBatTestCase *cases,
                            size_t count, int argc, char **argv);

/* ---- Allocation control (only meaningful inside a running test) -------- */

/** Lets the next `n` allocations succeed; every allocation after that fails. */
CBAT_API void cbat_test_fail_allocations_after(size_t n);

/** Turns allocation-failure injection off again. */
CBAT_API void cbat_test_stop_failing_allocations(void);

/** Number of cbat allocations currently live in this test. */
CBAT_API size_t cbat_test_live_allocations(void);

/* ---- Check primitives (use the macros below instead) ------------------- */

CBAT_API bool cbat_test_check(bool ok, const char *file, int line,
                              const char *fmt, ...) CBAT_PRINTF_FORMAT(4, 5);
CBAT_API bool cbat_test_check_int(long long a, long long b,
                                  const char *a_text, const char *b_text,
                                  const char *file, int line);
CBAT_API bool cbat_test_check_uint(unsigned long long a,
                                   unsigned long long b, const char *a_text,
                                   const char *b_text, const char *file, int line);
CBAT_API bool cbat_test_check_str(const char *a, const char *b,
                                  const char *a_text, const char *b_text,
                                  const char *file, int line);
CBAT_API bool cbat_test_check_mem(const void *a, const void *b,
                                  size_t n, const char *a_text, const char *b_text,
                                  const char *file, int line);
CBAT_API bool cbat_test_check_near(double a, double b,
                                   double tolerance, const char *a_text,
                                   const char *b_text, const char *file, int line);
CBAT_API bool cbat_test_check_err(CBatError actual,
                                  CBatError expected, const char *expr_text,
                                  const char *file, int line);

/* ---- Check macros ------------------------------------------------------- */
/* CHECK records a failure and continues; REQUIRE also returns from the test. */

#define CBAT_CHECK(cond) \
    cbat_test_check((cond) ? true : false, __FILE__, __LINE__, "%s", #cond)
#define CBAT_REQUIRE(cond) \
    do { if (!CBAT_CHECK(cond)) return; } while (0)

#define CBAT_FAIL(message) \
    cbat_test_check(false, __FILE__, __LINE__, "%s", (message))

#define CBAT_CHECK_EQ_INT(a, b) \
    cbat_test_check_int((long long)(a), (long long)(b), #a, #b, __FILE__, __LINE__)
#define CBAT_REQUIRE_EQ_INT(a, b) \
    do { if (!CBAT_CHECK_EQ_INT(a, b)) return; } while (0)

#define CBAT_CHECK_EQ_UINT(a, b) \
    cbat_test_check_uint((unsigned long long)(a), (unsigned long long)(b), \
                         #a, #b, __FILE__, __LINE__)
#define CBAT_REQUIRE_EQ_UINT(a, b) \
    do { if (!CBAT_CHECK_EQ_UINT(a, b)) return; } while (0)
#define CBAT_CHECK_EQ_SIZE(a, b) CBAT_CHECK_EQ_UINT(a, b)
#define CBAT_REQUIRE_EQ_SIZE(a, b) CBAT_REQUIRE_EQ_UINT(a, b)

#define CBAT_CHECK_EQ_STR(a, b) \
    cbat_test_check_str((a), (b), #a, #b, __FILE__, __LINE__)
#define CBAT_REQUIRE_EQ_STR(a, b) \
    do { if (!CBAT_CHECK_EQ_STR(a, b)) return; } while (0)

#define CBAT_CHECK_EQ_MEM(a, b, n) \
    cbat_test_check_mem((a), (b), (n), #a, #b, __FILE__, __LINE__)

#define CBAT_CHECK_NEAR(a, b, tolerance) \
    cbat_test_check_near((a), (b), (tolerance), #a, #b, __FILE__, __LINE__)

#define CBAT_CHECK_ERR(expr, expected) \
    cbat_test_check_err((expr), (expected), #expr, __FILE__, __LINE__)
#define CBAT_REQUIRE_ERR(expr, expected) \
    do { if (!CBAT_CHECK_ERR(expr, expected)) return; } while (0)
#define CBAT_CHECK_OK(expr) CBAT_CHECK_ERR(expr, CBAT_OK)
#define CBAT_REQUIRE_OK(expr) CBAT_REQUIRE_ERR(expr, CBAT_OK)

#define CBAT_CHECK_NULL(ptr) CBAT_CHECK((ptr) == NULL)
#define CBAT_CHECK_NOT_NULL(ptr) CBAT_CHECK((ptr) != NULL)
#define CBAT_REQUIRE_NOT_NULL(ptr) CBAT_REQUIRE((ptr) != NULL)

CBAT_END_DECLS

#endif /* CBAT_TEST_H */
