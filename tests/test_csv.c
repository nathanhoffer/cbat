#include <string.h>

#include "cbat/csv.h"
#include "cbat/file.h"
#include "cbat/fs.h"
#include "cbat/path.h"
#include "cbat/test.h"

static const char *field(const CBatList *rows, size_t row, size_t column) {
    const CBatList *fields = CBAT_LIST_GET(rows, CBatList *, row);
    return cbat_string_cstr(CBAT_LIST_GET(fields, CBatString *, column));
}

static size_t width(const CBatList *rows, size_t row) {
    return cbat_list_length(CBAT_LIST_GET(rows, CBatList *, row));
}

CBAT_TEST(test_simple_rows) {
    const char *text = "name,age\nada,36\ngrace,85\n";
    CBatList *rows = NULL;
    CBAT_REQUIRE_OK(cbat_csv_parse(text, strlen(text), NULL, &rows, NULL));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(rows), 3);
    CBAT_CHECK_EQ_STR(field(rows, 0, 0), "name");
    CBAT_CHECK_EQ_STR(field(rows, 2, 1), "85");
    cbat_list_free(rows);
}

CBAT_TEST(test_quotes_commas_newlines) {
    const char *text = "\"a,b\",\"say \"\"hi\"\"\",\"multi\nline\"\r\nx,,\"\"\r\n";
    CBatList *rows = NULL;
    CBAT_REQUIRE_OK(cbat_csv_parse(text, strlen(text), NULL, &rows, NULL));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(rows), 2);
    CBAT_CHECK_EQ_STR(field(rows, 0, 0), "a,b");
    CBAT_CHECK_EQ_STR(field(rows, 0, 1), "say \"hi\"");
    CBAT_CHECK_EQ_STR(field(rows, 0, 2), "multi\nline");
    CBAT_REQUIRE_EQ_SIZE(width(rows, 1), 3);
    CBAT_CHECK_EQ_STR(field(rows, 1, 1), "");
    CBAT_CHECK_EQ_STR(field(rows, 1, 2), "");
    cbat_list_free(rows);
}

CBAT_TEST(test_no_trailing_newline_cr_only_and_blank_lines) {
    const char *text = "a,b\r\rc,d\n\n\ne";
    CBatList *rows = NULL;
    CBAT_REQUIRE_OK(cbat_csv_parse(text, strlen(text), NULL, &rows, NULL));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(rows), 3);
    CBAT_CHECK_EQ_STR(field(rows, 1, 1), "d");
    CBAT_CHECK_EQ_STR(field(rows, 2, 0), "e");
    cbat_list_free(rows);
    CBAT_REQUIRE_OK(cbat_csv_parse("", 0, NULL, &rows, NULL));
    CBAT_CHECK_EQ_SIZE(cbat_list_length(rows), 0);
    cbat_list_free(rows);
    CBAT_REQUIRE_OK(cbat_csv_parse("a,", 2, NULL, &rows, NULL));
    CBAT_CHECK_EQ_SIZE(width(rows, 0), 2);
    cbat_list_free(rows);
}

CBAT_TEST(test_custom_delimiter) {
    CBatCsvOptions options = cbat_csv_default_options();
    CBatList *rows = NULL;
    options.delimiter = ';';
    CBAT_REQUIRE_OK(cbat_csv_parse("1;2,5;3", 7, &options, &rows, NULL));
    CBAT_REQUIRE_EQ_SIZE(width(rows, 0), 3);
    CBAT_CHECK_EQ_STR(field(rows, 0, 1), "2,5");
    cbat_list_free(rows);
    options.quote = ';';
    CBAT_CHECK_ERR(cbat_csv_parse("x", 1, &options, &rows, NULL), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_unterminated_quote_reports_position) {
    const char *text = "ok,row\nbad,\"never closed\n";
    CBatList *rows = NULL;
    CBatParseInfo info;
    memset(&info, 0, sizeof(info));
    CBAT_CHECK_ERR(cbat_csv_parse(text, strlen(text), NULL, &rows, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_SIZE(info.line, 2);
    CBAT_CHECK_EQ_SIZE(info.column, 5);
    CBAT_CHECK_EQ_SIZE(info.offset, 11);
    CBAT_CHECK_NOT_NULL(info.message);
}

CBAT_TEST(test_lenient_quotes) {
    CBatList *rows = NULL;
    CBAT_REQUIRE_OK(cbat_csv_parse("\"a\"b,c\"d", 8, NULL, &rows, NULL));
    CBAT_CHECK_EQ_STR(field(rows, 0, 0), "ab");
    CBAT_CHECK_EQ_STR(field(rows, 0, 1), "c\"d");
    cbat_list_free(rows);
}

CBAT_TEST(test_write_rows_and_round_trip) {
    const char *row1[] = {"plain", "has,comma", "has \"quote\"", " padded", ""};
    const char *row2[] = {"line\nbreak", NULL};
    CBatString *out = cbat_string_new();
    CBatList *rows = NULL;
    CBAT_REQUIRE_NOT_NULL(out);
    CBAT_REQUIRE_OK(cbat_csv_append_row(out, row1, 5, NULL));
    CBAT_REQUIRE_OK(cbat_csv_append_row(out, row2, 2, NULL));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(out),
                      "plain,\"has,comma\",\"has \"\"quote\"\"\",\" padded\",\n"
                      "\"line\nbreak\",\n");
    CBAT_REQUIRE_OK(cbat_csv_parse(out->data, out->length, NULL, &rows, NULL));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(rows), 2);
    CBAT_CHECK_EQ_STR(field(rows, 0, 2), "has \"quote\"");
    CBAT_CHECK_EQ_STR(field(rows, 0, 3), " padded");
    CBAT_CHECK_EQ_STR(field(rows, 1, 0), "line\nbreak");
    /* Re-emit the parsed row via the list variant. */
    cbat_string_clear(out);
    CBAT_REQUIRE_OK(cbat_csv_append_row_list(out, CBAT_LIST_GET(rows, CBatList *, 1), NULL));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(out), "\"line\nbreak\",\n");
    cbat_list_free(rows);
    cbat_string_free(out);
}

