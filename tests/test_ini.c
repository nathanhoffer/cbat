#include <string.h>
#include "cbat/ini.h"
#include "cbat/file.h"
#include "cbat/fs.h"
#include "cbat/path.h"
#include "cbat/test.h"

CBAT_TEST(test_sections_quotes_comments_and_order) {
    const char *text = "\xef\xbb\xbf" "global = root\r\n; comment\r\n[ server ] # note\n"
        "port: 8080\nname = \"  Ada # literal  \";comment\npath = 'C:\\temp\\file'\n"
        "url=https://example.com/#fragment\nflag = yes # comment\n[server]\nport=9090\n[empty]\n";
    CBatIni *ini = NULL;
    size_t it = 0;
    const char *name, *key, *value;
    CBAT_REQUIRE_OK(cbat_ini_parse(text, strlen(text), &ini, NULL));
    CBAT_CHECK_EQ_STR(cbat_ini_get(ini, "", "global"), "root");
    CBAT_CHECK_EQ_STR(cbat_ini_get(ini, "server", "name"), "  Ada # literal  ");
    CBAT_CHECK_EQ_STR(cbat_ini_get(ini, "server", "path"), "C:\\temp\\file");
    CBAT_CHECK_EQ_STR(cbat_ini_get(ini, "server", "url"), "https://example.com/#fragment");
    CBAT_CHECK_EQ_STR(cbat_ini_get(ini, "server", "port"), "9090");
    CBAT_REQUIRE(cbat_ini_next_section(ini, &it, &name)); CBAT_CHECK_EQ_STR(name, "");
    CBAT_REQUIRE(cbat_ini_next_section(ini, &it, &name)); CBAT_CHECK_EQ_STR(name, "server");
    CBAT_REQUIRE(cbat_ini_next_section(ini, &it, &name)); CBAT_CHECK_EQ_STR(name, "empty");
    CBAT_CHECK(!cbat_ini_next_section(ini, &it, &name));
    it = 0;
    CBAT_REQUIRE(cbat_ini_next_key(ini, "server", &it, &key, &value));
    CBAT_CHECK_EQ_STR(key, "port"); CBAT_CHECK_EQ_STR(value, "9090");
    it = 0; CBAT_CHECK(!cbat_ini_next_key(ini, "empty", &it, NULL, NULL));
    CBAT_CHECK_NULL(cbat_ini_get(ini, "Server", "port"));
    cbat_ini_free(ini);
}

CBAT_TEST(test_typed_getters) {
    const char *truths[] = {"true", "YES", "On", "1", "  true\t"};
    const char *falses[] = {"false", "NO", "Off", "0", "\tfalse "};
    CBatIni *ini = cbat_ini_new();
    int64_t integer = 123;
    double number = 123;
    bool boolean = true;
    size_t i;
    CBAT_REQUIRE_NOT_NULL(ini);
    CBAT_REQUIRE_OK(cbat_ini_set(ini, "", "integer", "-9223372036854775808"));
    CBAT_REQUIRE_OK(cbat_ini_get_int(ini, "", "integer", &integer)); CBAT_CHECK_EQ_INT(integer, INT64_MIN);
    CBAT_REQUIRE_OK(cbat_ini_set(ini, "", "double", "1.25e2"));
    CBAT_REQUIRE_OK(cbat_ini_get_double(ini, "", "double", &number)); CBAT_CHECK(number == 125.0);
    for (i = 0; i < CBAT_ARRAY_LEN(truths); i++) {
        CBAT_REQUIRE_OK(cbat_ini_set(ini, "", "flag", truths[i]));
        CBAT_REQUIRE_OK(cbat_ini_get_bool(ini, "", "flag", &boolean)); CBAT_CHECK(boolean);
    }
    for (i = 0; i < CBAT_ARRAY_LEN(falses); i++) {
        CBAT_REQUIRE_OK(cbat_ini_set(ini, "", "flag", falses[i]));
        CBAT_REQUIRE_OK(cbat_ini_get_bool(ini, "", "flag", &boolean)); CBAT_CHECK(!boolean);
    }
    CBAT_REQUIRE_OK(cbat_ini_set(ini, "", "bad", "maybe"));
    CBAT_CHECK_ERR(cbat_ini_get_bool(ini, "", "bad", &boolean), CBAT_ERR_PARSE); CBAT_CHECK(!boolean);
    CBAT_CHECK_ERR(cbat_ini_get_int(ini, "", "bad", &integer), CBAT_ERR_PARSE); CBAT_CHECK_EQ_INT(integer, INT64_MIN);
    CBAT_CHECK_ERR(cbat_ini_get_double(ini, "", "bad", &number), CBAT_ERR_PARSE); CBAT_CHECK(number == 125.0);
    CBAT_CHECK_ERR(cbat_ini_get_int(ini, "", "missing", &integer), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_ini_get_double(ini, "", "missing", &number), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_ini_get_bool(ini, "", "missing", &boolean), CBAT_ERR_NOT_FOUND);
    cbat_ini_free(ini);
}

