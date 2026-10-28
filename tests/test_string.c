#include <math.h>
#include <float.h>
#include <locale.h>
#include <string.h>

#include "cbat/string.h"
#include "cbat/test.h"
#include "cbat/random.h"

CBAT_TEST(test_new_from_and_free) {
    CBatString *a = cbat_string_new();
    CBatString *b = cbat_string_from("hello");
    CBatString *c = cbat_string_from(NULL);
    CBatString *d = cbat_string_from_length("ab\0cd", 5);
    CBAT_REQUIRE_NOT_NULL(a);
    CBAT_REQUIRE_NOT_NULL(b);
    CBAT_REQUIRE_NOT_NULL(c);
    CBAT_REQUIRE_NOT_NULL(d);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(a), "");
    CBAT_CHECK(cbat_string_is_empty(a));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(b), "hello");
    CBAT_CHECK_EQ_SIZE(cbat_string_length(b), 5);
    CBAT_CHECK_EQ_SIZE(cbat_string_length(c), 0);
    CBAT_CHECK_EQ_SIZE(cbat_string_length(d), 5);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(NULL), "");
    cbat_string_free(a);
    cbat_string_free(b);
    cbat_string_free(c);
    cbat_string_free(d);
    cbat_string_free(NULL);
}

CBAT_TEST(test_from_format_and_append_format) {
    CBatString *s = cbat_string_from_format("%s=%d", "x", 42);
    CBAT_REQUIRE_NOT_NULL(s);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "x=42");
    CBAT_CHECK_OK(cbat_string_append_format(s, ", %.2f", 3.14159));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "x=42, 3.14");
    CBAT_CHECK_OK(cbat_string_append_format(s, "%s", ""));
    CBAT_CHECK_EQ_SIZE(cbat_string_length(s), 10);
    cbat_string_free(s);
}

CBAT_TEST(test_append_grows) {
    CBatString *s = cbat_string_new();
    int i;
    CBAT_REQUIRE_NOT_NULL(s);
    for (i = 0; i < 1000; i++) {
        CBAT_REQUIRE_OK(cbat_string_append_char(s, (char)('a' + i % 26)));
    }
    CBAT_CHECK_EQ_SIZE(cbat_string_length(s), 1000);
    CBAT_CHECK_EQ_INT(s->data[1000], '\0');
    CBAT_CHECK_EQ_INT(s->data[26], 'a');
    CBAT_CHECK_ERR(cbat_string_append(s, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_string_append(NULL, "x"), CBAT_ERR_INVALID_ARGUMENT);
    cbat_string_free(s);
}

CBAT_TEST(test_append_self) {
    CBatString *s = cbat_string_from("abc");
    int i;
    CBAT_REQUIRE_NOT_NULL(s);
    for (i = 0; i < 6; i++) {
        CBAT_REQUIRE_OK(cbat_string_append_string(s, s));
    }
    CBAT_CHECK_EQ_SIZE(cbat_string_length(s), 3u * 64u);
    CBAT_CHECK(strncmp(s->data + 189, "abc", 3) == 0);
    CBAT_REQUIRE_OK(cbat_string_append(s, s->data + 1)); /* tail of itself */
    CBAT_CHECK_EQ_SIZE(cbat_string_length(s), 3u * 64u * 2u - 1u);
    cbat_string_free(s);
}

CBAT_TEST(test_copy_set_clear) {
    CBatString *s = cbat_string_from("one");
    CBatString *copy;
    CBAT_REQUIRE_NOT_NULL(s);
    copy = cbat_string_copy(s);
    CBAT_REQUIRE_NOT_NULL(copy);
    CBAT_CHECK_OK(cbat_string_set(s, "two"));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "two");
    CBAT_CHECK_EQ_STR(cbat_string_cstr(copy), "one");
    cbat_string_clear(s);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "");
    CBAT_CHECK_NULL(cbat_string_copy(NULL));
    cbat_string_free(copy);
    cbat_string_free(s);
}

CBAT_TEST(test_steal) {
    CBatString *s = cbat_string_from("mine now");
    char *raw;
    CBAT_REQUIRE_NOT_NULL(s);
    raw = cbat_string_steal(s);
    CBAT_CHECK_EQ_STR(raw, "mine now");
    cbat_free(raw);
    CBAT_CHECK_NULL(cbat_string_steal(NULL));
}

