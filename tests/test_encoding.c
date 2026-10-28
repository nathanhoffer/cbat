#include <string.h>

#include "cbat/encoding.h"
#include "cbat/test.h"

static void check_b64(const char *input, const char *expected) {
    CBatString *encoded = NULL;
    CBatString *decoded = NULL;
    if (!CBAT_CHECK_OK(cbat_base64_encode(input, strlen(input), &encoded))) {
        return;
    }
    CBAT_CHECK_EQ_STR(cbat_string_cstr(encoded), expected);
    if (CBAT_CHECK_OK(cbat_base64_decode(encoded->data, encoded->length, &decoded))) {
        CBAT_CHECK_EQ_STR(cbat_string_cstr(decoded), input);
        cbat_string_free(decoded);
    }
    cbat_string_free(encoded);
}

CBAT_TEST(test_base64_rfc4648_vectors) {
    check_b64("", "");
    check_b64("f", "Zg==");
    check_b64("fo", "Zm8=");
    check_b64("foo", "Zm9v");
    check_b64("foob", "Zm9vYg==");
    check_b64("fooba", "Zm9vYmE=");
    check_b64("foobar", "Zm9vYmFy");
}

CBAT_TEST(test_base64url_and_binary) {
    const unsigned char bytes[] = {0xFB, 0xFF, 0x00, 0xFE};
    CBatString *encoded = NULL;
    CBatString *decoded = NULL;
    CBAT_REQUIRE_OK(cbat_base64url_encode(bytes, sizeof(bytes), &encoded));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(encoded), "-_8A_g");
    CBAT_REQUIRE_OK(cbat_base64_decode(encoded->data, encoded->length, &decoded));
    CBAT_CHECK_EQ_SIZE(decoded->length, sizeof(bytes));
    CBAT_CHECK_EQ_MEM(decoded->data, bytes, sizeof(bytes));
    cbat_string_free(encoded);
    cbat_string_free(decoded);
}

CBAT_TEST(test_base64_decode_whitespace_and_errors) {
    CBatString *decoded = NULL;
    const char *wrapped = "Zm9v\nYmFy\r\n";
    CBAT_REQUIRE_OK(cbat_base64_decode(wrapped, strlen(wrapped), &decoded));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(decoded), "foobar");
    cbat_string_free(decoded);
    CBAT_REQUIRE_OK(cbat_base64_decode("Zg", 2, &decoded)); /* missing padding is fine */
    CBAT_CHECK_EQ_STR(cbat_string_cstr(decoded), "f");
    cbat_string_free(decoded);
    CBAT_CHECK_ERR(cbat_base64_decode("Zm9v!", 5, &decoded), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_base64_decode("Z", 1, &decoded), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_base64_decode("Zg==Zg==", 8, &decoded), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_base64_decode("Zg=", 3, &decoded), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_base64_decode(NULL, 3, &decoded), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_hex) {
    const unsigned char bytes[] = {0x00, 0x7F, 0xAB, 0xFF};
    CBatString *encoded = NULL;
    CBatString *decoded = NULL;
    CBAT_REQUIRE_OK(cbat_hex_encode(bytes, sizeof(bytes), &encoded));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(encoded), "007fabff");
    CBAT_REQUIRE_OK(cbat_hex_decode("007FabFF", 8, &decoded));
    CBAT_CHECK_EQ_MEM(decoded->data, bytes, sizeof(bytes));
    cbat_string_free(encoded);
    cbat_string_free(decoded);
    CBAT_CHECK_ERR(cbat_hex_decode("abc", 3, &decoded), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_hex_decode("zz", 2, &decoded), CBAT_ERR_PARSE);
    CBAT_REQUIRE_OK(cbat_hex_encode(NULL, 0, &encoded));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(encoded), "");
    cbat_string_free(encoded);
}

CBAT_TEST(test_out_of_memory) {
    CBatString *s = NULL;
    cbat_test_fail_allocations_after(0);
    CBAT_CHECK_ERR(cbat_base64_encode("abc", 3, &s), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_hex_decode("ab", 2, &s), CBAT_ERR_NO_MEMORY);
    cbat_test_fail_allocations_after(1);
    CBAT_CHECK_ERR(cbat_hex_encode("abcdefghijklmnopqrstuvwxyz", 26, &s), CBAT_ERR_NO_MEMORY);
    cbat_test_stop_failing_allocations();
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_base64_rfc4648_vectors),
    CBAT_TEST_ENTRY(test_base64url_and_binary),
    CBAT_TEST_ENTRY(test_base64_decode_whitespace_and_errors),
    CBAT_TEST_ENTRY(test_hex),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("encoding", tests)