CBAT_TEST(test_round_trip_and_late_global_keys) {
    CBatIni *ini = cbat_ini_new(), *copy = NULL;
    CBatString *text = NULL, *again = NULL;
    CBAT_REQUIRE_NOT_NULL(ini);
    CBAT_REQUIRE_OK(cbat_ini_set(ini, "section", "name", "  quote \" and \\ and # ; \t  "));
    CBAT_REQUIRE_OK(cbat_ini_set(ini, "", "global", "value"));
    CBAT_REQUIRE_OK(cbat_ini_to_string(ini, &text));
    CBAT_CHECK(cbat_string_starts_with(text, "global = \"value\"\n"));
    CBAT_REQUIRE_OK(cbat_ini_parse(text->data, text->length, &copy, NULL));
    CBAT_CHECK_EQ_STR(cbat_ini_get(copy, "section", "name"), "  quote \" and \\ and # ; \t  ");
    CBAT_REQUIRE_OK(cbat_ini_to_string(copy, &again)); CBAT_CHECK_EQ_STR(again->data, text->data);
    cbat_string_free(again); cbat_string_free(text); cbat_ini_free(copy); cbat_ini_free(ini);
    CBAT_REQUIRE_OK(cbat_ini_parse(NULL, 0, &ini, NULL));
    CBAT_REQUIRE_OK(cbat_ini_to_string(ini, &text)); CBAT_CHECK_EQ_SIZE(text->length, 0);
    cbat_string_free(text); cbat_ini_free(ini);
}