CBAT_TEST(test_insert_erase_truncate) {
    CBatString *s = cbat_string_from("held");
    CBAT_REQUIRE_NOT_NULL(s);
    CBAT_CHECK_OK(cbat_string_insert(s, 2, "llo wor"));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "hello world");
    CBAT_CHECK_OK(cbat_string_insert(s, 0, ">> "));
    CBAT_CHECK_OK(cbat_string_insert(s, s->length, "!"));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), ">> hello world!");
    CBAT_CHECK_ERR(cbat_string_insert(s, 100, "x"), CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_OK(cbat_string_insert(s, 3, s->data + 3)); /* insert own tail */
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), ">> hello world!hello world!");
    CBAT_CHECK_OK(cbat_string_erase(s, 0, 3));
    CBAT_CHECK_OK(cbat_string_erase(s, 12, 1000));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "hello world!");
    CBAT_CHECK_ERR(cbat_string_erase(s, 100, 1), CBAT_ERR_OUT_OF_RANGE);
    cbat_string_truncate(s, 5);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "hello");
    cbat_string_truncate(s, 50);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "hello");
    cbat_string_free(s);
}

CBAT_TEST(test_replace) {
    CBatString *s = cbat_string_from("a-b-c-d");
    size_t count = 0;
    CBAT_REQUIRE_NOT_NULL(s);
    CBAT_CHECK_OK(cbat_string_replace(s, "-", " + ", &count));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "a + b + c + d");
    CBAT_CHECK_EQ_SIZE(count, 3);
    CBAT_CHECK_OK(cbat_string_replace(s, " + ", "", NULL));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "abcd");
    CBAT_CHECK_OK(cbat_string_replace(s, "zz", "y", &count));
    CBAT_CHECK_EQ_SIZE(count, 0);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "abcd");
    CBAT_CHECK_ERR(cbat_string_replace(s, "", "y", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_OK(cbat_string_set(s, "aaa"));
    CBAT_CHECK_OK(cbat_string_replace(s, "aa", "b", &count));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "ba");
    cbat_string_free(s);
}

CBAT_TEST(test_case_and_trim) {
    CBatString *s = cbat_string_from(" \t Hello, World 9 \r\n");
    CBAT_REQUIRE_NOT_NULL(s);
    cbat_string_trim(s);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "Hello, World 9");
    cbat_string_to_upper(s);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "HELLO, WORLD 9");
    cbat_string_to_lower(s);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "hello, world 9");
    CBAT_CHECK_OK(cbat_string_set(s, "   "));
    cbat_string_trim(s);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "");
    CBAT_CHECK_OK(cbat_string_set(s, "  x  "));
    cbat_string_trim_left(s);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "x  ");
    cbat_string_trim_right(s);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "x");
    cbat_string_free(s);
}

CBAT_TEST(test_find_rfind_count_contains) {
    CBatString *s = cbat_string_from("one two one two one");
    CBAT_REQUIRE_NOT_NULL(s);
    CBAT_CHECK_EQ_SIZE(cbat_string_find(s, "two", 0), 4);
    CBAT_CHECK_EQ_SIZE(cbat_string_find(s, "two", 5), 12);
    CBAT_CHECK_EQ_SIZE(cbat_string_find(s, "three", 0), CBAT_NPOS);
    CBAT_CHECK_EQ_SIZE(cbat_string_find(s, "", 3), 3);
    CBAT_CHECK_EQ_SIZE(cbat_string_find(s, "one", 100), CBAT_NPOS);
    CBAT_CHECK_EQ_SIZE(cbat_string_rfind(s, "one"), 16);
    CBAT_CHECK_EQ_SIZE(cbat_string_rfind(s, "zzz"), CBAT_NPOS);
    CBAT_CHECK_EQ_SIZE(cbat_string_rfind(s, "a very long needle that does not fit"), CBAT_NPOS);
    CBAT_CHECK_EQ_SIZE(cbat_string_count(s, "one"), 3);
    CBAT_CHECK_EQ_SIZE(cbat_string_count(s, ""), 0);
    CBAT_CHECK(cbat_string_contains(s, "two one"));
    CBAT_CHECK(!cbat_string_contains(s, "twoone"));
    CBAT_CHECK(cbat_string_starts_with(s, "one "));
    CBAT_CHECK(!cbat_string_starts_with(s, "two"));
    CBAT_CHECK(cbat_string_ends_with(s, " one"));
    CBAT_CHECK(!cbat_string_ends_with(s, "one two one two one!"));
    CBAT_CHECK(cbat_string_equals(s, "one two one two one"));
    CBAT_CHECK(!cbat_string_equals(s, "one"));
    CBAT_CHECK_EQ_SIZE(cbat_string_find(NULL, "x", 0), CBAT_NPOS);
    cbat_string_free(s);
}

