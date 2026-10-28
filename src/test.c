/*
 * cbat/test.c - the unit-test runner.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/test.h"

#include <math.h>
#include <string.h>

/* Counting allocator wrapped around the default allocator during a test. */
typedef struct cbat__test_allocs {
    const CBatAllocator *inner;
    size_t live;
    size_t made;
    size_t fail_after;
    bool failing;
} cbat__test_allocs;

struct CBatTestContext {
    const char *name;
    FILE *out;
    size_t failures;
};

static cbat__test_allocs *cbat__current_allocs = NULL;
static CBatTestContext *cbat__current_ctx = NULL;

static bool cbat__should_fail(cbat__test_allocs *s) {
    return s->failing && s->made >= s->fail_after;
}

static void *cbat__counting_alloc(void *ctx, size_t size) {
    cbat__test_allocs *s = (cbat__test_allocs *)ctx;
    void *ptr;
    if (cbat__should_fail(s)) {
        return NULL;
    }
    ptr = s->inner->alloc(s->inner->ctx, size);
    if (ptr != NULL) {
        s->live++;
        s->made++;
    }
    return ptr;
}

static void *cbat__counting_resize(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    cbat__test_allocs *s = (cbat__test_allocs *)ctx;
    void *fresh;
    if (cbat__should_fail(s)) {
        return NULL;
    }
    fresh = s->inner->resize(s->inner->ctx, ptr, old_size, new_size);
    if (fresh != NULL) {
        s->made++;
    }
    return fresh;
}

static void cbat__counting_release(void *ctx, void *ptr) {
    cbat__test_allocs *s = (cbat__test_allocs *)ctx;
    if (s->live > 0) {
        s->live--;
    }
    s->inner->release(s->inner->ctx, ptr);
}

void cbat_test_fail_allocations_after(size_t n) {
    if (cbat__current_allocs != NULL) {
        cbat__current_allocs->failing = true;
        cbat__current_allocs->fail_after = n;
        cbat__current_allocs->made = 0;
    }
}

void cbat_test_stop_failing_allocations(void) {
    if (cbat__current_allocs != NULL) {
        cbat__current_allocs->failing = false;
    }
}

size_t cbat_test_live_allocations(void) {
    return cbat__current_allocs != NULL ? cbat__current_allocs->live : 0;
}

/* ---- Reporting ---------------------------------------------------------- */

/* Checks used outside a running test report here instead of crashing. */
static CBatTestContext cbat__orphan_ctx = { "(outside a test)", NULL, 0 };

static CBatTestContext *cbat__context(void) {
    if (cbat__current_ctx != NULL) {
        return cbat__current_ctx;
    }
    cbat__orphan_ctx.out = stderr;
    return &cbat__orphan_ctx;
}

static void cbat__report_failure_header(CBatTestContext *t) {
    if (t->failures == 0 && t->out != NULL) {
        fprintf(t->out, "  FAIL  %s\n", t->name);
    }
    t->failures++;
}

static void cbat__report_location(CBatTestContext *t, const char *file, int line) {
    cbat__report_failure_header(t);
    if (t->out != NULL) {
        fprintf(t->out, "        %s:%d: ", file, line);
    }
}

bool cbat_test_check(bool ok, const char *file, int line,
                     const char *fmt, ...) {
    CBatTestContext *t = cbat__context();
    va_list args;
    if (ok) {
        return true;
    }
    cbat__report_location(t, file, line);
    if (t->out != NULL) {
        fputs("check failed: ", t->out);
        va_start(args, fmt);
        vfprintf(t->out, fmt, args);
        va_end(args);
        fputc('\n', t->out);
    }
    return false;
}

bool cbat_test_check_int(long long a, long long b,
                         const char *a_text, const char *b_text,
                         const char *file, int line) {
    CBatTestContext *t = cbat__context();
    if (a == b) {
        return true;
    }
    cbat__report_location(t, file, line);
    if (t->out != NULL) {
        fprintf(t->out, "%s == %s failed: %lld != %lld\n", a_text, b_text, a, b);
    }
    return false;
}

bool cbat_test_check_uint(unsigned long long a, unsigned long long b,
                          const char *a_text, const char *b_text,
                          const char *file, int line) {
    CBatTestContext *t = cbat__context();
    if (a == b) {
        return true;
    }
    cbat__report_location(t, file, line);
    if (t->out != NULL) {
        fprintf(t->out, "%s == %s failed: %llu != %llu\n", a_text, b_text, a, b);
    }
    return false;
}

static void cbat__print_quoted(FILE *out, const char *s) {
    if (s == NULL) {
        fputs("(null)", out);
    } else {
        fprintf(out, "\"%s\"", s);
    }
}

bool cbat_test_check_str(const char *a, const char *b,
                         const char *a_text, const char *b_text,
                         const char *file, int line) {
    CBatTestContext *t = cbat__context();
    if ((a == NULL && b == NULL) || (a != NULL && b != NULL && strcmp(a, b) == 0)) {
        return true;
    }
    cbat__report_location(t, file, line);
    if (t->out != NULL) {
        fprintf(t->out, "%s == %s failed: ", a_text, b_text);
        cbat__print_quoted(t->out, a);
        fputs(" != ", t->out);
        cbat__print_quoted(t->out, b);
        fputc('\n', t->out);
    }
    return false;
}

