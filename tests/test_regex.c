#include "cbat/regex.h"
#include "cbat/test.h"
#include "cbat/base.h"
#include <string.h>

CBAT_TEST(test_syntax_and_priority) {
    const char *patterns[] = {"a+", "a+?", "a|ab", "ab|a", "a{2,4}", "a{2,4}?", "(?:ab)*", "[a-z]+", "\\d+", "[^0-9]+"};
    const char *texts[] = {"aaa", "aaa", "ab", "ab", "aaaaa", "aaaaa", "ababx", "abc12", "123x", "abc123"};
    size_t ends[] = {3,1,1,2,4,2,4,3,3,3};
    size_t i;
    for (i = 0; i < sizeof(ends)/sizeof(ends[0]); i++) {
        CBatRegex *regex = NULL;
        CBatRegexMatch match;
        CBAT_REQUIRE_OK(cbat_regex_compile(patterns[i], 0, &regex, NULL));
        CBAT_REQUIRE_OK(cbat_regex_search(regex, texts[i], 0, &match));
        CBAT_CHECK_EQ_INT(match.groups[0].start, 0); CBAT_CHECK_EQ_INT(match.groups[0].end, ends[i]);
        cbat_regex_free(regex);
    }
}
CBAT_TEST(test_captures_and_assertions) {
    CBatRegex *regex = NULL;
    CBatRegexMatch m;
    CBAT_REQUIRE_OK(cbat_regex_compile("(a+)(b)?", 0, &regex, NULL));
    CBAT_CHECK_EQ_INT(cbat_regex_group_count(regex), 2);
    CBAT_REQUIRE_OK(cbat_regex_search(regex, "xxaaab", 0, &m));
    CBAT_CHECK_EQ_INT(m.groups[1].start, 2); CBAT_CHECK_EQ_INT(m.groups[1].end, 5);
    CBAT_CHECK_EQ_INT(m.groups[2].start, 5); CBAT_CHECK_EQ_INT(m.groups[2].end, 6);
    CBAT_REQUIRE_OK(cbat_regex_search(regex, "aaa", 0, &m)); CBAT_CHECK_EQ_UINT(m.groups[2].start, CBAT_NPOS);
    CBAT_CHECK(cbat_regex_is_match(regex, "aaab")); CBAT_CHECK(!cbat_regex_is_match(regex, "xaaab"));
    cbat_regex_free(regex);
    CBAT_REQUIRE_OK(cbat_regex_compile("^\\b[a-z]+\\b$", CBAT_REGEX_MULTILINE | CBAT_REGEX_IGNORE_CASE, &regex, NULL));
    CBAT_REQUIRE_OK(cbat_regex_search(regex, "123\nABC\n456", 0, &m));
    CBAT_CHECK_EQ_INT(m.groups[0].start, 4); CBAT_CHECK_EQ_INT(m.groups[0].end, 7);
    cbat_regex_free(regex);
    CBAT_REQUIRE_OK(cbat_regex_compile("a.b", CBAT_REGEX_DOT_ALL, &regex, NULL));
    CBAT_CHECK(cbat_regex_is_match(regex, "a\nb")); cbat_regex_free(regex);
}
CBAT_TEST(test_collections) {
    CBatRegex *regex = NULL;
    CBatList *matches = NULL, *pieces = NULL;
    CBatString *replaced = NULL;
    CBAT_REQUIRE_OK(cbat_regex_compile("(\\d+)", 0, &regex, NULL));
    CBAT_REQUIRE_OK(cbat_regex_find_all(regex, "a12b3", &matches)); CBAT_CHECK_EQ_INT(matches->length, 2);
    CBAT_REQUIRE_OK(cbat_regex_replace(regex, "a12b3", "[\\1]", &replaced)); CBAT_CHECK_EQ_STR(replaced->data, "a[12]b[3]");
    CBAT_REQUIRE_OK(cbat_regex_split(regex, "a12b3", &pieces)); CBAT_CHECK_EQ_INT(pieces->length, 3);
    CBAT_CHECK_EQ_STR((*(CBatString **)cbat_list_get(pieces, 1))->data, "b");
    cbat_list_free(matches); cbat_list_free(pieces); cbat_string_free(replaced); cbat_regex_free(regex);
    CBAT_REQUIRE_OK(cbat_regex_compile("", 0, &regex, NULL));
    CBAT_REQUIRE_OK(cbat_regex_find_all(regex, "ab", &matches)); CBAT_CHECK_EQ_INT(matches->length, 3);
    CBAT_REQUIRE_OK(cbat_regex_replace(regex, "ab", "-", &replaced)); CBAT_CHECK_EQ_STR(replaced->data, "-a-b-");
    cbat_string_free(replaced); cbat_list_free(matches); cbat_regex_free(regex);
}
CBAT_TEST(test_errors_and_oom) {
    const char *invalid[] = {"(", "[", "a{4,2}", "a**", "\\1", "(?=x)", "[z-a]", "a{1001}"};
    CBatRegex *regex = NULL;
    CBatParseInfo info;
    size_t i, n;
    bool compiled = false, searched = false, replaced = false, found_all = false, split = false;
    for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
        CBAT_CHECK(cbat_regex_compile(invalid[i], 0, &regex, &info) != CBAT_OK); CBAT_CHECK_NULL(regex);
    }
    CBAT_REQUIRE_OK(cbat_regex_compile("(a+|b){1,3}", 0, &regex, NULL));
    for (n = 0; n < 100; n++) {
        CBatRegex *created = NULL;
        CBatRegexMatch m;
        CBatString *text = NULL;
        CBatList *list = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(n); err = cbat_regex_compile("(a+|b){1,3}", 0, &created, NULL); cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) compiled = true; else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(created); }
        cbat_regex_free(created);
        cbat_test_fail_allocations_after(n); err = cbat_regex_search(regex, "aaab", 0, &m); cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) searched = true; else CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
        cbat_test_fail_allocations_after(n); err = cbat_regex_replace(regex, "aaab", "\\1", &text); cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) replaced = true; else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(text); }
        cbat_string_free(text);
        cbat_test_fail_allocations_after(n); err = cbat_regex_find_all(regex, "aaab", &list); cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) found_all = true; else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(list); }
        cbat_list_free(list); list = NULL;
        cbat_test_fail_allocations_after(n); err = cbat_regex_split(regex, "aaab", &list); cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) split = true; else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(list); }
        cbat_list_free(list);
        if (compiled && searched && replaced && found_all && split) break;
    }
    CBAT_CHECK(compiled && searched && replaced && found_all && split); cbat_regex_free(regex);
}
CBAT_TEST(test_nullable_cycles_limits_and_allocator) {
    CBatRegex *regex = NULL;
    CBatRegexMatch match, before;
    CBatArena *arena;
    char text[20001], nesting[600];
    size_t i;
    CBAT_REQUIRE_OK(cbat_regex_compile("(a*)*", 0, &regex, NULL));
    CBAT_REQUIRE_OK(cbat_regex_search(regex, "b", 0, &match));
    CBAT_CHECK_EQ_INT(match.groups[1].start, 0); CBAT_CHECK_EQ_INT(match.groups[1].end, 0);
    CBAT_REQUIRE_OK(cbat_regex_search(regex, "acc", 0, &match));
    CBAT_CHECK_EQ_INT(match.groups[1].start, 0); CBAT_CHECK_EQ_INT(match.groups[1].end, 1);
    cbat_regex_free(regex); regex = NULL;
    CBAT_REQUIRE_OK(cbat_regex_compile("(a+)+b", 0, &regex, NULL));
    memset(text, 'a', sizeof(text) - 1); text[sizeof(text) - 1] = 0;
    memset(&match, 0x55, sizeof(match)); before = match;
    CBAT_CHECK_ERR(cbat_regex_search(regex, text, 0, &match), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_EQ_MEM(&match, &before, sizeof(match));
    CBAT_CHECK_ERR(cbat_regex_search(regex, "a", 2, &match), CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_ERR(cbat_regex_search(NULL, "a", 0, &match), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_regex_search(regex, NULL, 0, &match), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_regex_search(regex, "a", 0, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_regex_find_all(regex, "a", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_regex_split(regex, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    { CBatString *out = NULL;
      CBAT_CHECK_ERR(cbat_regex_replace(regex, "a", "\\2", &out), CBAT_ERR_INVALID_ARGUMENT);
      CBAT_CHECK_ERR(cbat_regex_replace(regex, "a", "\\", &out), CBAT_ERR_INVALID_ARGUMENT);
      CBAT_CHECK_ERR(cbat_regex_replace(regex, "a", "\\q", &out), CBAT_ERR_INVALID_ARGUMENT); }
    cbat_regex_free(regex); regex = NULL;
    CBAT_CHECK_ERR(cbat_regex_compile(NULL, 0, &regex, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_regex_compile("a", 8, &regex, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_regex_compile("a", 0, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    for (i = 0; i < 129; i++) memcpy(nesting + i*3, "(?:", 3);
    for (i = 0; i < 129; i++) nesting[387+i] = ')'; nesting[516] = 0;
    CBAT_CHECK_ERR(cbat_regex_compile(nesting, 0, &regex, NULL), CBAT_ERR_OUT_OF_RANGE);
    arena = cbat_arena_new(4096); CBAT_REQUIRE_NOT_NULL(arena);
    CBAT_REQUIRE_OK(cbat_regex_compile_with("a{2,}", 0, cbat_arena_allocator(arena), &regex, NULL));
    CBAT_CHECK(cbat_regex_is_match(regex, "aaaa")); CBAT_CHECK(!cbat_regex_is_match(regex, "a"));
    cbat_regex_free(regex); cbat_arena_free(arena); cbat_regex_free(NULL);
    CBAT_CHECK_EQ_INT(cbat_regex_group_count(NULL), 0);
}
static const CBatTestCase tests[] = {CBAT_TEST_ENTRY(test_syntax_and_priority), CBAT_TEST_ENTRY(test_captures_and_assertions),
    CBAT_TEST_ENTRY(test_collections), CBAT_TEST_ENTRY(test_errors_and_oom),
    CBAT_TEST_ENTRY(test_nullable_cycles_limits_and_allocator)};
CBAT_TEST_MAIN("regex", tests)
