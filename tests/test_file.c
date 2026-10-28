#include <string.h>

#include "cbat/file.h"
#include "cbat/fs.h"
#include "cbat/path.h"
#include "cbat/test.h"

static CBatString *sandbox = NULL;

static CBatString *temp_path(const char *name) {
    return sandbox == NULL ? NULL : cbat_path_join(cbat_string_cstr(sandbox), name);
}

CBAT_TEST(test_write_and_read_round_trip) {
    CBatString *path = temp_path("round.bin");
    CBatString *content = NULL;
    const char bytes[] = {'a', '\0', 'b', '\n', '\r', 'c'};
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write(cbat_string_cstr(path), bytes, sizeof(bytes)));
    CBAT_REQUIRE_OK(cbat_file_read(cbat_string_cstr(path), &content));
    CBAT_CHECK_EQ_SIZE(content->length, sizeof(bytes));
    CBAT_CHECK_EQ_MEM(content->data, bytes, sizeof(bytes));
    cbat_string_free(content);
    cbat_string_free(path);
}

CBAT_TEST(test_read_large_file) {
    CBatString *path = temp_path("large.txt");
    CBatString *big = cbat_string_repeat("0123456789", 20000); /* 200 KB: several chunks */
    CBatString *content = NULL;
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_NOT_NULL(big);
    CBAT_REQUIRE_OK(cbat_file_write(cbat_string_cstr(path), big->data, big->length));
    CBAT_REQUIRE_OK(cbat_file_read(cbat_string_cstr(path), &content));
    CBAT_CHECK_EQ_SIZE(content->length, 200000);
    CBAT_CHECK(cbat_string_compare(content, big) == 0);
    cbat_string_free(content);
    cbat_string_free(big);
    cbat_string_free(path);
}

CBAT_TEST(test_empty_file) {
    CBatString *path = temp_path("empty.txt");
    CBatString *content = NULL;
    CBatList *lines = NULL;
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write(cbat_string_cstr(path), NULL, 0));
    CBAT_REQUIRE_OK(cbat_file_read(cbat_string_cstr(path), &content));
    CBAT_CHECK_EQ_SIZE(content->length, 0);
    CBAT_REQUIRE_OK(cbat_file_read_lines(cbat_string_cstr(path), &lines));
    CBAT_CHECK_EQ_SIZE(cbat_list_length(lines), 0);
    cbat_list_free(lines);
    cbat_string_free(content);
    cbat_string_free(path);
}

CBAT_TEST(test_missing_file_errors) {
    CBatString *content = NULL;
    CBatList *lines = NULL;
    CBatLineReader *reader = NULL;
    CBAT_CHECK_ERR(cbat_file_read("no/such/file.txt", &content), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_file_read_lines("no/such/file.txt", &lines), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_line_reader_open("no/such/file.txt", &reader), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_file_read(NULL, &content), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_file_write("x", NULL, 3), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_file_write_text("x", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_file_copy("no/such/file.txt", "x"), CBAT_ERR_NOT_FOUND);
}

CBAT_TEST(test_append) {
    CBatString *path = temp_path("append.txt");
    CBatString *content = NULL;
    CBAT_REQUIRE_NOT_NULL(path);
    cbat_fs_remove_file(cbat_string_cstr(path));
    CBAT_REQUIRE_OK(cbat_file_append(cbat_string_cstr(path), "one\n", 4));
    CBAT_REQUIRE_OK(cbat_file_append(cbat_string_cstr(path), "two\n", 4));
    CBAT_REQUIRE_OK(cbat_file_read(cbat_string_cstr(path), &content));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(content), "one\ntwo\n");
    cbat_string_free(content);
    cbat_string_free(path);
}

CBAT_TEST(test_read_lines_strips_endings) {
    CBatString *path = temp_path("lines.txt");
    CBatList *lines = NULL;
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(path), "alpha\r\nbeta\n\ngamma"));
    CBAT_REQUIRE_OK(cbat_file_read_lines(cbat_string_cstr(path), &lines));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(lines), 4);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(CBAT_LIST_GET(lines, CBatString *, 0)), "alpha");
    CBAT_CHECK_EQ_STR(cbat_string_cstr(CBAT_LIST_GET(lines, CBatString *, 1)), "beta");
    CBAT_CHECK_EQ_STR(cbat_string_cstr(CBAT_LIST_GET(lines, CBatString *, 2)), "");
    CBAT_CHECK_EQ_STR(cbat_string_cstr(CBAT_LIST_GET(lines, CBatString *, 3)), "gamma");
    cbat_list_free(lines);
    cbat_string_free(path);
}

