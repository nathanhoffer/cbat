#include <math.h>
#include <string.h>

#include "cbat/fs.h"
#include "cbat/json.h"
#include "cbat/path.h"
#include "cbat/test.h"

static CBatJson *parse(const char *text) {
    CBatJson *value = NULL;
    if (cbat_json_parse(text, strlen(text), &value, NULL) != CBAT_OK) {
        return NULL;
    }
    return value;
}

/* Parses `text`, serializes compactly and compares with `expected`. */
static void check_round_trip(const char *text, const char *expected) {
    CBatJson *value = parse(text);
    CBatString *out = NULL;
    if (!CBAT_CHECK_NOT_NULL(value)) {
        return;
    }
    if (CBAT_CHECK_OK(cbat_json_to_string(value, 0, &out))) {
        CBAT_CHECK_EQ_STR(cbat_string_cstr(out), expected);
        cbat_string_free(out);
    }
    cbat_json_free(value);
}

CBAT_TEST(test_parse_scalars) {
    check_round_trip("null", "null");
    check_round_trip(" true ", "true");
    check_round_trip("false", "false");
    check_round_trip("42", "42");
    check_round_trip("-0", "0");
    check_round_trip("3.5", "3.5");
    check_round_trip("0.1", "0.1");
    check_round_trip("1e3", "1000");
    check_round_trip("-2.5E-3", "-0.0025");
    check_round_trip("9223372036854775807", "9223372036854775807");
    check_round_trip("12345678901234567890", "1.2345678901234567e+19");
    check_round_trip("\"hi\"", "\"hi\"");
}

CBAT_TEST(test_parse_containers_keep_order) {
    check_round_trip("[1, [2, []], {}]", "[1,[2,[]],{}]");
    check_round_trip("{\"z\": 1, \"a\": {\"b\": [true, null]}}", "{\"z\":1,\"a\":{\"b\":[true,null]}}");
    check_round_trip("{\"k\": 1, \"k\": 2}", "{\"k\":2}");
}

CBAT_TEST(test_string_escapes_and_unicode) {
    CBatJson *value = parse("\"a\\\"b\\\\c\\/d\\b\\f\\n\\r\\t\\u00e9\\ud83d\\ude00\"");
    const char *text;
    size_t length;
    CBAT_REQUIRE_NOT_NULL(value);
    CBAT_REQUIRE_OK(cbat_json_get_string(value, &text, &length));
    CBAT_CHECK_EQ_STR(text, "a\"b\\c/d\b\f\n\r\t\xC3\xA9\xF0\x9F\x98\x80");
    cbat_json_free(value);
    check_round_trip("\"tab\\tquote\\\"ctl\\u0001\"", "\"tab\\tquote\\\"ctl\\u0001\"");
    check_round_trip("\"caf\xC3\xA9\"", "\"caf\xC3\xA9\"");
    value = parse("\"nul\\u0000inside\"");
    CBAT_REQUIRE_NOT_NULL(value);
    CBAT_REQUIRE_OK(cbat_json_get_string(value, &text, &length));
    CBAT_CHECK_EQ_SIZE(length, 10);
    cbat_json_free(value);
}

static void check_error(const char *text, size_t line, size_t column) {
    CBatJson *value = NULL;
    CBatParseInfo info;
    memset(&info, 0, sizeof(info));
    if (!CBAT_CHECK_ERR(cbat_json_parse(text, strlen(text), &value, &info), CBAT_ERR_PARSE)) {
        cbat_json_free(value);
        return;
    }
    CBAT_CHECK_EQ_SIZE(info.line, line);
    CBAT_CHECK_EQ_SIZE(info.column, column);
    CBAT_CHECK_NOT_NULL(info.message);
}