CBAT_TEST(test_single_empty_field_round_trip) {
    const char *empty[] = {""};
    const char *null_field[] = {NULL};
    CBatList *rows = NULL;
    CBatString *out = cbat_string_new();
    CBatList *fields = cbat_list_new(sizeof(CBatString *));
    CBatString *value = NULL;
    CBAT_REQUIRE_NOT_NULL(out);
    CBAT_REQUIRE_NOT_NULL(fields);
    CBAT_REQUIRE_OK(cbat_csv_append_row(out, empty, 1, NULL));
    CBAT_REQUIRE_OK(cbat_csv_append_row(out, null_field, 1, NULL));
    CBAT_REQUIRE_OK(cbat_list_push(fields, &value));
    CBAT_REQUIRE_OK(cbat_csv_append_row_list(out, fields, NULL));
    CBAT_CHECK_EQ_STR(out->data, "\"\"\n\"\"\n\"\"\n");
    CBAT_REQUIRE_OK(cbat_csv_parse(out->data, out->length, NULL, &rows, NULL));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(rows), 3);
    CBAT_CHECK_EQ_STR(field(rows, 0, 0), "");
    CBAT_CHECK_EQ_STR(field(rows, 1, 0), "");
    CBAT_CHECK_EQ_STR(field(rows, 2, 0), "");
    cbat_list_free(rows);
    cbat_list_free(fields);
    cbat_string_free(out);
}

CBAT_TEST(test_file_and_streaming_reader) {
    CBatString *dir = NULL;
    CBatString *path;
    CBatList *rows = NULL;
    CBatCsvReader *reader = NULL;
    const CBatList *fields;
    int count = 0;
    CBAT_REQUIRE_OK(cbat_fs_make_temp_dir("cbat-csv-", &dir));
    path = cbat_path_join(cbat_string_cstr(dir), "data.csv");
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write_text(cbat_string_cstr(path), "h1,h2\n1,\"x\ny\"\n2,z\n"));
    CBAT_REQUIRE_OK(cbat_csv_read_file(cbat_string_cstr(path), NULL, &rows, NULL));
    CBAT_CHECK_EQ_SIZE(cbat_list_length(rows), 3);
    cbat_list_free(rows);

    CBAT_REQUIRE_OK(cbat_csv_reader_open(cbat_string_cstr(path), NULL, &reader));
    while (cbat_csv_reader_next(reader, &fields) == CBAT_OK) {
        count++;
        if (count == 2) {
            CBAT_CHECK_EQ_STR(cbat_string_cstr(CBAT_LIST_GET(fields, CBatString *, 1)), "x\ny");
            CBAT_CHECK_EQ_SIZE(cbat_csv_reader_line_number(reader), 2);
        }
        if (count == 3) {
            CBAT_CHECK_EQ_SIZE(cbat_csv_reader_line_number(reader), 4);
        }
    }
    CBAT_CHECK_EQ_INT(count, 3);
    cbat_csv_reader_close(reader);
    CBAT_CHECK_ERR(cbat_csv_reader_open("no/such.csv", NULL, &reader), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_OK(cbat_fs_remove_tree(cbat_string_cstr(dir)));
    cbat_string_free(path);
    cbat_string_free(dir);
}

CBAT_TEST(test_out_of_memory) {
    const char *text = "a,b,c\n1,2,3\n";
    size_t n;
    for (n = 0; n < 20; n++) {
        CBatList *rows = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(n);
        err = cbat_csv_parse(text, strlen(text), NULL, &rows, NULL);
        cbat_test_stop_failing_allocations();
        CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
        if (err == CBAT_OK) {
            cbat_list_free(rows);
        }
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_simple_rows),
    CBAT_TEST_ENTRY(test_quotes_commas_newlines),
    CBAT_TEST_ENTRY(test_no_trailing_newline_cr_only_and_blank_lines),
    CBAT_TEST_ENTRY(test_custom_delimiter),
    CBAT_TEST_ENTRY(test_unterminated_quote_reports_position),
    CBAT_TEST_ENTRY(test_lenient_quotes),
    CBAT_TEST_ENTRY(test_write_rows_and_round_trip),
    CBAT_TEST_ENTRY(test_single_empty_field_round_trip),
    CBAT_TEST_ENTRY(test_file_and_streaming_reader),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("csv", tests)
