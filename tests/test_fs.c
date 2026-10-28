#include <string.h>

#include "cbat/file.h"
#include "cbat/fs.h"
#include "cbat/path.h"
#include "cbat/test.h"

/* Every test works inside its own fresh temp directory. */
static CBatString *make_sandbox(void) {
    CBatString *dir = NULL;
    if (cbat_fs_make_temp_dir("cbat-test-", &dir) != CBAT_OK) {
        return NULL;
    }
    return dir;
}

static CBatString *in(const CBatString *dir, const char *name) {
    return cbat_path_join(cbat_string_cstr(dir), name);
}

CBAT_TEST(test_temp_and_current_dir) {
    CBatString *tmp = NULL;
    CBatString *cwd = NULL;
    CBAT_REQUIRE_OK(cbat_fs_temp_dir(&tmp));
    CBAT_CHECK(cbat_fs_is_dir(cbat_string_cstr(tmp)));
    CBAT_REQUIRE_OK(cbat_fs_current_dir(&cwd));
    CBAT_CHECK(cbat_fs_is_dir(cbat_string_cstr(cwd)));
    CBAT_CHECK(cbat_path_is_absolute(cbat_string_cstr(cwd)));
    cbat_string_free(tmp);
    cbat_string_free(cwd);
}

CBAT_TEST(test_make_temp_dir_is_unique) {
    CBatString *a = make_sandbox();
    CBatString *b = make_sandbox();
    CBAT_REQUIRE_NOT_NULL(a);
    CBAT_REQUIRE_NOT_NULL(b);
    CBAT_CHECK(!cbat_string_equals(a, cbat_string_cstr(b)));
    CBAT_CHECK(cbat_fs_is_dir(cbat_string_cstr(a)));
    CBAT_CHECK_OK(cbat_fs_remove_dir(cbat_string_cstr(a)));
    CBAT_CHECK_OK(cbat_fs_remove_dir(cbat_string_cstr(b)));
    CBAT_CHECK(!cbat_fs_exists(cbat_string_cstr(a)));
    cbat_string_free(a);
    cbat_string_free(b);
}

CBAT_TEST(test_exists_is_file_is_dir_size) {
    CBatString *dir = make_sandbox();
    CBatString *file;
    uint64_t size = 0;
    CBAT_REQUIRE_NOT_NULL(dir);
    file = in(dir, "f.txt");
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(file), "12345"));
    CBAT_CHECK(cbat_fs_exists(cbat_string_cstr(file)));
    CBAT_CHECK(cbat_fs_is_file(cbat_string_cstr(file)));
    CBAT_CHECK(!cbat_fs_is_dir(cbat_string_cstr(file)));
    CBAT_CHECK(cbat_fs_is_dir(cbat_string_cstr(dir)));
    CBAT_CHECK_OK(cbat_fs_file_size(cbat_string_cstr(file), &size));
    CBAT_CHECK_EQ_UINT(size, 5);
    CBAT_CHECK_ERR(cbat_fs_file_size(cbat_string_cstr(dir), &size), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK(!cbat_fs_exists("definitely/not/here.txt"));
    CBAT_CHECK(!cbat_fs_exists(NULL));
    CBAT_CHECK_ERR(cbat_fs_file_size("definitely/not/here.txt", &size), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_OK(cbat_fs_remove_tree(cbat_string_cstr(dir)));
    cbat_string_free(file);
    cbat_string_free(dir);
}

CBAT_TEST(test_make_dir_and_dirs) {
    CBatString *dir = make_sandbox();
    CBatString *one;
    CBatString *deep;
    CBAT_REQUIRE_NOT_NULL(dir);
    one = in(dir, "one");
    deep = in(dir, "x/y//z/");
    CBAT_CHECK_OK(cbat_fs_make_dir(cbat_string_cstr(one)));
    CBAT_CHECK_ERR(cbat_fs_make_dir(cbat_string_cstr(one)), CBAT_ERR_ALREADY_EXISTS);
    CBAT_CHECK_OK(cbat_fs_make_dirs(cbat_string_cstr(deep)));
    CBAT_CHECK(cbat_fs_is_dir(cbat_string_cstr(deep)));
    CBAT_CHECK_OK(cbat_fs_make_dirs(cbat_string_cstr(deep))); /* idempotent */
    CBAT_CHECK_ERR(cbat_fs_make_dir(""), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_fs_remove_dir(cbat_string_cstr(dir)), CBAT_ERR_NOT_EMPTY);
    CBAT_CHECK_OK(cbat_fs_remove_tree(cbat_string_cstr(dir)));
    CBAT_CHECK(!cbat_fs_exists(cbat_string_cstr(dir)));
    cbat_string_free(one);
    cbat_string_free(deep);
    cbat_string_free(dir);
}

CBAT_TEST(test_make_dirs_over_file_fails) {
    CBatString *dir = make_sandbox();
    CBatString *file;
    CBatString *below;
    CBAT_REQUIRE_NOT_NULL(dir);
    file = in(dir, "plain");
    below = in(dir, "plain/sub");
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(file), "x"));
    CBAT_CHECK(cbat_fs_make_dirs(cbat_string_cstr(below)) != CBAT_OK);
    CBAT_CHECK_OK(cbat_fs_remove_tree(cbat_string_cstr(dir)));
    cbat_string_free(file);
    cbat_string_free(below);
    cbat_string_free(dir);
}

