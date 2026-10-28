#include "cbat/path.h"
#include "cbat/test.h"

/* Runs fn(arg) and compares the produced string, freeing it. */
#define CHECK_PATH(expr, expected)                       \
    do {                                                 \
        CBatString *result_ = (expr);                    \
        CBAT_CHECK_NOT_NULL(result_);                    \
        CBAT_CHECK_EQ_STR(cbat_string_cstr(result_), expected); \
        cbat_string_free(result_);                       \
    } while (0)

CBAT_TEST(test_is_absolute) {
    CBAT_CHECK(cbat_path_is_absolute("/usr/bin"));
    CBAT_CHECK(!cbat_path_is_absolute("usr/bin"));
    CBAT_CHECK(!cbat_path_is_absolute(""));
    CBAT_CHECK(!cbat_path_is_absolute(NULL));
#ifdef _WIN32
    CBAT_CHECK(cbat_path_is_absolute("C:\\Windows"));
    CBAT_CHECK(cbat_path_is_absolute("c:/x"));
    CBAT_CHECK(!cbat_path_is_absolute("C:relative"));
    CBAT_CHECK(cbat_path_is_separator('\\'));
#else
    CBAT_CHECK(!cbat_path_is_separator('\\'));
#endif
    CBAT_CHECK(cbat_path_is_separator('/'));
}

CBAT_TEST(test_join) {
    const char *parts[] = {"a", "b", "c.txt"};
    CHECK_PATH(cbat_path_join("a", "b"), "a/b");
    CHECK_PATH(cbat_path_join("a/", "b"), "a/b");
    CHECK_PATH(cbat_path_join("", "b"), "b");
    CHECK_PATH(cbat_path_join("a", ""), "a");
    CHECK_PATH(cbat_path_join("a", "/abs"), "/abs");
    CHECK_PATH(cbat_path_join(NULL, NULL), "");
    CHECK_PATH(cbat_path_join_many(parts, 3), "a/b/c.txt");
    CHECK_PATH(cbat_path_join_many(parts, 0), "");
}

CBAT_TEST(test_basename_dirname) {
    CHECK_PATH(cbat_path_basename("a/b.txt"), "b.txt");
    CHECK_PATH(cbat_path_basename("a/b/"), "b");
    CHECK_PATH(cbat_path_basename("b"), "b");
    CHECK_PATH(cbat_path_basename("/"), "");
    CHECK_PATH(cbat_path_basename(""), "");
    CHECK_PATH(cbat_path_dirname("a/b/c"), "a/b");
    CHECK_PATH(cbat_path_dirname("a/b/"), "a");
    CHECK_PATH(cbat_path_dirname("a//b"), "a");
    CHECK_PATH(cbat_path_dirname("b"), ".");
    CHECK_PATH(cbat_path_dirname("/a"), "/");
    CHECK_PATH(cbat_path_dirname("/"), "/");
    CHECK_PATH(cbat_path_dirname(""), ".");
#ifdef _WIN32
    CHECK_PATH(cbat_path_dirname("C:\\dir\\f.txt"), "C:\\dir");
    CHECK_PATH(cbat_path_dirname("C:\\f.txt"), "C:\\");
    CHECK_PATH(cbat_path_basename("C:\\dir\\f.txt"), "f.txt");
#endif
}

CBAT_TEST(test_extension_stem) {
    CHECK_PATH(cbat_path_extension("x.tar.gz"), ".gz");
    CHECK_PATH(cbat_path_extension("dir.d/file"), "");
    CHECK_PATH(cbat_path_extension(".bashrc"), "");
    CHECK_PATH(cbat_path_extension("trailing."), "");
    CHECK_PATH(cbat_path_stem("dir/x.tar.gz"), "x.tar");
    CHECK_PATH(cbat_path_stem(".bashrc"), ".bashrc");
    CHECK_PATH(cbat_path_stem("plain"), "plain");
}

CBAT_TEST(test_with_extension) {
    CHECK_PATH(cbat_path_with_extension("a/x.txt", ".md"), "a/x.md");
    CHECK_PATH(cbat_path_with_extension("a/x.txt", "md"), "a/x.md");
    CHECK_PATH(cbat_path_with_extension("a/x.txt", ""), "a/x");
    CHECK_PATH(cbat_path_with_extension("a/x", ".c"), "a/x.c");
    CHECK_PATH(cbat_path_with_extension("dir.d/x", ".c"), "dir.d/x.c");
}

CBAT_TEST(test_normalize) {
    CHECK_PATH(cbat_path_normalize("a/./b/../c"), "a/c");
    CHECK_PATH(cbat_path_normalize("a//b///c/"), "a/b/c");
    CHECK_PATH(cbat_path_normalize("/../x"), "/x");
    CHECK_PATH(cbat_path_normalize("../../x"), "../../x");
    CHECK_PATH(cbat_path_normalize("a/../.."), "..");
    CHECK_PATH(cbat_path_normalize("a/.."), ".");
    CHECK_PATH(cbat_path_normalize(""), ".");
    CHECK_PATH(cbat_path_normalize("/"), "/");
    CHECK_PATH(cbat_path_normalize("./"), ".");
#ifdef _WIN32
    CHECK_PATH(cbat_path_normalize("C:\\a\\..\\b"), "C:/b");
#endif
}

CBAT_TEST(test_out_of_memory) {
    size_t n;
    for (n = 0; n < 4; n++) {
        CBatString *s;
        cbat_test_fail_allocations_after(n);
        s = cbat_path_normalize("a/b/../c/./d");
        cbat_test_stop_failing_allocations();
        if (s != NULL) {
            CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "a/c/d");
            cbat_string_free(s);
        }
        cbat_test_fail_allocations_after(n);
        s = cbat_path_join("dir", "file");
        cbat_test_stop_failing_allocations();
        cbat_string_free(s);
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_is_absolute),
    CBAT_TEST_ENTRY(test_join),
    CBAT_TEST_ENTRY(test_basename_dirname),
    CBAT_TEST_ENTRY(test_extension_stem),
    CBAT_TEST_ENTRY(test_with_extension),
    CBAT_TEST_ENTRY(test_normalize),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("path", tests)
