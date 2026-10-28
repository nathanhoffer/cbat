#include "cbat/cbat.h"
#include <string.h>

CBAT_TEST(test_json_extensions) {
    CBatJsonParseOptions options = {true, true, 2};
    CBatJson *value = NULL;
    CBatParseInfo info;
    const char *text = "/* hi */ {\"a\": [1, // x\n],} // end";
    CBAT_REQUIRE_OK(cbat_json_parse_with(text, strlen(text), &options, &value, &info));
    CBAT_CHECK_EQ_INT(cbat_json_pointer_int(value, "/a/0", -1), 1);
    CBAT_CHECK_EQ_INT(cbat_json_pointer_int(value, "/missing", -1), -1);
    CBAT_CHECK(cbat_json_pointer_number(value, "/a/0", -1) == 1);
    CBAT_CHECK_EQ_STR(cbat_json_pointer_string(value, "/a", "fallback"), "fallback");
    CBAT_CHECK(cbat_json_pointer_bool(value, "/a", true));
    cbat_json_free(value); value = NULL;
    CBAT_CHECK_ERR(cbat_json_parse(text, strlen(text), &value, NULL), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_json_parse_with("[[[]]]", 6, &options, &value, NULL), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_json_parse_with("1 /*", 4, &options, &value, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_INT(info.offset, 2);
    CBAT_CHECK_ERR(cbat_json_parse_with("[1,,]", 5, &options, &value, NULL), CBAT_ERR_PARSE);
    options.max_depth = CBAT_JSON_MAX_DEPTH + 1;
    CBAT_CHECK_ERR(cbat_json_parse_with("1", 1, &options, &value, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_NULL(value);
}

CBAT_TEST(test_sorted_and_merge_patch) {
    CBatJson *target = NULL, *patch = NULL, *expected = NULL;
    CBatString *sorted = NULL;
    const char *a = "{\"z\":1,\"a\":{\"z\":2,\"a\":3},\"items\":[1,2]}";
    const char *b = "{\"z\":null,\"a\":{\"z\":null,\"new\":4},\"items\":[9]}";
    const char *c = "{\"a\":{\"a\":3,\"new\":4},\"items\":[9]}";
    CBAT_REQUIRE_OK(cbat_json_parse(a, strlen(a), &target, NULL));
    CBAT_REQUIRE_OK(cbat_json_to_string_sorted(target, 0, &sorted));
    CBAT_CHECK_EQ_STR(sorted->data, "{\"a\":{\"a\":3,\"z\":2},\"items\":[1,2],\"z\":1}");
    cbat_string_free(sorted);
    CBAT_REQUIRE_OK(cbat_json_parse(b, strlen(b), &patch, NULL));
    CBAT_REQUIRE_OK(cbat_json_parse(c, strlen(c), &expected, NULL));
    CBAT_REQUIRE_OK(cbat_json_merge_patch(target, patch));
    CBAT_CHECK(cbat_json_equals(target, expected));
    CBAT_REQUIRE_OK(cbat_json_merge_patch(target, target));
    CBAT_CHECK(cbat_json_equals(target, expected));
    CBAT_REQUIRE_OK(cbat_json_merge_patch(target, cbat_json_object_get(target, "items")));
    CBAT_CHECK_EQ_INT(cbat_json_type(target), CBAT_JSON_ARRAY);
    CBAT_CHECK_EQ_INT(cbat_json_pointer_int(target, "/0", 0), 9);
    CBAT_CHECK_ERR(cbat_json_merge_patch(NULL, patch), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_json_merge_patch(target, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_json_to_string_sorted(NULL, 0, &sorted), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_json_to_string_sorted(target, -1, &sorted), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_json_to_string_sorted(target, 0, NULL), CBAT_ERR_INVALID_ARGUMENT);
    cbat_json_free(target); cbat_json_free(patch); cbat_json_free(expected);
}

CBAT_TEST(test_csv_records) {
    CBatCsvOptions options;
    CBatList *records = NULL;
    CBatDict *record;
    CBatString *row = cbat_string_new();
    const char *fields[] = {"a,b", "x"};
    const char *text = "name,age\nAda,36\nGrace,85\n";
    cbat_csv_options_init(&options); cbat_csv_options_init(NULL);
    CBAT_CHECK_EQ_INT(options.delimiter, ',');
    CBAT_REQUIRE_OK(cbat_csv_parse_records(text, strlen(text), &options, &records, NULL));
    CBAT_CHECK_EQ_INT(cbat_list_length(records), 2);
    record = *(CBatDict **)cbat_list_get(records, 0);
    CBAT_CHECK_EQ_STR((*(CBatString **)cbat_dict_get(record, "name"))->data, "Ada");
    cbat_list_free(records); records = NULL;
    CBAT_CHECK_ERR(cbat_csv_parse_records("a,a\nx,y", 7, NULL, &records, NULL), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_csv_parse_records("a,b\nx", 5, NULL, &records, NULL), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_csv_parse_records("a\0b\nx", 5, NULL, &records, NULL), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_csv_parse_records("a", 1, NULL, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_REQUIRE_OK(cbat_csv_parse_records("", 0, NULL, &records, NULL));
    CBAT_CHECK_EQ_INT(cbat_list_length(records), 0); cbat_list_free(records);
    CBAT_REQUIRE_NOT_NULL(row);
    CBAT_REQUIRE_OK(cbat_csv_format_row(row, fields, 2, NULL));
    CBAT_CHECK_EQ_STR(row->data, "\"a,b\",x\n"); cbat_string_free(row);
}

CBAT_TEST(test_extras_oom) {
    CBatJson *base = NULL, *patch = NULL;
    size_t n;
    bool merged = false, sorted = false, records_ok = false;
    CBAT_REQUIRE_OK(cbat_json_parse("{\"b\":1}", 7, &base, NULL));
    CBAT_REQUIRE_OK(cbat_json_parse("{\"a\":{\"x\":2}}", 13, &patch, NULL));
    for (n = 0; n < 200 && !(merged && sorted && records_ok); n++) {
        CBatJson *target = cbat_json_copy(base);
        CBatString *text = NULL;
        CBatList *records = NULL;
        CBatError err;
        CBAT_REQUIRE_NOT_NULL(target);
        cbat_test_fail_allocations_after(n);
        err = cbat_json_merge_patch(target, patch);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) merged = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK(cbat_json_equals(target, base)); }
        cbat_json_free(target);
        cbat_test_fail_allocations_after(n);
        err = cbat_json_to_string_sorted(patch, 2, &text);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) sorted = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(text); }
        cbat_string_free(text);
        cbat_test_fail_allocations_after(n);
        err = cbat_csv_parse_records("a,b\nx,y\n", 8, NULL, &records, NULL);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) records_ok = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(records); }
        cbat_list_free(records);
    }
    CBAT_CHECK(merged && sorted && records_ok);
    cbat_json_free(base); cbat_json_free(patch);
}

CBAT_TEST(test_merge_patch_rfc_examples) {
    const char *cases[][3] = {
        {"{\"a\":\"b\"}", "{\"a\":\"c\"}", "{\"a\":\"c\"}"},
        {"{\"a\":\"b\"}", "{\"a\":null}", "{}"},
        {"[\"a\",\"b\"]", "{\"a\":\"b\",\"c\":null}", "{\"a\":\"b\"}"},
        {"{\"a\":\"foo\"}", "null", "null"},
        {"{\"a\":\"foo\"}", "\"bar\"", "\"bar\""},
        {"{}", "{\"a\":{\"bb\":{\"ccc\":null}}}", "{\"a\":{\"bb\":{}}}"}
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        CBatJson *target = NULL, *patch = NULL, *expected = NULL;
        CBAT_REQUIRE_OK(cbat_json_parse(cases[i][0], strlen(cases[i][0]), &target, NULL));
        CBAT_REQUIRE_OK(cbat_json_parse(cases[i][1], strlen(cases[i][1]), &patch, NULL));
        CBAT_REQUIRE_OK(cbat_json_parse(cases[i][2], strlen(cases[i][2]), &expected, NULL));
        CBAT_REQUIRE_OK(cbat_json_merge_patch(target, patch));
        CBAT_CHECK(cbat_json_equals(target, expected));
        cbat_json_free(target); cbat_json_free(patch); cbat_json_free(expected);
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_json_extensions), CBAT_TEST_ENTRY(test_sorted_and_merge_patch),
    CBAT_TEST_ENTRY(test_csv_records), CBAT_TEST_ENTRY(test_extras_oom),
    CBAT_TEST_ENTRY(test_merge_patch_rfc_examples)
};
CBAT_TEST_MAIN("format_extras", tests)
