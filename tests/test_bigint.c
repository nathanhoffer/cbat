#include "cbat/bigint.h"
#include "cbat/test.h"

static void check_decimal(const CBatBigInt *value, const char *expected) {
    CBatString *text = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_to_string(value, &text));
    CBAT_CHECK_EQ_STR(text->data, expected);
    cbat_string_free(text);
}
CBAT_TEST(test_decimal_and_signed_arithmetic) {
    CBatBigInt *a = NULL, *b = NULL, *out = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_from_string(" \t-00012345678901234567890\r\n", &a));
    CBAT_REQUIRE_OK(cbat_bigint_from_string("99999999999999999999", &b));
    check_decimal(a, "-12345678901234567890");
    CBAT_CHECK(cbat_bigint_compare(a, b) < 0);
    CBAT_CHECK(cbat_bigint_compare(NULL, a) > 0);
    CBAT_REQUIRE_OK(cbat_bigint_add(a, b, &out));
    check_decimal(out, "87654321098765432109");
    cbat_bigint_free(out);
    out = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_subtract(a, b, &out));
    check_decimal(out, "-112345678901234567889");
    cbat_bigint_free(out);
    out = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_multiply(a, b, &out));
    check_decimal(out, "-1234567890123456788987654321098765432110");
    cbat_bigint_free(out);
    out = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_subtract(a, a, &out));
    check_decimal(out, "0");
    cbat_bigint_free(out);
    out = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_from_int64(INT64_MIN, &out));
    check_decimal(out, "-9223372036854775808");
    cbat_bigint_free(out);
    out = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_from_string("-00000", &out));
    check_decimal(out, "0");
    CBAT_CHECK_EQ_INT(cbat_bigint_compare(out, NULL), 0);
    cbat_bigint_free(out);
    cbat_bigint_free(a);
    cbat_bigint_free(b);
}
CBAT_TEST(test_division_signs_and_identity) {
    const char *as[] = {"123456789012345678901234567890",
                        "-123456789012345678901234567890",
                        "123456789012345678901234567890",
                        "-123456789012345678901234567890",
                        "5",
                        "0"};
    const char *bs[] = {"9876543210123456789",  "9876543210123456789",   "-9876543210123456789",
                        "-9876543210123456789", "999999999999999999999", "2"};
    size_t i;
    for (i = 0; i < 6; i++) {
        CBatBigInt *a = NULL, *b = NULL, *q = NULL, *r = NULL, *product = NULL, *sum = NULL;
        CBAT_REQUIRE_OK(cbat_bigint_from_string(as[i], &a));
        CBAT_REQUIRE_OK(cbat_bigint_from_string(bs[i], &b));
        CBAT_REQUIRE_OK(cbat_bigint_divmod(a, b, &q, &r));
        CBAT_REQUIRE_OK(cbat_bigint_multiply(q, b, &product));
        CBAT_REQUIRE_OK(cbat_bigint_add(product, r, &sum));
        CBAT_CHECK_EQ_INT(cbat_bigint_compare(a, sum), 0);
        if (i < 4) {
            check_decimal(q, i == 1 || i == 2 ? "-12499999887" : "12499999887");
            check_decimal(r, i == 1 || i == 3 ? "-1851851782685185047" : "1851851782685185047");
        }
        cbat_bigint_free(a);
        cbat_bigint_free(b);
        cbat_bigint_free(q);
        cbat_bigint_free(r);
        cbat_bigint_free(product);
        cbat_bigint_free(sum);
    }
}
CBAT_TEST(test_power_factorial_and_allocator) {
    CBatBigInt *base = NULL, *power = NULL, *factorial = NULL;
    CBatArena *arena = cbat_arena_new(4096);
    int64_t i;
    CBAT_REQUIRE_NOT_NULL(arena);
    CBAT_REQUIRE_OK(cbat_bigint_from_string_with("-2", cbat_arena_allocator(arena), &base));
    CBAT_REQUIRE_OK(cbat_bigint_pow(base, 100, &power));
    check_decimal(power, "1267650600228229401496703205376");
    cbat_bigint_free(power);
    power = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_pow(base, 0, &power));
    check_decimal(power, "1");
    cbat_bigint_free(power);
    cbat_bigint_free(base);
    cbat_arena_free(arena);
    CBAT_REQUIRE_OK(cbat_bigint_from_int64(1, &factorial));
    for (i = 2; i <= 100; i++) {
        CBatBigInt *factor = NULL, *next = NULL;
        CBAT_REQUIRE_OK(cbat_bigint_from_int64(i, &factor));
        CBAT_REQUIRE_OK(cbat_bigint_multiply(factorial, factor, &next));
        cbat_bigint_free(factorial);
        factorial = next;
        cbat_bigint_free(factor);
    }
    check_decimal(factorial,
                  "93326215443944152681699238856266700490715968264381621468592963895217599993229915"
                  "608941463976156518286253697920827223758251185210916864000000000000000000000000");
    cbat_bigint_free(factorial);
}
CBAT_TEST(test_errors_and_all_oom) {
    CBatBigInt *a = NULL, *b = NULL, *zero = cbat_bigint_new();
    size_t op, n;
    const char *bad[] = {"", "+", "--1", "1 2", "0x10", "1_000"};
    CBAT_REQUIRE_NOT_NULL(zero);
    for (n = 0; n < sizeof(bad) / sizeof(bad[0]); n++)
        CBAT_CHECK_ERR(cbat_bigint_from_string(bad[n], &a), CBAT_ERR_PARSE);
    CBAT_REQUIRE_OK(cbat_bigint_from_string("123456789012345678901234567890", &a));
    CBAT_REQUIRE_OK(cbat_bigint_from_string("1234567890123", &b));
    CBAT_CHECK_ERR(cbat_bigint_divmod(a, zero, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    for (op = 0; op < 10; op++) {
        bool success = false;
        for (n = 0; n < 100; n++) {
            CBatBigInt *out = NULL, *r = NULL;
            CBatString *text = NULL;
            CBatError err;
            cbat_test_fail_allocations_after(n);
            switch (op) {
            case 0:
                out = cbat_bigint_new();
                err = out == NULL ? CBAT_ERR_NO_MEMORY : CBAT_OK;
                break;
            case 1:
                err = cbat_bigint_from_string("12345678901234567890", &out);
                break;
            case 2:
                err = cbat_bigint_from_int64(INT64_MIN, &out);
                break;
            case 3:
                err = cbat_bigint_copy(a, &out);
                break;
            case 4:
                err = cbat_bigint_add(a, b, &out);
                break;
            case 5:
                err = cbat_bigint_subtract(a, b, &out);
                break;
            case 6:
                err = cbat_bigint_multiply(a, b, &out);
                break;
            case 7:
                err = cbat_bigint_divmod(a, b, &out, &r);
                break;
            case 8:
                err = cbat_bigint_pow(b, 7, &out);
                break;
            default:
                err = cbat_bigint_to_string(a, &text);
                break;
            }
            cbat_test_stop_failing_allocations();
            if (err == CBAT_OK)
                success = true;
            else {
                CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
                CBAT_CHECK_NULL(out);
                CBAT_CHECK_NULL(r);
                CBAT_CHECK_NULL(text);
            }
            cbat_bigint_free(out);
            cbat_bigint_free(r);
            cbat_string_free(text);
            if (success)
                break;
        }
        CBAT_CHECK(success);
    }
    cbat_bigint_free(a);
    cbat_bigint_free(b);
    cbat_bigint_free(zero);
    cbat_bigint_free(NULL);
}
CBAT_TEST(test_output_contracts_and_zero_allocator) {
    CBatBigInt *a = NULL, *b = NULL, *q = NULL, *r = NULL, *zero = NULL;
    CBatArena *arena = cbat_arena_new(4096);
    CBatString *text = NULL;
    CBAT_REQUIRE_NOT_NULL(arena);
    zero = cbat_bigint_new_with(cbat_arena_allocator(arena));
    CBAT_REQUIRE_NOT_NULL(zero);
    CBAT_REQUIRE_OK(cbat_bigint_from_int64(20, &a));
    CBAT_REQUIRE_OK(cbat_bigint_from_int64(-3, &b));
    CBAT_REQUIRE_OK(cbat_bigint_divmod(a, b, &q, NULL));
    check_decimal(q, "-6");
    cbat_bigint_free(q);
    q = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_divmod(a, b, NULL, &r));
    check_decimal(r, "2");
    cbat_bigint_free(r);
    r = NULL;
    CBAT_CHECK_ERR(cbat_bigint_divmod(a, zero, &q, &r), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_NULL(q);
    CBAT_CHECK_NULL(r);
    CBAT_CHECK_ERR(cbat_bigint_divmod(a, b, &q, &q), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_from_string(NULL, &q), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_from_string("1", NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_from_int64(1, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_copy(NULL, &q), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_to_string(NULL, &text), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_add(NULL, b, &q), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_subtract(a, NULL, &q), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_multiply(a, b, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bigint_pow(NULL, 2, &q), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_REQUIRE_OK(cbat_bigint_pow(zero, 0, &q));
    check_decimal(q, "1");
    cbat_bigint_free(q);
    q = NULL;
    CBAT_REQUIRE_OK(cbat_bigint_add(zero, b, &q));
    CBAT_REQUIRE_OK(cbat_bigint_to_string(q, &text));
    CBAT_CHECK(text->allocator == cbat_arena_allocator(arena));
    CBAT_CHECK_EQ_STR(text->data, "-3");
    cbat_string_free(text);
    cbat_bigint_free(q);
    cbat_bigint_free(a);
    cbat_bigint_free(b);
    cbat_bigint_free(zero);
    cbat_arena_free(arena);
}
static const CBatTestCase tests[] = {CBAT_TEST_ENTRY(test_decimal_and_signed_arithmetic),
                                     CBAT_TEST_ENTRY(test_division_signs_and_identity),
                                     CBAT_TEST_ENTRY(test_power_factorial_and_allocator),
                                     CBAT_TEST_ENTRY(test_errors_and_all_oom),
                                     CBAT_TEST_ENTRY(test_output_contracts_and_zero_allocator)};
CBAT_TEST_MAIN("bigint", tests)