CBAT_TEST(test_list_dir_sorted) {
    CBatString *dir = make_sandbox();
    CBatList *names = NULL;
    const char *files[] = {"b.txt", "a.txt", "c.txt"};
    size_t i;
    CBAT_REQUIRE_NOT_NULL(dir);
    for (i = 0; i < 3; i++) {
        CBatString *p = in(dir, files[i]);
        CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(p), files[i]));
        cbat_string_free(p);
    }
    {
        CBatString *sub = in(dir, "sub");
        CBAT_REQUIRE_OK(cbat_fs_make_dir(cbat_string_cstr(sub)));
        cbat_string_free(sub);
    }
    CBAT_REQUIRE_OK(cbat_fs_list_dir(cbat_string_cstr(dir), &names));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(names), 4);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(CBAT_LIST_GET(names, CBatString *, 0)), "a.txt");
    CBAT_CHECK_EQ_STR(cbat_string_cstr(CBAT_LIST_GET(names, CBatString *, 3)), "sub");
    cbat_list_free(names);
    CBAT_CHECK_ERR(cbat_fs_list_dir("definitely/not/here", &names), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_OK(cbat_fs_remove_tree(cbat_string_cstr(dir)));
    cbat_string_free(dir);
}

CBAT_TEST(test_rename_and_remove_file) {
    CBatString *dir = make_sandbox();
    CBatString *a;
    CBatString *b;
    CBatString *content = NULL;
    CBAT_REQUIRE_NOT_NULL(dir);
    a = in(dir, "a");
    b = in(dir, "b");
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(a), "new"));
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(b), "old"));
    CBAT_CHECK_OK(cbat_fs_rename(cbat_string_cstr(a), cbat_string_cstr(b))); /* replaces */
    CBAT_CHECK(!cbat_fs_exists(cbat_string_cstr(a)));
    CBAT_REQUIRE_OK(cbat_file_read(cbat_string_cstr(b), &content));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(content), "new");
    cbat_string_free(content);
    CBAT_CHECK_ERR(cbat_fs_rename(cbat_string_cstr(a), cbat_string_cstr(b)), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_OK(cbat_fs_remove_file(cbat_string_cstr(b)));
    CBAT_CHECK_ERR(cbat_fs_remove_file(cbat_string_cstr(b)), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_OK(cbat_fs_remove_dir(cbat_string_cstr(dir)));
    cbat_string_free(a);
    cbat_string_free(b);
    cbat_string_free(dir);
}

typedef struct WalkState {
    int files;
    int dirs;
    int limit;
} WalkState;

static bool count_entries(const char *path, bool is_dir, void *user_data) {
    WalkState *state = (WalkState *)user_data;
    CBAT_UNUSED(path);
    if (is_dir) {
        state->dirs++;
    } else {
        state->files++;
    }
    return state->limit == 0 || state->files + state->dirs < state->limit;
}

CBAT_TEST(test_walk) {
    CBatString *dir = make_sandbox();
    CBatString *deep;
    CBatString *f1;
    CBatString *f2;
    WalkState state = {0, 0, 0};
    CBAT_REQUIRE_NOT_NULL(dir);
    deep = in(dir, "a/b");
    f1 = in(dir, "a/one.c");
    f2 = in(dir, "a/b/two.c");
    CBAT_REQUIRE_OK(cbat_fs_make_dirs(cbat_string_cstr(deep)));
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(f1), "1"));
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(f2), "2"));
    CBAT_REQUIRE_OK(cbat_fs_walk(cbat_string_cstr(dir), count_entries, &state));
    CBAT_CHECK_EQ_INT(state.dirs, 2);
    CBAT_CHECK_EQ_INT(state.files, 2);
    state.files = state.dirs = 0;
    state.limit = 2; /* stop early */
    CBAT_REQUIRE_OK(cbat_fs_walk(cbat_string_cstr(dir), count_entries, &state));
    CBAT_CHECK_EQ_INT(state.files + state.dirs, 2);
    CBAT_CHECK_ERR(cbat_fs_walk(NULL, count_entries, &state), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_OK(cbat_fs_remove_tree(cbat_string_cstr(dir)));
    cbat_string_free(deep);
    cbat_string_free(f1);
    cbat_string_free(f2);
    cbat_string_free(dir);
}