CBAT_TEST(test_parse_errors_with_positions) {
    check_error("", 1, 1);
    check_error("[1, 2", 1, 6);
    check_error("[1,]", 1, 4);
    check_error("{\"a\" 1}", 1, 6);
    check_error("{\n  \"a\": tru\n}", 2, 8);
    check_error("01", 1, 2);
    check_error("1.", 1, 3);
    check_error("-", 1, 2);
    check_error("\"unterminated", 1, 14);
    check_error("\"bad \\x escape\"", 1, 6);
    check_error("\"\\ud800\"", 1, 2);
    check_error("\"\\udc00\"", 1, 2);
    check_error("\"ctl\x01\"", 1, 5);
    check_error("\"\xFF\"", 1, 2);
    check_error("[1] [2]", 1, 5);
    check_error("{\"a\\u0000b\": 1}", 1, 2);
    check_error("1e999", 1, 1);
    check_error("NaN", 1, 1);
}

CBAT_TEST(test_depth_limit) {
    char deep[2 * (CBAT_JSON_MAX_DEPTH + 1) + 1];
    CBatJson *value = NULL;
    size_t i;
    for (i = 0; i <= CBAT_JSON_MAX_DEPTH; i++) {
        deep[i] = '[';
        deep[2 * (CBAT_JSON_MAX_DEPTH + 1) - 1 - i] = ']';
    }
    deep[2 * (CBAT_JSON_MAX_DEPTH + 1)] = '\0';
    CBAT_CHECK_ERR(cbat_json_parse(deep, strlen(deep), &value, NULL), CBAT_ERR_PARSE);
    /* One level less is fine. */
    CBAT_REQUIRE_OK(cbat_json_parse(deep + 1, strlen(deep) - 2, &value, NULL));
    cbat_json_free(value);
}

CBAT_TEST(test_build_and_pretty_print) {
    CBatJson *root = cbat_json_new_object();
    CBatJson *tags = cbat_json_new_array();
    CBatString *out = NULL;
    CBAT_REQUIRE_NOT_NULL(root);
    CBAT_REQUIRE_NOT_NULL(tags);
    CBAT_CHECK_OK(cbat_json_object_set_string(root, "name", "cbat"));
    CBAT_CHECK_OK(cbat_json_object_set_int(root, "version", 1));
    CBAT_CHECK_OK(cbat_json_object_set_bool(root, "free", true));
    CBAT_CHECK_OK(cbat_json_object_set_null(root, "nothing"));
    CBAT_CHECK_OK(cbat_json_array_append_string(tags, "c"));
    CBAT_CHECK_OK(cbat_json_array_append_number(tags, 0.5));
    CBAT_CHECK_OK(cbat_json_array_append_int(tags, -7));
    CBAT_CHECK_OK(cbat_json_array_append_bool(tags, false));
    CBAT_CHECK_OK(cbat_json_array_append_null(tags));
    CBAT_REQUIRE_OK(cbat_json_object_set(root, "tags", tags));
    CBAT_REQUIRE_OK(cbat_json_to_string(root, 2, &out));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(out),
                      "{\n"
                      "  \"name\": \"cbat\",\n"
                      "  \"version\": 1,\n"
                      "  \"free\": true,\n"
                      "  \"nothing\": null,\n"
                      "  \"tags\": [\n"
                      "    \"c\",\n"
                      "    0.5,\n"
                      "    -7,\n"
                      "    false,\n"
                      "    null\n"
                      "  ]\n"
                      "}");
    cbat_string_free(out);
    cbat_json_free(root);
}