CBAT_TEST(test_compare) {
    CBatString *a = cbat_string_from("apple");
    CBatString *b = cbat_string_from("apples");
    CBatString *c = cbat_string_from("banana");
    CBAT_REQUIRE_NOT_NULL(a);
    CBAT_REQUIRE_NOT_NULL(b);
    CBAT_REQUIRE_NOT_NULL(c);
    CBAT_CHECK(cbat_string_compare(a, b) < 0);
    CBAT_CHECK(cbat_string_compare(b, a) > 0);
    CBAT_CHECK(cbat_string_compare(c, a) > 0);
    CBAT_CHECK(cbat_string_compare(a, a) == 0);
    CBAT_CHECK(cbat_string_compare(NULL, a) < 0);
    CBAT_CHECK(cbat_string_compare_elements(&a, &c) < 0);
    cbat_string_free(a);
    cbat_string_free(b);
    cbat_string_free(c);
}

CBAT_TEST(test_substring_and_repeat) {
    CBatString *s = cbat_string_from("hello world");
    CBatString *sub;
    CBatString *rep;
    CBAT_REQUIRE_NOT_NULL(s);
    sub = cbat_string_substring(s, 6, 5);
    CBAT_REQUIRE_NOT_NULL(sub);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(sub), "world");
    cbat_string_free(sub);
    sub = cbat_string_substring(s, 6, 1000);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(sub), "world");
    cbat_string_free(sub);
    sub = cbat_string_substring(s, 1000, 3);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(sub), "");
    cbat_string_free(sub);
    rep = cbat_string_repeat("ab", 3);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(rep), "ababab");
    cbat_string_free(rep);
    rep = cbat_string_repeat("ab", 0);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(rep), "");
    cbat_string_free(rep);
    CBAT_CHECK_NULL(cbat_string_repeat(NULL, 3));
    cbat_string_free(s);
}

static const char *piece(const CBatList *list, size_t i) {
    return cbat_string_cstr(CBAT_LIST_GET(list, CBatString *, i));
}

CBAT_TEST(test_split) {
    CBatList *parts = NULL;
    CBAT_REQUIRE_OK(cbat_string_split("a,b,,c", ",", &parts));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(parts), 4);
    CBAT_CHECK_EQ_STR(piece(parts, 0), "a");
    CBAT_CHECK_EQ_STR(piece(parts, 2), "");
    CBAT_CHECK_EQ_STR(piece(parts, 3), "c");
    cbat_list_free(parts);

    CBAT_REQUIRE_OK(cbat_string_split("", ",", &parts));
    CBAT_CHECK_EQ_SIZE(cbat_list_length(parts), 1);
    cbat_list_free(parts);

    CBAT_REQUIRE_OK(cbat_string_split("x::y::", "::", &parts));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(parts), 3);
    CBAT_CHECK_EQ_STR(piece(parts, 1), "y");
    CBAT_CHECK_EQ_STR(piece(parts, 2), "");
    cbat_list_free(parts);

    CBAT_CHECK_ERR(cbat_string_split("abc", "", &parts), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_split_whitespace) {
    CBatList *parts = NULL;
    CBAT_REQUIRE_OK(cbat_string_split_whitespace("  the quick\t\nbrown   fox ", &parts));
    CBAT_REQUIRE_EQ_SIZE(cbat_list_length(parts), 4);
    CBAT_CHECK_EQ_STR(piece(parts, 0), "the");
    CBAT_CHECK_EQ_STR(piece(parts, 3), "fox");
    cbat_list_free(parts);
    CBAT_REQUIRE_OK(cbat_string_split_whitespace("   ", &parts));
    CBAT_CHECK_EQ_SIZE(cbat_list_length(parts), 0);
    cbat_list_free(parts);
}

