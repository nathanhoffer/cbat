#include <math.h>

#include "cbat/math.h"
#include "cbat/test.h"

CBAT_TEST(test_gcd_lcm) {
    int64_t out = 0;
    CBAT_CHECK_EQ_INT(cbat_math_gcd(12, 18), 6);
    CBAT_CHECK_EQ_INT(cbat_math_gcd(-12, 18), 6);
    CBAT_CHECK_EQ_INT(cbat_math_gcd(0, 5), 5);
    CBAT_CHECK_EQ_INT(cbat_math_gcd(0, 0), 0);
    CBAT_CHECK_EQ_INT(cbat_math_gcd(INT64_MIN, 0), INT64_MAX); /* saturates */
    CBAT_CHECK_OK(cbat_math_lcm(4, 6, &out));
    CBAT_CHECK_EQ_INT(out, 12);
    CBAT_CHECK_OK(cbat_math_lcm(-4, 6, &out));
    CBAT_CHECK_EQ_INT(out, 12);
    CBAT_CHECK_OK(cbat_math_lcm(0, 6, &out));
    CBAT_CHECK_EQ_INT(out, 0);
    CBAT_CHECK_ERR(cbat_math_lcm(INT64_MAX, INT64_MAX - 1, &out), CBAT_ERR_OVERFLOW);
    CBAT_CHECK_ERR(cbat_math_lcm(1, 2, NULL), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_isqrt) {
    CBAT_CHECK_EQ_UINT(cbat_math_isqrt(0), 0);
    CBAT_CHECK_EQ_UINT(cbat_math_isqrt(1), 1);
    CBAT_CHECK_EQ_UINT(cbat_math_isqrt(15), 3);
    CBAT_CHECK_EQ_UINT(cbat_math_isqrt(16), 4);
    CBAT_CHECK_EQ_UINT(cbat_math_isqrt(999999999999999999ull), 999999999u);
    CBAT_CHECK_EQ_UINT(cbat_math_isqrt(UINT64_MAX), 4294967295u);
    CBAT_CHECK_EQ_UINT(cbat_math_isqrt(4294967296ull * 4294967295ull), 4294967295u);
}

CBAT_TEST(test_is_prime) {
    static const uint64_t primes[] = {2, 3, 5, 7, 11, 13, 97, 7919, 1000000007ull};
    static const uint64_t composites[] = {0, 1, 4, 9, 25, 91, 7917, 1000000007ull * 3};
    size_t i;
    for (i = 0; i < CBAT_ARRAY_LEN(primes); i++) {
        CBAT_CHECK(cbat_math_is_prime(primes[i]));
    }
    for (i = 0; i < CBAT_ARRAY_LEN(composites); i++) {
        CBAT_CHECK(!cbat_math_is_prime(composites[i]));
    }
}

CBAT_TEST(test_clamp_lerp_is_close) {
    CBAT_CHECK_NEAR(cbat_math_clamp(5, 0, 1), 1, 0);
    CBAT_CHECK_NEAR(cbat_math_clamp(-5, 0, 1), 0, 0);
    CBAT_CHECK_NEAR(cbat_math_clamp(0.5, 0, 1), 0.5, 0);
    CBAT_CHECK_NEAR(cbat_math_lerp(10, 20, 0.25), 12.5, 0);
    CBAT_CHECK(cbat_math_is_close(0.1 + 0.2, 0.3, 1e-9, 0));
    CBAT_CHECK(!cbat_math_is_close(1.0, 1.1, 1e-9, 0));
    CBAT_CHECK(cbat_math_is_close(0.0, 1e-12, 1e-9, 1e-10));
    CBAT_CHECK(!cbat_math_is_close(0.0, 1e-12, 1e-9, 0));
    CBAT_CHECK(cbat_math_is_close(INFINITY, INFINITY, 1e-9, 0));
    CBAT_CHECK(!cbat_math_is_close(NAN, NAN, 1e-9, 0));
}

CBAT_TEST(test_checked_int64) {
    int64_t out = 0;
    CBAT_CHECK(cbat_math_add_int64(1, 2, &out));
    CBAT_CHECK_EQ_INT(out, 3);
    CBAT_CHECK(!cbat_math_add_int64(INT64_MAX, 1, &out));
    CBAT_CHECK(!cbat_math_add_int64(INT64_MIN, -1, &out));
    CBAT_CHECK(cbat_math_mul_int64(-4, 5, &out));
    CBAT_CHECK_EQ_INT(out, -20);
    CBAT_CHECK(!cbat_math_mul_int64(INT64_MAX, 2, &out));
    CBAT_CHECK(!cbat_math_mul_int64(INT64_MIN, -1, &out));
    CBAT_CHECK(!cbat_math_mul_int64(-1, INT64_MIN, &out));
    CBAT_CHECK(cbat_math_mul_int64(0, INT64_MIN, &out));
    CBAT_CHECK_EQ_INT(out, 0);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_gcd_lcm),
    CBAT_TEST_ENTRY(test_isqrt),
    CBAT_TEST_ENTRY(test_is_prime),
    CBAT_TEST_ENTRY(test_clamp_lerp_is_close),
    CBAT_TEST_ENTRY(test_checked_int64),
};

CBAT_TEST_MAIN("math", tests)
