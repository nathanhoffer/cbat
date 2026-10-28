/*
 * cbat/math.h - integer and floating-point helpers missing from <math.h>
 * (cbat's answer to Python's math.gcd, math.isclose, math.isqrt, ...).
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_MATH_H
#define CBAT_MATH_H

#include "cbat/base.h"

CBAT_BEGIN_DECLS

/** Greatest common divisor (always >= 0; gcd(0, 0) == 0). */
CBAT_API int64_t cbat_math_gcd(int64_t a, int64_t b);

/** Least common multiple (>= 0). CBAT_ERR_OVERFLOW if it does not fit. */
CBAT_API CBatError cbat_math_lcm(int64_t a, int64_t b, int64_t *out);

/** floor(sqrt(n)) computed exactly for every 64-bit n. */
CBAT_API uint64_t cbat_math_isqrt(uint64_t n);

/** Primality by trial division (fine for tutorials, slow above ~1e15). */
CBAT_API bool cbat_math_is_prime(uint64_t n);

/** Clamps x into [low, high]. */
CBAT_API double cbat_math_clamp(double x, double low, double high);

/** Linear interpolation: a + (b - a) * t. */
CBAT_API double cbat_math_lerp(double a, double b, double t);

/**
 * Python's math.isclose: |a-b| <= max(rel_tol * max(|a|,|b|), abs_tol).
 * Typical: rel_tol = 1e-9, abs_tol = 0.
 */
CBAT_API bool cbat_math_is_close(double a, double b, double rel_tol, double abs_tol);

/** Checked 64-bit arithmetic: false on overflow (out untouched). */
CBAT_API bool cbat_math_add_int64(int64_t a, int64_t b, int64_t *out);
CBAT_API bool cbat_math_mul_int64(int64_t a, int64_t b, int64_t *out);

CBAT_END_DECLS

#endif /* CBAT_MATH_H */