CBAT_TEST(test_join) {
    CBatList *parts = NULL;
    CBatString *joined = NULL;
    CBatList *wrong = cbat_list_new(sizeof(int));
    CBAT_REQUIRE_OK(cbat_string_split("a b c", " ", &parts));
    CBAT_REQUIRE_OK(cbat_string_join(parts, ", ", &joined));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(joined), "a, b, c");
    cbat_string_free(joined);
    CBAT_CHECK_ERR(cbat_string_join(wrong, ",", &joined), CBAT_ERR_INVALID_ARGUMENT);
    cbat_list_clear(parts);
    CBAT_REQUIRE_OK(cbat_string_join(parts, ", ", &joined));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(joined), "");
    cbat_string_free(joined);
    cbat_list_free(parts);
    cbat_list_free(wrong);
}

CBAT_TEST(test_to_int64) {
    int64_t v = 0;
    CBAT_CHECK_OK(cbat_string_to_int64("42", &v));
    CBAT_CHECK_EQ_INT(v, 42);
    CBAT_CHECK_OK(cbat_string_to_int64("  -17  ", &v));
    CBAT_CHECK_EQ_INT(v, -17);
    CBAT_CHECK_OK(cbat_string_to_int64("+5", &v));
    CBAT_CHECK_EQ_INT(v, 5);
    CBAT_CHECK_OK(cbat_string_to_int64("9223372036854775807", &v));
    CBAT_CHECK(v == INT64_MAX);
    CBAT_CHECK_OK(cbat_string_to_int64("-9223372036854775808", &v));
    CBAT_CHECK(v == INT64_MIN);
    CBAT_CHECK_ERR(cbat_string_to_int64("9223372036854775808", &v), CBAT_ERR_OVERFLOW);
    CBAT_CHECK_ERR(cbat_string_to_int64("99999999999999999999x", &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_int64("", &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_int64("-", &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_int64("12abc", &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_int64("1 2", &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_int64(NULL, &v), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_OK(cbat_string_to_int64_base("ff", 16, &v));
    CBAT_CHECK_EQ_INT(v, 255);
    CBAT_CHECK_OK(cbat_string_to_int64_base("-101", 2, &v));
    CBAT_CHECK_EQ_INT(v, -5);
    CBAT_CHECK_ERR(cbat_string_to_int64_base("12", 2, &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_int64_base("1", 37, &v), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_to_double) {
    double v = 0;
    CBAT_CHECK_OK(cbat_string_to_double("3.25", &v));
    CBAT_CHECK_NEAR(v, 3.25, 0.0);
    CBAT_CHECK_OK(cbat_string_to_double(" -1e3 ", &v));
    CBAT_CHECK_NEAR(v, -1000.0, 0.0);
    CBAT_CHECK_OK(cbat_string_to_double("inf", &v));
    CBAT_CHECK(isinf(v));
    CBAT_CHECK_OK(cbat_string_to_double("nan", &v));
    CBAT_CHECK(v != v);
    CBAT_CHECK_ERR(cbat_string_to_double("1e999", &v), CBAT_ERR_OVERFLOW);
    CBAT_CHECK_ERR(cbat_string_to_double("0x10", &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_double("1.5.2", &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_double("  ", &v), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_double("abc", &v), CBAT_ERR_PARSE);
}

CBAT_TEST(test_utf8_decode_encode) {
    uint32_t cp = 0;
    char buffer[4];
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("A", 1, &cp), 1);
    CBAT_CHECK_EQ_UINT(cp, 0x41);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\xC3\xA9", 2, &cp), 2);
    CBAT_CHECK_EQ_UINT(cp, 0xE9);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\xE2\x82\xAC", 3, &cp), 3);
    CBAT_CHECK_EQ_UINT(cp, 0x20AC);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\xF0\x9F\x98\x80", 4, &cp), 4);
    CBAT_CHECK_EQ_UINT(cp, 0x1F600);
    /* Invalid: overlong, surrogate, truncated, stray continuation, > U+10FFFF. */
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\xC0\xAF", 2, &cp), 0);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\xE0\x80\xAF", 3, &cp), 0);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\xED\xA0\x80", 3, &cp), 0);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\xE2\x82", 2, &cp), 0);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\x80", 1, &cp), 0);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("\xF4\x90\x80\x80", 4, &cp), 0);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_decode("", 0, &cp), 0);

    CBAT_CHECK_EQ_SIZE(cbat_utf8_encode(0x41, buffer), 1);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_encode(0xE9, buffer), 2);
    CBAT_CHECK_EQ_MEM(buffer, "\xC3\xA9", 2);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_encode(0x20AC, buffer), 3);
    CBAT_CHECK_EQ_MEM(buffer, "\xE2\x82\xAC", 3);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_encode(0x1F600, buffer), 4);
    CBAT_CHECK_EQ_MEM(buffer, "\xF0\x9F\x98\x80", 4);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_encode(0xD800, buffer), 0);
    CBAT_CHECK_EQ_SIZE(cbat_utf8_encode(0x110000, buffer), 0);
}

CBAT_TEST(test_utf8_count_and_validate) {
    size_t count = 0;
    const char *text = "h\xC3\xA9llo \xF0\x9F\x98\x80";
    CBAT_CHECK_OK(cbat_utf8_count(text, strlen(text), &count));
    CBAT_CHECK_EQ_SIZE(count, 7);
    CBAT_CHECK(cbat_utf8_is_valid(text, strlen(text)));
    CBAT_CHECK(!cbat_utf8_is_valid("\xFF", 1));
    CBAT_CHECK_ERR(cbat_utf8_count("ab\xC3", 3, &count), CBAT_ERR_INVALID_UTF8);
    CBAT_CHECK(cbat_utf8_is_valid("", 0));
}

CBAT_TEST(test_append_codepoint) {
    CBatString *s = cbat_string_new();
    CBAT_REQUIRE_NOT_NULL(s);
    CBAT_CHECK_OK(cbat_string_append_codepoint(s, 0x48));
    CBAT_CHECK_OK(cbat_string_append_codepoint(s, 0x20AC));
    CBAT_CHECK_ERR(cbat_string_append_codepoint(s, 0xDFFF), CBAT_ERR_INVALID_UTF8);
    CBAT_CHECK_EQ_STR(cbat_string_cstr(s), "H\xE2\x82\xAC");
    cbat_string_free(s);
}

CBAT_TEST(test_string_in_arena) {
    CBatArena *arena = cbat_arena_new(0);
    CBatString *s;
    int i;
    CBAT_REQUIRE_NOT_NULL(arena);
    s = cbat_string_new_with(cbat_arena_allocator(arena));
    CBAT_REQUIRE_NOT_NULL(s);
    for (i = 0; i < 100; i++) {
        CBAT_REQUIRE_OK(cbat_string_append(s, "arena "));
    }
    CBAT_CHECK_EQ_SIZE(cbat_string_length(s), 600);
    cbat_arena_free(arena);
}

CBAT_TEST(test_uint64_and_integer_formatting) {
    uint64_t value = 7;
    char text[21];
    CBatString *s = cbat_string_new();
    CBAT_REQUIRE_NOT_NULL(s);
    CBAT_CHECK_OK(cbat_string_to_uint64(" +18446744073709551615 \t", &value));
    CBAT_CHECK_EQ_UINT(value, UINT64_MAX);
    CBAT_CHECK_ERR(cbat_string_to_uint64("18446744073709551616", &value), CBAT_ERR_OVERFLOW);
    CBAT_CHECK_EQ_UINT(value, UINT64_MAX);
    CBAT_CHECK_ERR(cbat_string_to_uint64("18446744073709551616x", &value), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_uint64("-0", &value), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_uint64("+", &value), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_uint64("  ", &value), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_uint64("0x10", &value), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_string_to_uint64(NULL, &value), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_string_to_uint64("1", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_EQ_SIZE(cbat_format_int64(INT64_MIN, text), 20);
    CBAT_CHECK_EQ_STR(text, "-9223372036854775808");
    CBAT_CHECK_EQ_SIZE(cbat_format_int64(INT64_MAX, text), 19);
    CBAT_CHECK_EQ_STR(text, "9223372036854775807");
    CBAT_CHECK_EQ_SIZE(cbat_format_int64(0, text), 1);
    CBAT_CHECK_EQ_STR(text, "0");
    CBAT_CHECK_EQ_SIZE(cbat_format_int64(1, NULL), 0);
    CBAT_REQUIRE_OK(cbat_string_append_int64_grouped(s, INT64_MIN, ','));
    CBAT_CHECK_EQ_STR(s->data, "-9,223,372,036,854,775,808");
    cbat_string_clear(s);
    CBAT_REQUIRE_OK(cbat_string_append_int64_grouped(s, 1234567, '_'));
    CBAT_CHECK_EQ_STR(s->data, "1_234_567");
    cbat_string_clear(s);
    CBAT_REQUIRE_OK(cbat_string_append_int64_grouped(s, -1234, 0));
    CBAT_CHECK_EQ_STR(s->data, "-1234");
    CBAT_CHECK_ERR(cbat_string_append_int64_grouped(NULL, 1, ','), CBAT_ERR_INVALID_ARGUMENT);
    cbat_string_free(s);
}

CBAT_TEST(test_shortest_double_notation) {
    const double values[] = {0.0, -0.0, 0.1, 1.25, 10.0, 1000.0, 0.0001, DBL_MAX, DBL_MIN,
        4.9406564584124654e-324, 9007199254740992.0, 1.2345678901234567};
    size_t i;
    char text[32];
    CBatString *s = cbat_string_new();
    CBAT_REQUIRE_NOT_NULL(s);
    for (i = 0; i < CBAT_ARRAY_LEN(values); i++) {
        double parsed = 0;
        size_t length = cbat_format_double(values[i], text);
        CBAT_CHECK(length > 0 && length < sizeof(text));
        CBAT_CHECK_EQ_SIZE(length, strlen(text));
        CBAT_REQUIRE_OK(cbat_string_to_double(text, &parsed));
        CBAT_CHECK_EQ_MEM(&parsed, &values[i], sizeof(double));
    }
    cbat_format_double(0.1, text); CBAT_CHECK_EQ_STR(text, "0.1");
    cbat_format_double(10.0, text); CBAT_CHECK_EQ_STR(text, "10");
    cbat_format_double(1000.0, text); CBAT_CHECK_EQ_STR(text, "1e3");
    cbat_format_double(0.0001, text); CBAT_CHECK_EQ_STR(text, "1e-4");
    cbat_format_double(-0.0, text); CBAT_CHECK_EQ_STR(text, "-0");
    cbat_format_double(HUGE_VAL, text); CBAT_CHECK_EQ_STR(text, "inf");
    cbat_format_double(-HUGE_VAL, text); CBAT_CHECK_EQ_STR(text, "-inf");
    cbat_format_double(NAN, text); CBAT_CHECK_EQ_STR(text, "nan");
    CBAT_CHECK_EQ_SIZE(cbat_format_double(1.0, NULL), 0);
    CBAT_REQUIRE_OK(cbat_string_append_double(s, 0.1));
    CBAT_CHECK_EQ_STR(s->data, "0.1");
    CBAT_CHECK_ERR(cbat_string_append_double(NULL, 1.0), CBAT_ERR_INVALID_ARGUMENT);
    cbat_string_free(s);
}

CBAT_TEST(test_random_double_round_trips) {
    CBatRandom rng;
    size_t count = 0;
    cbat_random_seed(&rng, 42);
    while (count < 100000) {
        uint64_t bits = cbat_random_u64(&rng);
        double value, parsed;
        char text[32];
        memcpy(&value, &bits, sizeof(value));
        if (!isfinite(value)) continue;
        CBAT_REQUIRE(cbat_format_double(value, text) > 0);
        CBAT_REQUIRE_OK(cbat_string_to_double(text, &parsed));
        CBAT_REQUIRE(memcmp(&value, &parsed, sizeof(value)) == 0);
        count++;
    }
}

CBAT_TEST(test_numeric_locale_and_allocation_errors) {
    const char *locales[] = {"de_DE.UTF-8", "de_DE.utf8", "German_Germany.1252", "de-DE"};
    char *saved = cbat_strdup(NULL, setlocale(LC_NUMERIC, NULL));
    CBatString *large = cbat_string_repeat("0", 300);
    CBatString *output = cbat_string_from("123456789012345");
    size_t i;
    double value = 9;
    bool available = false;
    CBAT_REQUIRE_NOT_NULL(saved); CBAT_REQUIRE_NOT_NULL(large); CBAT_REQUIRE_NOT_NULL(output);
    CBAT_REQUIRE_OK(cbat_string_insert(large, 0, "0."));
    CBAT_REQUIRE_OK(cbat_string_append(large, "1"));
    for (i = 0; i < CBAT_ARRAY_LEN(locales); i++) {
        if (setlocale(LC_NUMERIC, locales[i]) != NULL) { available = true; break; }
    }
    if (available) {
        char text[32];
        CBatError err;
        CBAT_CHECK_OK(cbat_string_to_double("1.25", &value));
        CBAT_CHECK(value == 1.25);
        CBAT_CHECK_ERR(cbat_string_to_double("1,25", &value), CBAT_ERR_PARSE);
        cbat_format_double(1.25, text);
        CBAT_CHECK_EQ_STR(text, "1.25");
        if (strcmp(localeconv()->decimal_point, ".") != 0) {
            value = 9;
            cbat_test_fail_allocations_after(0);
            err = cbat_string_to_double(large->data, &value);
            cbat_test_stop_failing_allocations();
            CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
            CBAT_CHECK(value == 9);
        }
    }
    CBAT_CHECK_NOT_NULL(setlocale(LC_NUMERIC, saved));
    cbat_test_fail_allocations_after(0);
    {
        CBatError a = cbat_string_append_double(output, DBL_MAX);
        CBatError b = cbat_string_append_int64_grouped(output, INT64_MIN, ',');
        cbat_test_stop_failing_allocations();
        CBAT_CHECK_ERR(a, CBAT_ERR_NO_MEMORY);
        CBAT_CHECK_ERR(b, CBAT_ERR_NO_MEMORY);
    }
    CBAT_CHECK_EQ_STR(output->data, "123456789012345");
    cbat_free(saved); cbat_string_free(large); cbat_string_free(output);
}

CBAT_TEST(test_out_of_memory) {
    size_t n;
    for (n = 0; n < 6; n++) {
        CBatString *s;
        CBatList *parts = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(n);
        s = cbat_string_from("seed");
        if (s != NULL) {
            err = cbat_string_append_format(s, "%0100d", 1);
            CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
            CBAT_CHECK_EQ_INT(s->data[s->length], '\0');
            cbat_string_free(s);
        }
        err = cbat_string_split("a,b,c", ",", &parts);
        CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
        if (err == CBAT_OK) {
            cbat_list_free(parts);
        }
        cbat_test_stop_failing_allocations();
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_new_from_and_free),
    CBAT_TEST_ENTRY(test_from_format_and_append_format),
    CBAT_TEST_ENTRY(test_append_grows),
    CBAT_TEST_ENTRY(test_append_self),
    CBAT_TEST_ENTRY(test_copy_set_clear),
    CBAT_TEST_ENTRY(test_steal),
    CBAT_TEST_ENTRY(test_insert_erase_truncate),
    CBAT_TEST_ENTRY(test_replace),
    CBAT_TEST_ENTRY(test_case_and_trim),
    CBAT_TEST_ENTRY(test_find_rfind_count_contains),
    CBAT_TEST_ENTRY(test_compare),
    CBAT_TEST_ENTRY(test_substring_and_repeat),
    CBAT_TEST_ENTRY(test_split),
    CBAT_TEST_ENTRY(test_split_whitespace),
    CBAT_TEST_ENTRY(test_join),
    CBAT_TEST_ENTRY(test_to_int64),
    CBAT_TEST_ENTRY(test_to_double),
    CBAT_TEST_ENTRY(test_utf8_decode_encode),
    CBAT_TEST_ENTRY(test_utf8_count_and_validate),
    CBAT_TEST_ENTRY(test_append_codepoint),
    CBAT_TEST_ENTRY(test_string_in_arena),
    CBAT_TEST_ENTRY(test_uint64_and_integer_formatting),
    CBAT_TEST_ENTRY(test_shortest_double_notation),
    CBAT_TEST_ENTRY(test_random_double_round_trips),
    CBAT_TEST_ENTRY(test_numeric_locale_and_allocation_errors),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("string", tests)
