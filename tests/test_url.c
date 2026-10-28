#include <string.h>
#include "cbat/url.h"
#include "cbat/test.h"

CBAT_TEST(test_codecs_and_binary_decoding) {
    CBatString *encoded = NULL, *decoded = NULL;
    char bytes[256];
    size_t i;
    CBAT_REQUIRE_OK(cbat_url_encode("AZaz09-._~ /+?&=", &encoded));
    CBAT_CHECK_EQ_STR(encoded->data, "AZaz09-._~%20%2F%2B%3F%26%3D");
    cbat_string_free(encoded);
    CBAT_REQUIRE_OK(cbat_url_encode_form("a b+c", &encoded));
    CBAT_CHECK_EQ_STR(encoded->data, "a+b%2Bc"); cbat_string_free(encoded);
    CBAT_REQUIRE_OK(cbat_url_decode("a+b%20c%00%ff", &decoded));
    CBAT_CHECK_EQ_SIZE(decoded->length, 7);
    CBAT_CHECK_EQ_MEM(decoded->data, "a+b c\0\xff", 7);
    cbat_string_free(decoded); decoded = NULL;
    for (i = 1; i < 256; i++) bytes[i - 1] = (char)i;
    bytes[255] = '\0';
    CBAT_REQUIRE_OK(cbat_url_encode(bytes, &encoded));
    CBAT_REQUIRE_OK(cbat_url_decode(encoded->data, &decoded));
    CBAT_CHECK_EQ_SIZE(decoded->length, 255); CBAT_CHECK_EQ_MEM(decoded->data, bytes, 255);
    cbat_string_free(encoded); cbat_string_free(decoded); decoded = NULL;
    CBAT_CHECK_ERR(cbat_url_decode("%", &decoded), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_url_decode("%0", &decoded), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_url_decode("%GG", &decoded), CBAT_ERR_PARSE);
    CBAT_CHECK_NULL(decoded);
    CBAT_CHECK_ERR(cbat_url_encode(NULL, &encoded), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_encode("x", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_encode_form(NULL, &encoded), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_encode_form("x", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_decode(NULL, &decoded), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_decode("x", NULL), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_absolute_relative_and_literal_hosts) {
    CBatUrl *url = NULL;
    const char *literals[] = {"http://[::]/", "http://[::1]:80/", "http://[2001:db8::1]/",
        "http://[1:2:3:4:5:6:7:8]/", "http://[::ffff:192.0.2.1]/", "http://[v1.a:!$]/"};
    size_t i;
    CBAT_REQUIRE_OK(cbat_url_parse("HTTPS://user:pass@Example.com:8443/a%2Fb?x=1#part", &url, NULL));
    CBAT_CHECK_EQ_STR(url->scheme->data, "HTTPS"); CBAT_CHECK_EQ_STR(url->user->data, "user:pass");
    CBAT_CHECK_EQ_STR(url->host->data, "Example.com"); CBAT_CHECK_EQ_INT(url->port, 8443);
    CBAT_CHECK_EQ_STR(url->path->data, "/a%2Fb"); CBAT_CHECK_EQ_STR(url->query->data, "x=1");
    CBAT_CHECK_EQ_STR(url->fragment->data, "part"); cbat_url_free(url);
    CBAT_REQUIRE_OK(cbat_url_parse("../path?a?b#x?y", &url, NULL));
    CBAT_CHECK_EQ_STR(url->scheme->data, ""); CBAT_CHECK_EQ_STR(url->host->data, "");
    CBAT_CHECK_EQ_STR(url->path->data, "../path"); CBAT_CHECK_EQ_STR(url->query->data, "a?b");
    CBAT_CHECK_EQ_STR(url->fragment->data, "x?y"); CBAT_CHECK_EQ_INT(url->port, -1); cbat_url_free(url);
    CBAT_REQUIRE_OK(cbat_url_parse("mailto:user@example.com", &url, NULL));
    CBAT_CHECK_EQ_STR(url->scheme->data, "mailto"); CBAT_CHECK_EQ_STR(url->path->data, "user@example.com");
    cbat_url_free(url);
    CBAT_REQUIRE_OK(cbat_url_parse("file:///tmp/file", &url, NULL));
    CBAT_CHECK_EQ_STR(url->host->data, ""); CBAT_CHECK_EQ_STR(url->path->data, "/tmp/file"); cbat_url_free(url);
    CBAT_REQUIRE_OK(cbat_url_parse("//host:65535", &url, NULL));
    CBAT_CHECK_EQ_STR(url->scheme->data, ""); CBAT_CHECK_EQ_STR(url->host->data, "host");
    CBAT_CHECK_EQ_INT(url->port, 65535); cbat_url_free(url);
    CBAT_REQUIRE_OK(cbat_url_parse("http://host:", &url, NULL));
    CBAT_CHECK_EQ_INT(url->port, -1); cbat_url_free(url);
    CBAT_REQUIRE_OK(cbat_url_parse("", &url, NULL));
    CBAT_CHECK_EQ_STR(url->path->data, ""); cbat_url_free(url);
    for (i = 0; i < CBAT_ARRAY_LEN(literals); i++) {
        CBAT_REQUIRE_OK(cbat_url_parse(literals[i], &url, NULL));
        CBAT_CHECK(strchr(url->host->data, '[') == NULL);
        cbat_url_free(url);
    }
    cbat_url_free(NULL);
}

CBAT_TEST(test_parse_failures_and_positions) {
    const char *invalid[] = {"http://a b/", "1bad:x", "http://[::1", "http://[:::1]/",
        "http://[1:2:3:4:5:6:7:8:9]/", "http://[::ffff:300.0.2.1]/", "http://[v1.]/",
        "http://host:65536/", "http://host:-1/", "http://host:a/", "http://a@@b/",
        "path#first#second", "http://host/%", "http://host/\xC3\xA9", "http://host/\n", "a[b]"};
    CBatUrl *url = NULL;
    CBatParseInfo info;
    size_t i;
    for (i = 0; i < CBAT_ARRAY_LEN(invalid); i++) {
        memset(&info, 0, sizeof(info));
        CBAT_CHECK_ERR(cbat_url_parse(invalid[i], &url, &info), CBAT_ERR_PARSE);
        CBAT_CHECK_NULL(url); CBAT_CHECK_NOT_NULL(info.message); CBAT_CHECK_EQ_SIZE(info.line, 1);
        CBAT_CHECK(info.offset <= strlen(invalid[i])); CBAT_CHECK_EQ_SIZE(info.column, info.offset + 1);
    }
    CBAT_CHECK_ERR(cbat_url_parse("http://host/%GG", &url, &info), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_SIZE(info.offset, 12);
    CBAT_CHECK_ERR(cbat_url_parse(NULL, &url, &info), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_parse("", NULL, &info), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_query_round_trip_and_repeated_keys) {
    CBatDict *params = NULL, *copy = NULL, *wrong = cbat_dict_new(1);
    CBatString *text = NULL;
    CBatString *value;
    CBAT_REQUIRE_NOT_NULL(wrong);
    CBAT_REQUIRE_OK(cbat_url_parse_query("name=Ada+Lovelace&lang=C%2B%2B&name=Grace&flag&=empty&&binary=%00%FF", &params));
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(params), 5);
    value = CBAT_DICT_GET(params, "name", CBatString *, NULL); CBAT_REQUIRE_NOT_NULL(value);
    CBAT_CHECK_EQ_STR(value->data, "Grace");
    value = CBAT_DICT_GET(params, "lang", CBatString *, NULL); CBAT_CHECK_EQ_STR(value->data, "C++");
    value = CBAT_DICT_GET(params, "flag", CBatString *, NULL); CBAT_CHECK_EQ_SIZE(value->length, 0);
    value = CBAT_DICT_GET(params, "binary", CBatString *, NULL);
    CBAT_CHECK_EQ_SIZE(value->length, 2); CBAT_CHECK_EQ_MEM(value->data, "\0\xff", 2);
    CBAT_REQUIRE_OK(cbat_url_build_query(params, &text));
    CBAT_CHECK_EQ_STR(text->data, "name=Grace&lang=C%2B%2B&flag=&=empty&binary=%00%FF");
    CBAT_REQUIRE_OK(cbat_url_parse_query(text->data, &copy));
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(copy), 5);
    cbat_dict_free(copy); copy = NULL; cbat_dict_free(params); cbat_string_free(text); text = NULL;
    CBAT_REQUIRE_OK(cbat_url_parse_query("&&", &params));
    CBAT_CHECK_EQ_SIZE(cbat_dict_length(params), 0); cbat_dict_free(params); params = NULL;
    CBAT_CHECK_ERR(cbat_url_parse_query("bad=%", &params), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_url_parse_query("%00=value", &params), CBAT_ERR_PARSE);
    CBAT_CHECK_NULL(params);
    CBAT_CHECK_ERR(cbat_url_parse_query(NULL, &params), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_parse_query("", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_build_query(NULL, &text), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_url_build_query(wrong, &text), CBAT_ERR_TYPE);
    CBAT_CHECK_ERR(cbat_url_build_query(wrong, NULL), CBAT_ERR_INVALID_ARGUMENT);
    cbat_dict_free(wrong);
}

CBAT_TEST(test_allocation_failures_leave_outputs_unchanged) {
    size_t allowed;
    CBatDict *source = NULL;
    bool encoded = false, form = false, decoded = false, parsed = false, query = false, built = false;
    CBAT_REQUIRE_OK(cbat_url_parse_query("a=one+two&b=%00%FF", &source));
    for (allowed = 0; allowed < 50; allowed++) {
        CBatString *text = NULL;
        CBatUrl *url = NULL;
        CBatDict *params = NULL;
        CBatError err;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_url_encode("a long string with spaces, punctuation and \xC3\xA9", &text);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) encoded = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(text); }
        cbat_string_free(text); text = NULL;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_url_encode_form("a long string with spaces + plus", &text);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) form = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(text); }
        cbat_string_free(text); text = NULL;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_url_decode("a%20long%20string%20with%20spaces", &text);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) decoded = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(text); }
        cbat_string_free(text); text = NULL;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_url_parse("https://user@example.com:443/a?b=c#d", &url, NULL);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) parsed = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(url); }
        cbat_url_free(url);
        cbat_test_fail_allocations_after(allowed);
        err = cbat_url_parse_query("a=1&b=2&a=3", &params);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) query = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(params); }
        cbat_dict_free(params);
        cbat_test_fail_allocations_after(allowed);
        err = cbat_url_build_query(source, &text);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) built = true;
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(text); }
        cbat_string_free(text);
        if (encoded && form && decoded && parsed && query && built) break;
    }
    CBAT_CHECK(encoded && form && decoded && parsed && query && built);
    cbat_dict_free(source);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_codecs_and_binary_decoding),
    CBAT_TEST_ENTRY(test_absolute_relative_and_literal_hosts),
    CBAT_TEST_ENTRY(test_parse_failures_and_positions),
    CBAT_TEST_ENTRY(test_query_round_trip_and_repeated_keys),
    CBAT_TEST_ENTRY(test_allocation_failures_leave_outputs_unchanged)
};
CBAT_TEST_MAIN("url", tests)