CBAT_TEST(test_unicode_file_names) {
    CBatString *dir = make_sandbox();
    CBatString *file;
    CBatList *names = NULL;
    CBAT_REQUIRE_NOT_NULL(dir);
    file = in(dir, "caf\xC3\xA9-\xE2\x98\x83.txt"); /* café-☃.txt */
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(file), "snow"));
    CBAT_CHECK(cbat_fs_is_file(cbat_string_cstr(file)));
    CBAT_REQUIRE_OK(cbat_fs_list_dir(cbat_string_cstr(dir), &names));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(names), 1);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(CBAT_LIST_GET(names, CBatString *, 0)),
                      "caf\xC3\xA9-\xE2\x98\x83.txt");
    cbat_list_free(names);
    CBAT_CHECK_OK(cbat_fs_remove_tree(cbat_string_cstr(dir)));
    cbat_string_free(file);
    cbat_string_free(dir);
}

CBAT_TEST(test_remove_tree_errors) {
    CBAT_CHECK_ERR(cbat_fs_remove_tree("definitely/not/here"), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_fs_remove_tree(""), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_fs_remove_tree(NULL), CBAT_ERR_INVALID_ARGUMENT);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_temp_and_current_dir),
    CBAT_TEST_ENTRY(test_make_temp_dir_is_unique),
    CBAT_TEST_ENTRY(test_exists_is_file_is_dir_size),
    CBAT_TEST_ENTRY(test_make_dir_and_dirs),
    CBAT_TEST_ENTRY(test_make_dirs_over_file_fails),
    CBAT_TEST_ENTRY(test_list_dir_sorted),
    CBAT_TEST_ENTRY(test_rename_and_remove_file),
    CBAT_TEST_ENTRY(test_walk),
    CBAT_TEST_ENTRY(test_unicode_file_names),
    CBAT_TEST_ENTRY(test_remove_tree_errors),
};

CBAT_TEST_MAIN("fs", tests)