CBAT_TEST(test_getters) {
    CBatJson *doc = parse("{\"i\": 5, \"f\": 2.5, \"big\": 1e30, \"s\": \"x\", \"b\": true}");
    int64_t i64 = 0;
    double d = 0;
    bool b = false;
    CBAT_REQUIRE_NOT_NULL(doc);
    CBAT_CHECK_OK(cbat_json_get_int64(cbat_json_object_get(doc, "i"), &i64));
    CBAT_CHECK_EQ_INT(i64, 5);
    CBAT_CHECK_ERR(cbat_json_get_int64(cbat_json_object_get(doc, "f"), &i64), CBAT_ERR_TYPE);
    CBAT_CHECK_ERR(cbat_json_get_int64(cbat_json_object_get(doc, "big"), &i64), CBAT_ERR_OVERFLOW);
    CBAT_CHECK_OK(cbat_json_get_number(cbat_json_object_get(doc, "f"), &d));
    CBAT_CHECK_NEAR(d, 2.5, 0.0);
    CBAT_CHECK_OK(cbat_json_get_bool(cbat_json_object_get(doc, "b"), &b));
    CBAT_CHECK(b);
    CBAT_CHECK_ERR(cbat_json_get_bool(cbat_json_object_get(doc, "s"), &b), CBAT_ERR_TYPE);
    CBAT_CHECK_EQ_STR(cbat_json_as_string(cbat_json_object_get(doc, "s"), "?"), "x");
    CBAT_CHECK_EQ_STR(cbat_json_as_string(cbat_json_object_get(doc, "missing"), "?"), "?");
    CBAT_CHECK_EQ_INT(cbat_json_as_int64(cbat_json_object_get(doc, "f"), -1), -1);
    CBAT_CHECK_NEAR(cbat_json_as_number(cbat_json_object_get(doc, "i"), 0), 5.0, 0.0);
    CBAT_CHECK(cbat_json_as_bool(NULL, true));
    CBAT_CHECK_EQ_INT(cbat_json_type(cbat_json_object_get(doc, "s")), CBAT_JSON_STRING);
    CBAT_CHECK_EQ_INT(cbat_json_type(NULL), CBAT_JSON_NULL);
    CBAT_CHECK_EQ_STR(cbat_json_type_name(CBAT_JSON_OBJECT), "object");
    CBAT_CHECK_EQ_SIZE(cbat_json_length(doc), 5);
    cbat_json_free(doc);
}

CBAT_TEST(test_pointer) {
    CBatJson *doc = parse("{\"users\": [{\"name\": \"ada\"}, {\"name\": \"grace\"}],"
                          " \"a/b\": 1, \"m~n\": 2, \"\": 3}");
    CBAT_REQUIRE_NOT_NULL(doc);
    CBAT_CHECK(cbat_json_pointer(doc, "") == doc);
    CBAT_CHECK_EQ_STR(cbat_json_as_string(cbat_json_pointer(doc, "/users/1/name"), NULL), "grace");
    CBAT_CHECK_EQ_INT(cbat_json_as_int64(cbat_json_pointer(doc, "/a~1b"), 0), 1);
    CBAT_CHECK_EQ_INT(cbat_json_as_int64(cbat_json_pointer(doc, "/m~0n"), 0), 2);
    CBAT_CHECK_EQ_INT(cbat_json_as_int64(cbat_json_pointer(doc, "/"), 0), 3);
    CBAT_CHECK_NULL(cbat_json_pointer(doc, "/users/2"));
    CBAT_CHECK_NULL(cbat_json_pointer(doc, "/users/01"));
    CBAT_CHECK_NULL(cbat_json_pointer(doc, "/users/-"));
    CBAT_CHECK_NULL(cbat_json_pointer(doc, "/users/0/name/x"));
    CBAT_CHECK_NULL(cbat_json_pointer(doc, "users"));
    CBAT_CHECK_NULL(cbat_json_pointer(doc, "/bad~2escape"));
    cbat_json_free(doc);
}

