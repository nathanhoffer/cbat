/*
 * cbat/math.c - integer/float helpers.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/math.h"

#include <math.h>

static uint64_t cbat__math_abs(int64_t x) {
    /* Works for INT64_MIN as well. */
    return x < 0 ? 0u - (uint64_t)x : (uint64_t)x;
}

static uint64_t cbat__math_gcd_u(uint64_t a, uint64_t b) {
    while (b != 0) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int64_t cbat_math_gcd(int64_t a, int64_t b) {
    uint64_t g = cbat__math_gcd_u(cbat__math_abs(a), cbat__math_abs(b));
    /* gcd(INT64_MIN, 0) = 2^63 does not fit; saturate. */
    return g > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)g;
}

CBatError cbat_math_lcm(int64_t a, int64_t b, int64_t *out) {
    uint64_t ua = cbat__math_abs(a);
    uint64_t ub = cbat__math_abs(b);
    uint64_t g;
    uint64_t q;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (ua == 0 || ub == 0) {
        *out = 0;
        return CBAT_OK;
    }
    g = cbat__math_gcd_u(ua, ub);
    q = ua / g;
    if (q > (uint64_t)INT64_MAX / ub) {
        return CBAT_ERR_OVERFLOW;
    }
    *out = (int64_t)(q * ub);
    return CBAT_OK;
}

uint64_t cbat_math_isqrt(uint64_t n) {
    uint64_t x;
    if (n < 2) {
        return n;
    }
    /* Start from the floating estimate, then correct by integer steps. */
    x = (uint64_t)sqrt((double)n);
    if (x > 0xFFFFFFFFu) {
        x = 0xFFFFFFFFu;
    }
    while (x * x > n) {
        x--;
    }
    while (x < 0xFFFFFFFFu && (x + 1) * (x + 1) <= n) {
        x++;
    }
    return x;
}

bool cbat_math_is_prime(uint64_t n) {
    uint64_t i;
    uint64_t limit;
    if (n < 2) {
        return false;
    }
    if (n < 4) {
        return true;
    }
    if (n % 2 == 0 || n % 3 == 0) {
        return false;
    }
    limit = cbat_math_isqrt(n);
    for (i = 5; i <= limit; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return false;
        }
    }
    return true;
}

double cbat_math_clamp(double x, double low, double high) {
    if (x < low) {
        return low;
    }
    if (x > high) {
        return high;
    }
    return x;
}

double cbat_math_lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

bool cbat_math_is_close(double a, double b, double rel_tol, double abs_tol) {
    double diff;
    double scale;
    if (a == b) {
        return true; /* handles equal infinities */
    }
    if (a != a || b != b || a == HUGE_VAL || a == -HUGE_VAL || b == HUGE_VAL || b == -HUGE_VAL) {
        return false;
    }
    diff = fabs(a - b);
    scale = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return diff <= rel_tol * scale || diff <= abs_tol;
}

bool cbat_math_add_int64(int64_t a, int64_t b, int64_t *out) {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
        return false;
    }
    if (out != NULL) {
        *out = a + b;
    }
    return true;
}

bool cbat_math_mul_int64(int64_t a, int64_t b, int64_t *out) {
    if (a != 0 && b != 0) {
        if (a > 0) {
            if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a)) {
                return false;
            }
        } else {
            if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b)) {
                return false;
            }
        }
    }
    if (out != NULL) {
        *out = a * b;
    }
    return true;
}