bool cbat_test_check_mem(const void *a, const void *b, size_t n,
                         const char *a_text, const char *b_text,
                         const char *file, int line) {
    CBatTestContext *t = cbat__context();
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    size_t i;
    if (a == NULL || b == NULL) {
        if (a == b) {
            return true;
        }
        cbat__report_location(t, file, line);
        if (t->out != NULL) {
            fprintf(t->out, "%s == %s failed: one pointer is NULL\n", a_text, b_text);
        }
        return false;
    }
    for (i = 0; i < n; i++) {
        if (pa[i] != pb[i]) {
            cbat__report_location(t, file, line);
            if (t->out != NULL) {
                fprintf(t->out, "%s == %s failed: first difference at byte %lu (0x%02x != 0x%02x)\n",
                        a_text, b_text, (unsigned long)i, pa[i], pb[i]);
            }
            return false;
        }
    }
    return true;
}

bool cbat_test_check_near(double a, double b, double tolerance,
                          const char *a_text, const char *b_text,
                          const char *file, int line) {
    CBatTestContext *t = cbat__context();
    if ((a != a && b != b) || fabs(a - b) <= tolerance || a == b) {
        return true;
    }
    cbat__report_location(t, file, line);
    if (t->out != NULL) {
        fprintf(t->out, "%s ~= %s failed: %.17g vs %.17g (tolerance %g)\n",
                a_text, b_text, a, b, tolerance);
    }
    return false;
}

bool cbat_test_check_err(CBatError actual, CBatError expected,
                         const char *expr_text, const char *file, int line) {
    CBatTestContext *t = cbat__context();
    if (actual == expected) {
        return true;
    }
    cbat__report_location(t, file, line);
    if (t->out != NULL) {
        fprintf(t->out, "%s returned %s (%s), expected %s\n", expr_text,
                cbat_error_name(actual), cbat_error_str(actual), cbat_error_name(expected));
    }
    return false;
}

/* ---- Runner ------------------------------------------------------------- */

CBatTestSummary cbat_test_run(const char *suite, const CBatTestCase *cases, size_t count,
                              const char *filter, FILE *out) {
    CBatTestSummary summary = {0, 0, 0};
    cbat__test_allocs *outer_allocs = cbat__current_allocs;
    CBatTestContext *outer_ctx = cbat__current_ctx;
    const CBatAllocator *outer_default = cbat_default_allocator();
    size_t i;

    if (out != NULL) {
        fprintf(out, "[%s] running %lu test(s)\n", suite, (unsigned long)count);
    }
    for (i = 0; i < count; i++) {
        CBatTestContext ctx;
        cbat__test_allocs allocs;
        CBatAllocator counting;

        if (filter != NULL && strstr(cases[i].name, filter) == NULL) {
            summary.skipped++;
            continue;
        }
        allocs.inner = outer_default;
        allocs.live = 0;
        allocs.made = 0;
        allocs.fail_after = 0;
        allocs.failing = false;
        counting.alloc = cbat__counting_alloc;
        counting.resize = cbat__counting_resize;
        counting.release = cbat__counting_release;
        counting.ctx = &allocs;

        ctx.name = cases[i].name;
        ctx.out = out;
        ctx.failures = 0;

        cbat__current_allocs = &allocs;
        cbat__current_ctx = &ctx;
        cbat_set_default_allocator(&counting);
        cases[i].func();
        cbat_set_default_allocator(outer_default);
        cbat__current_allocs = outer_allocs;
        cbat__current_ctx = outer_ctx;

        if (ctx.failures == 0 && allocs.live != 0) {
            cbat__report_failure_header(&ctx);
            if (out != NULL) {
                fprintf(out, "        leaked %lu cbat allocation(s)\n", (unsigned long)allocs.live);
            }
        }
        if (ctx.failures == 0) {
            summary.passed++;
            if (out != NULL) {
                fprintf(out, "  ok    %s\n", ctx.name);
            }
        } else {
            summary.failed++;
        }
    }
    if (out != NULL) {
        fprintf(out, "[%s] %lu passed, %lu failed", suite,
                (unsigned long)summary.passed, (unsigned long)summary.failed);
        if (summary.skipped > 0) {
            fprintf(out, ", %lu filtered out", (unsigned long)summary.skipped);
        }
        fputc('\n', out);
        fflush(out);
    }
    return summary;
}

int cbat_test_main(const char *suite, const CBatTestCase *cases, size_t count,
                   int argc, char **argv) {
    const char *filter = NULL;
    CBatTestSummary summary;
    if (argc > 1) {
        if (strcmp(argv[1], "--list") == 0) {
            size_t i;
            for (i = 0; i < count; i++) {
                printf("%s\n", cases[i].name);
            }
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("usage: %s [--list] [name-filter]\n", argv[0]);
            return 0;
        }
        filter = argv[1];
    }
    summary = cbat_test_run(suite, cases, count, filter, stdout);
    return summary.failed == 0 ? 0 : 1;
}