CBAT_TEST(test_modify_remove_copy_equals) {
    CBatJson *doc = parse("{\"a\": [1, 2, 3], \"b\": {\"c\": \"d\"}}");
    CBatJson *copy;
    CBatJson *other;
    CBAT_REQUIRE_NOT_NULL(doc);
    copy = cbat_json_copy(doc);
    CBAT_REQUIRE_NOT_NULL(copy);
    CBAT_CHECK(cbat_json_equals(doc, copy));
    CBAT_CHECK_OK(cbat_json_array_remove(cbat_json_object_get(doc, "a"), 0));
    CBAT_CHECK(!cbat_json_equals(doc, copy));
    CBAT_CHECK_ERR(cbat_json_array_remove(cbat_json_object_get(doc, "a"), 9), CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_OK(cbat_json_object_remove(doc, "b"));
    CBAT_CHECK_ERR(cbat_json_object_remove(doc, "b"), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_json_object_set_int(cbat_json_object_get(doc, "a"), "k", 1), CBAT_ERR_TYPE);
    CBAT_CHECK_ERR(cbat_json_array_append_int(doc, 1), CBAT_ERR_TYPE);
    other = parse("{\"b\": {\"c\": \"d\"}, \"a\": [1, 2, 3]}"); /* order differs */
    CBAT_CHECK(cbat_json_equals(copy, other));
    CBAT_CHECK(!cbat_json_equals(copy, NULL));
    CBAT_CHECK_ERR(cbat_json_object_set(doc, "self", doc), CBAT_ERR_INVALID_ARGUMENT);
    cbat_json_free(other);
    cbat_json_free(copy);
    cbat_json_free(doc);
}

CBAT_TEST(test_number_formatting) {
    CBatJson *n = cbat_json_new_number(1.0 / 3.0);
    CBatString *out = NULL;
    double back = 0;
    CBAT_REQUIRE_NOT_NULL(n);
    CBAT_REQUIRE_OK(cbat_json_to_string(n, 0, &out));
    CBAT_CHECK_OK(cbat_string_to_double(cbat_string_cstr(out), &back));
    CBAT_CHECK(back == 1.0 / 3.0);
    cbat_string_free(out);
    cbat_json_free(n);
    n = cbat_json_new_number(NAN);
    CBAT_REQUIRE_NOT_NULL(n);
    CBAT_CHECK_ERR(cbat_json_to_string(n, 0, &out), CBAT_ERR_OUT_OF_RANGE);
    cbat_json_free(n);
}

CBAT_TEST(test_file_round_trip) {
    CBatString *dir = NULL;
    CBatString *path;
    CBatJson *doc = parse("{\"saved\": [1, 2]}");
    CBatJson *loaded = NULL;
    CBAT_REQUIRE_NOT_NULL(doc);
    CBAT_REQUIRE_OK(cbat_fs_make_temp_dir("cbat-json-", &dir));
    path = cbat_path_join(cbat_string_cstr(dir), "doc.json");
    CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_json_write_file(cbat_string_cstr(path), doc, 2));
    CBAT_REQUIRE_OK(cbat_json_parse_file(cbat_string_cstr(path), &loaded, NULL));
    CBAT_CHECK(cbat_json_equals(doc, loaded));
    CBAT_CHECK_ERR(cbat_json_parse_file("no/such.json", &loaded, NULL), CBAT_ERR_NOT_FOUND);
    cbat_json_free(loaded);
    cbat_json_free(doc);
    CBAT_CHECK_OK(cbat_fs_remove_tree(cbat_string_cstr(dir)));
    cbat_string_free(path);
    cbat_string_free(dir);
}

CBAT_TEST(test_bom_is_skipped) {
    CBatJson *value = NULL;
    CBAT_REQUIRE_OK(cbat_json_parse("\xEF\xBB\xBF[1]", 6, &value, NULL));
    CBAT_CHECK_EQ_SIZE(cbat_json_length(value), 1);
    cbat_json_free(value);
}

CBAT_TEST(test_out_of_memory) {
    const char *text = "{\"a\": [1, 2.5, \"three\", {\"b\": null}], \"c\": true}";
    size_t n;
    for (n = 0; n < 40; n++) {
        CBatJson *value = NULL;
        CBatString *out = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(n);
        err = cbat_json_parse(text, strlen(text), &value, NULL);
        CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
        if (err == CBAT_OK) {
            err = cbat_json_to_string(value, 2, &out);
            CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
            cbat_string_free(out);
            cbat_json_free(value);
        }
        cbat_test_stop_failing_allocations();
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_parse_scalars),
    CBAT_TEST_ENTRY(test_parse_containers_keep_order),
    CBAT_TEST_ENTRY(test_string_escapes_and_unicode),
    CBAT_TEST_ENTRY(test_parse_errors_with_positions),
    CBAT_TEST_ENTRY(test_depth_limit),
    CBAT_TEST_ENTRY(test_build_and_pretty_print),
    CBAT_TEST_ENTRY(test_getters),
    CBAT_TEST_ENTRY(test_pointer),
    CBAT_TEST_ENTRY(test_modify_remove_copy_equals),
    CBAT_TEST_ENTRY(test_number_formatting),
    CBAT_TEST_ENTRY(test_file_round_trip),
    CBAT_TEST_ENTRY(test_bom_is_skipped),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("json", tests)