CBAT_TEST(test_invalid_input_and_positions) {
    const char *bad[] = {"[missing", "[]", "[a] trailing", "key without delimiter", "=value",
        "a=\"never closed", "a='ok' trailing", "a=bad\x01", "[a[b]\nkey=value"};
    size_t i, it = 0;
    CBatIni *ini = NULL;
    CBatParseInfo info;
    int64_t integer; double number; bool boolean;
    for (i = 0; i < CBAT_ARRAY_LEN(bad); i++) {
        CBAT_CHECK_ERR(cbat_ini_parse(bad[i], strlen(bad[i]), &ini, &info), CBAT_ERR_PARSE);
        CBAT_CHECK_NULL(ini); CBAT_CHECK_NOT_NULL(info.message);
    }
    CBAT_CHECK_ERR(cbat_ini_parse("a=1\r\nbad", 8, &ini, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_SIZE(info.line, 2); CBAT_CHECK_EQ_SIZE(info.column, 1); CBAT_CHECK_EQ_SIZE(info.offset, 5);
    CBAT_CHECK_ERR(cbat_ini_parse("a=1\0b=2", 7, &ini, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_SIZE(info.offset, 3);
    CBAT_CHECK_ERR(cbat_ini_parse(NULL, 1, &ini, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_parse("", 0, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    ini = cbat_ini_new(); CBAT_REQUIRE_NOT_NULL(ini);
    CBAT_CHECK_ERR(cbat_ini_set(NULL, "", "key", "value"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, NULL, "key", "value"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, "", NULL, "value"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, "", "key", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, "", "", "value"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, "[bad]", "key", "value"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, "", "bad=key", "value"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, "", " key", "value"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, "", "#comment", "value"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_set(ini, "", "key", "multi\nline"), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_NULL(cbat_ini_get(NULL, "", "x")); CBAT_CHECK_NULL(cbat_ini_get(ini, NULL, "x"));
    CBAT_CHECK_NULL(cbat_ini_get(ini, "", NULL));
    CBAT_CHECK_ERR(cbat_ini_get_int(NULL, "", "x", &integer), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_int(ini, NULL, "x", &integer), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_int(ini, "", NULL, &integer), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_int(ini, "", "x", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_double(NULL, "", "x", &number), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_double(ini, NULL, "x", &number), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_double(ini, "", NULL, &number), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_double(ini, "", "x", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_bool(NULL, "", "x", &boolean), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_bool(ini, NULL, "x", &boolean), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_bool(ini, "", NULL, &boolean), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_get_bool(ini, "", "x", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK(!cbat_ini_next_section(NULL, &it, NULL)); CBAT_CHECK(!cbat_ini_next_section(ini, NULL, NULL));
    CBAT_CHECK(!cbat_ini_next_key(ini, "missing", &it, NULL, NULL));
    CBAT_CHECK(!cbat_ini_next_key(NULL, "", &it, NULL, NULL));
    CBAT_CHECK(!cbat_ini_next_key(ini, NULL, &it, NULL, NULL));
    CBAT_CHECK_ERR(cbat_ini_to_string(NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_to_string(ini, NULL), CBAT_ERR_INVALID_ARGUMENT);
    cbat_ini_free(ini); cbat_ini_free(NULL);
}

CBAT_TEST(test_files_and_custom_allocator) {
    CBatString *dir = NULL, *path;
    CBatIni *ini = NULL;
    CBatArena *arena;
    CBAT_REQUIRE_OK(cbat_fs_make_temp_dir("cbat-ini-", &dir));
    path = cbat_path_join(dir->data, "settings.ini"); CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write_text(path->data, "[app]\nname=cbat\n"));
    CBAT_REQUIRE_OK(cbat_ini_read_file(path->data, &ini, NULL));
    CBAT_CHECK_EQ_STR(cbat_ini_get(ini, "app", "name"), "cbat"); cbat_ini_free(ini); ini = NULL;
    CBAT_CHECK_ERR(cbat_ini_read_file("no/such/file.ini", &ini, NULL), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_ini_read_file(NULL, &ini, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_ini_read_file(path->data, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_REQUIRE_OK(cbat_fs_remove_tree(dir->data)); cbat_string_free(path); cbat_string_free(dir);
    arena = cbat_arena_new(4096); CBAT_REQUIRE_NOT_NULL(arena);
    ini = cbat_ini_new_with(cbat_arena_allocator(arena)); CBAT_REQUIRE_NOT_NULL(ini);
    CBAT_REQUIRE_OK(cbat_ini_set(ini, "app", "name", "arena"));
    CBAT_CHECK_EQ_STR(cbat_ini_get(ini, "app", "name"), "arena");
    cbat_ini_free(ini); cbat_arena_free(arena);
}

CBAT_TEST(test_oom_atomicity) {
    const char *source = "global=1\n[app]\nname=cbat\n[empty]\n";
    size_t allowed;
    bool parsed = false, serialized = false, inserted = false, replaced = false;
    CBatIni *base = NULL;
    CBAT_REQUIRE_OK(cbat_ini_parse(source, strlen(source), &base, NULL));
    for (allowed = 0; allowed < 100; allowed++) {
        CBatIni *ini = NULL;
        CBatString *text = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_ini_parse(source, strlen(source), &ini, NULL);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) parsed = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(ini); }
        cbat_ini_free(ini);
        cbat_test_fail_allocations_after(allowed);
        err = cbat_ini_to_string(base, &text);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) serialized = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(text); }
        cbat_string_free(text);
        ini = cbat_ini_new(); CBAT_REQUIRE_NOT_NULL(ini);
        CBAT_REQUIRE_OK(cbat_ini_set(ini, "existing", "key", "original"));
        cbat_test_fail_allocations_after(allowed);
        err = cbat_ini_set(ini, "new-section", "key", "value");
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) inserted = true;
        else {
            size_t it = 0; const char *name;
            CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
            CBAT_CHECK_NULL(cbat_ini_get(ini, "new-section", "key"));
            CBAT_REQUIRE(cbat_ini_next_section(ini, &it, &name));
            CBAT_CHECK(!cbat_ini_next_section(ini, &it, &name));
        }
        cbat_test_fail_allocations_after(allowed);
        err = cbat_ini_set(ini, "existing", "key", "replacement");
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) replaced = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_EQ_STR(cbat_ini_get(ini, "existing", "key"), "original"); }
        cbat_ini_free(ini);
        if (parsed && serialized && inserted && replaced) break;
    }
    CBAT_CHECK(parsed && serialized && inserted && replaced);
    cbat_ini_free(base);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_sections_quotes_comments_and_order),
    CBAT_TEST_ENTRY(test_typed_getters),
    CBAT_TEST_ENTRY(test_round_trip_and_late_global_keys),
    CBAT_TEST_ENTRY(test_invalid_input_and_positions),
    CBAT_TEST_ENTRY(test_files_and_custom_allocator),
    CBAT_TEST_ENTRY(test_oom_atomicity)
};
CBAT_TEST_MAIN("ini", tests)