CBAT_TEST(test_line_reader_numbers_and_end) {
    CBatString *path = temp_path("reader.txt");
    CBatLineReader *reader = NULL;
    const char *line;
    size_t length;
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(path), "x\nyy\n"));
    CBAT_REQUIRE_OK(cbat_line_reader_open(cbat_string_cstr(path), &reader));
    CBAT_CHECK_EQ_SIZE(cbat_line_reader_line_number(reader), 0);
    CBAT_REQUIRE_OK(cbat_line_reader_next(reader, &line, &length));
    CBAT_CHECK_EQ_STR(line, "x");
    CBAT_REQUIRE_OK(cbat_line_reader_next(reader, &line, &length));
    CBAT_CHECK_EQ_STR(line, "yy");
    CBAT_CHECK_EQ_SIZE(length, 2);
    CBAT_CHECK_EQ_SIZE(cbat_line_reader_line_number(reader), 2);
    CBAT_CHECK_ERR(cbat_line_reader_next(reader, &line, &length), CBAT_ERR_END);
    CBAT_CHECK_ERR(cbat_line_reader_next(reader, &line, &length), CBAT_ERR_END);
    cbat_line_reader_close(reader);
    cbat_line_reader_close(NULL);
    cbat_string_free(path);
}

CBAT_TEST(test_line_reader_from_open_file) {
    CBatString *path = temp_path("from_file.txt");
    FILE *file;
    CBatLineReader *reader = NULL;
    const char *line;
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(path), "only line"));
    file = cbat_fs_open(cbat_string_cstr(path), "rb");
    CBAT_REQUIRE_NOT_NULL(file);
    CBAT_REQUIRE_OK(cbat_line_reader_from_file(file, &reader));
    CBAT_REQUIRE_OK(cbat_line_reader_next(reader, &line, NULL));
    CBAT_CHECK_EQ_STR(line, "only line");
    cbat_line_reader_close(reader);
    fclose(file); /* the reader did not own it */
    CBAT_CHECK_ERR(cbat_line_reader_from_file(NULL, &reader), CBAT_ERR_INVALID_ARGUMENT);
    cbat_string_free(path);
}

CBAT_TEST(test_copy) {
    CBatString *from = temp_path("copy_src.txt");
    CBatString *to = temp_path("copy_dst.txt");
    CBatString *content = NULL;
    CBAT_REQUIRE_NOT_NULL(from);
    CBAT_REQUIRE_NOT_NULL(to);
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(from), "copied bytes"));
    CBAT_REQUIRE_OK(cbat_file_copy(cbat_string_cstr(from), cbat_string_cstr(to)));
    CBAT_REQUIRE_OK(cbat_file_read(cbat_string_cstr(to), &content));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(content), "copied bytes");
    cbat_string_free(content);
    cbat_string_free(from);
    cbat_string_free(to);
}

CBAT_TEST(test_out_of_memory) {
    CBatString *path = temp_path("oom.txt");
    size_t n;
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(path), "a\nb\nc\n"));
    for (n = 0; n < 8; n++) {
        CBatList *lines = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(n);
        err = cbat_file_read_lines(cbat_string_cstr(path), &lines);
        cbat_test_stop_failing_allocations();
        CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
        if (err == CBAT_OK) {
            CBAT_CHECK_EQ_SIZE(cbat_list_length(lines), 3);
            cbat_list_free(lines);
        }
    }
    cbat_string_free(path);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_write_and_read_round_trip),
    CBAT_TEST_ENTRY(test_read_large_file),
    CBAT_TEST_ENTRY(test_empty_file),
    CBAT_TEST_ENTRY(test_missing_file_errors),
    CBAT_TEST_ENTRY(test_append),
    CBAT_TEST_ENTRY(test_read_lines_strips_endings),
    CBAT_TEST_ENTRY(test_line_reader_numbers_and_end),
    CBAT_TEST_ENTRY(test_line_reader_from_open_file),
    CBAT_TEST_ENTRY(test_copy),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

/* A custom main: the shared sandbox is created and removed outside the
   runner, so it does not count as a per-test allocation. */
int main(int argc, char **argv) {
    int status;
    if (cbat_fs_make_temp_dir("cbat-file-", &sandbox) != CBAT_OK) {
        fprintf(stderr, "could not create a temp directory\n");
        return 1;
    }
    status = cbat_test_main("file", tests, CBAT_ARRAY_LEN(tests), argc, argv);
    cbat_fs_remove_tree(cbat_string_cstr(sandbox));
    cbat_string_free(sandbox);
    return status;
}
