/* Base-10^9 signed limb arithmetic. Part of cbat. MIT License. */
#include "cbat/bigint.h"
#include <stdio.h>
#include <string.h>

#define CBAT__BIG_BASE UINT32_C(1000000000)
struct CBatBigInt {
    uint32_t *limbs;
    size_t length, capacity;
    int sign;
    const CBatAllocator *allocator;
};
CBatBigInt *cbat_bigint_new_with(const CBatAllocator *allocator) {
    CBatBigInt *value;
    if (allocator == NULL)
        allocator = cbat_default_allocator();
    value = (CBatBigInt *)cbat_alloc_zeroed(allocator, sizeof(*value));
    if (value != NULL)
        value->allocator = allocator;
    return value;
}
CBatBigInt *cbat_bigint_new(void) { return cbat_bigint_new_with(NULL); }
void cbat_bigint_free(CBatBigInt *value) {
    if (value != NULL) {
        cbat_release(value->allocator, value->limbs);
        cbat_release(value->allocator, value);
    }
}
static CBatError cbat__big_reserve(CBatBigInt *value, size_t needed) {
    size_t capacity, bytes;
    uint32_t *limbs;
    if (needed <= value->capacity)
        return CBAT_OK;
    capacity = value->capacity == 0 ? 4 : value->capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(uint32_t))
        capacity = needed;
    if (!cbat_size_mul(capacity, sizeof(uint32_t), &bytes))
        return CBAT_ERR_OVERFLOW;
    limbs = (uint32_t *)cbat_resize(value->allocator, value->limbs,
                                    value->capacity * sizeof(uint32_t), bytes);
    if (limbs == NULL)
        return CBAT_ERR_NO_MEMORY;
    value->limbs = limbs;
    value->capacity = capacity;
    return CBAT_OK;
}
static void cbat__big_normalize(CBatBigInt *value) {
    while (value->length != 0 && value->limbs[value->length - 1] == 0)
        value->length--;
    if (value->length == 0)
        value->sign = 0;
}
static bool cbat__big_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
CBatError cbat_bigint_from_string_with(const char *text, const CBatAllocator *allocator,
                                       CBatBigInt **out) {
    const char *start, *end, *p;
    CBatBigInt *value;
    size_t digits;
    int sign = 1;
    CBatError err;
    if (text == NULL || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    start = text;
    end = text + strlen(text);
    while (start < end && cbat__big_space(*start))
        start++;
    while (end > start && cbat__big_space(end[-1]))
        end--;
    if (start < end && (*start == '+' || *start == '-')) {
        if (*start == '-')
            sign = -1;
        start++;
    }
    if (start == end)
        return CBAT_ERR_PARSE;
    for (p = start; p < end; p++)
        if (*p < '0' || *p > '9')
            return CBAT_ERR_PARSE;
    while (start < end && *start == '0')
        start++;
    value = cbat_bigint_new_with(allocator);
    if (value == NULL)
        return CBAT_ERR_NO_MEMORY;
    digits = (size_t)(end - start);
    err = cbat__big_reserve(value, digits == 0 ? 0 : (digits - 1) / 9 + 1);
    if (err != CBAT_OK) {
        cbat_bigint_free(value);
        return err;
    }
    while (end > start) {
        uint32_t limb = 0;
        p = end - start > 9 ? end - 9 : start;
        for (; p < end; p++)
            limb = limb * 10u + (uint32_t)(*p - '0');
        value->limbs[value->length++] = limb;
        end = end - start > 9 ? end - 9 : start;
    }
    value->sign = value->length == 0 ? 0 : sign;
    *out = value;
    return CBAT_OK;
}
CBatError cbat_bigint_from_string(const char *text, CBatBigInt **out) {
    return cbat_bigint_from_string_with(text, NULL, out);
}
CBatError cbat_bigint_from_int64(int64_t number, CBatBigInt **out) {
    CBatBigInt *value;
    uint64_t magnitude = number < 0 ? (uint64_t)(-(number + 1)) + 1u : (uint64_t)number;
    CBatError err;
    if (out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    value = cbat_bigint_new();
    if (value == NULL)
        return CBAT_ERR_NO_MEMORY;
    err = cbat__big_reserve(value, magnitude == 0 ? 0 : 3);
    if (err != CBAT_OK) {
        cbat_bigint_free(value);
        return err;
    }
    while (magnitude != 0) {
        value->limbs[value->length++] = (uint32_t)(magnitude % CBAT__BIG_BASE);
        magnitude /= CBAT__BIG_BASE;
    }
    value->sign = number < 0 ? -1 : number > 0 ? 1 : 0;
    *out = value;
    return CBAT_OK;
}
static CBatError cbat__big_copy_with(const CBatBigInt *value, const CBatAllocator *allocator,
                                     CBatBigInt **out) {
    CBatBigInt *copy;
    CBatError err;
    if (value == NULL || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    copy = cbat_bigint_new_with(allocator);
    if (copy == NULL)
        return CBAT_ERR_NO_MEMORY;
    err = cbat__big_reserve(copy, value->length);
    if (err != CBAT_OK) {
        cbat_bigint_free(copy);
        return err;
    }
    if (value->length != 0)
        memcpy(copy->limbs, value->limbs, value->length * sizeof(uint32_t));
    copy->length = value->length;
    copy->sign = value->sign;
    *out = copy;
    return CBAT_OK;
}
CBatError cbat_bigint_copy(const CBatBigInt *value, CBatBigInt **out) {
    if (value == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    return cbat__big_copy_with(value, value->allocator, out);
}
CBatError cbat_bigint_to_string(const CBatBigInt *value, CBatString **out) {
    CBatString *text;
    CBatError err = CBAT_OK;
    size_t i;
    char digits[16];
    if (value == NULL || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    text = cbat_string_new_with(value->allocator);
    if (text == NULL)
        return CBAT_ERR_NO_MEMORY;
    if (value->sign == 0)
        err = cbat_string_append_char(text, '0');
    else {
        if (value->sign < 0)
            err = cbat_string_append_char(text, '-');
        for (i = value->length; i != 0 && err == CBAT_OK; i--) {
            (void)snprintf(digits, sizeof(digits), i == value->length ? "%u" : "%09u",
                           (unsigned)value->limbs[i - 1]);
            err = cbat_string_append(text, digits);
        }
    }
    if (err != CBAT_OK) {
        cbat_string_free(text);
        return err;
    }
    *out = text;
    return CBAT_OK;
}
static int cbat__big_magnitude(const uint32_t *a, size_t an, const uint32_t *b, size_t bn) {
    size_t i;
    if (an != bn)
        return an > bn ? 1 : -1;
    for (i = an; i != 0; i--)
        if (a[i - 1] != b[i - 1])
            return a[i - 1] > b[i - 1] ? 1 : -1;
    return 0;
}
int cbat_bigint_compare(const CBatBigInt *a, const CBatBigInt *b) {
    int as = a == NULL ? 0 : a->sign, bs = b == NULL ? 0 : b->sign;
    if (as != bs)
        return as > bs ? 1 : -1;
    if (as == 0)
        return 0;
    return as * cbat__big_magnitude(a->limbs, a->length, b->limbs, b->length);
}
static CBatError cbat__big_add_magnitude(CBatBigInt *out, const CBatBigInt *a,
                                         const CBatBigInt *b) {
    size_t i, needed, n = a->length > b->length ? a->length : b->length;
    uint64_t carry = 0;
    CBatError err;
    if (!cbat_size_add(n, 1, &needed))
        return CBAT_ERR_OVERFLOW;
    err = cbat__big_reserve(out, needed);
    if (err != CBAT_OK)
        return err;
    for (i = 0; i < n; i++) {
        uint64_t sum =
            carry + (i < a->length ? a->limbs[i] : 0u) + (i < b->length ? b->limbs[i] : 0u);
        out->limbs[i] = (uint32_t)(sum % CBAT__BIG_BASE);
        carry = sum / CBAT__BIG_BASE;
    }
    out->length = n;
    if (carry != 0)
        out->limbs[out->length++] = (uint32_t)carry;
    return CBAT_OK;
}
static void cbat__big_subtract_raw(uint32_t *a, size_t an, const uint32_t *b, size_t bn) {
    size_t i;
    uint64_t borrow = 0;
    for (i = 0; i < an; i++) {
        uint64_t sub = (i < bn ? b[i] : 0u) + borrow, av = a[i];
        borrow = av < sub ? 1u : 0u;
        a[i] = (uint32_t)(av + borrow * CBAT__BIG_BASE - sub);
    }
}
static CBatError cbat__big_add(const CBatBigInt *a, const CBatBigInt *b, bool subtract,
                               CBatBigInt **out) {
    CBatBigInt *result;
    CBatError err = CBAT_OK;
    int bs, order;
    if (a == NULL || b == NULL || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    bs = subtract ? -b->sign : b->sign;
    if (bs == 0)
        return cbat_bigint_copy(a, out);
    if (a->sign == 0) {
        err = cbat__big_copy_with(b, a->allocator, &result);
        if (err == CBAT_OK) {
            result->sign = bs;
            *out = result;
        }
        return err;
    }
    result = cbat_bigint_new_with(a->allocator);
    if (result == NULL)
        return CBAT_ERR_NO_MEMORY;
    if (a->sign == bs) {
        err = cbat__big_add_magnitude(result, a, b);
        result->sign = a->sign;
    } else {
        const CBatBigInt *large, *small;
        order = cbat__big_magnitude(a->limbs, a->length, b->limbs, b->length);
        large = order >= 0 ? a : b;
        small = order >= 0 ? b : a;
        err = cbat__big_reserve(result, large->length);
        if (err == CBAT_OK) {
            memcpy(result->limbs, large->limbs, large->length * sizeof(uint32_t));
            result->length = large->length;
            result->sign = order >= 0 ? a->sign : bs;
            cbat__big_subtract_raw(result->limbs, result->length, small->limbs, small->length);
            cbat__big_normalize(result);
        }
    }
    if (err != CBAT_OK) {
        cbat_bigint_free(result);
        return err;
    }
    *out = result;
    return CBAT_OK;
}
CBatError cbat_bigint_add(const CBatBigInt *a, const CBatBigInt *b, CBatBigInt **out) {
    return cbat__big_add(a, b, false, out);
}
CBatError cbat_bigint_subtract(const CBatBigInt *a, const CBatBigInt *b, CBatBigInt **out) {
    return cbat__big_add(a, b, true, out);
}
CBatError cbat_bigint_multiply(const CBatBigInt *a, const CBatBigInt *b, CBatBigInt **out) {
    CBatBigInt *result;
    size_t needed, i, j;
    CBatError err;
    if (a == NULL || b == NULL || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_bigint_new_with(a->allocator);
    if (result == NULL)
        return CBAT_ERR_NO_MEMORY;
    if (a->sign != 0 && b->sign != 0) {
        if (!cbat_size_add(a->length, b->length, &needed)) {
            cbat_bigint_free(result);
            return CBAT_ERR_OVERFLOW;
        }
        err = cbat__big_reserve(result, needed);
        if (err != CBAT_OK) {
            cbat_bigint_free(result);
            return err;
        }
        memset(result->limbs, 0, needed * sizeof(uint32_t));
        result->length = needed;
        result->sign = a->sign * b->sign;
        for (i = 0; i < a->length; i++) {
            uint64_t carry = 0;
            for (j = 0; j < b->length; j++) {
                uint64_t product =
                    (uint64_t)a->limbs[i] * b->limbs[j] + result->limbs[i + j] + carry;
                result->limbs[i + j] = (uint32_t)(product % CBAT__BIG_BASE);
                carry = product / CBAT__BIG_BASE;
            }
            result->limbs[i + b->length] = (uint32_t)carry;
        }
        cbat__big_normalize(result);
    }
    *out = result;
    return CBAT_OK;
}
static size_t cbat__big_multiply_small(const CBatBigInt *value, uint32_t multiplier,
                                       uint32_t *out) {
    size_t i, n = value->length;
    uint64_t carry = 0;
    for (i = 0; i < n; i++) {
        uint64_t product = (uint64_t)value->limbs[i] * multiplier + carry;
        out[i] = (uint32_t)(product % CBAT__BIG_BASE);
        carry = product / CBAT__BIG_BASE;
    }
    if (carry != 0)
        out[n++] = (uint32_t)carry;
    while (n != 0 && out[n - 1] == 0)
        n--;
    return n;
}
CBatError cbat_bigint_divmod(const CBatBigInt *a, const CBatBigInt *b, CBatBigInt **quotient,
                             CBatBigInt **remainder) {
    CBatBigInt *q = NULL, *r = NULL;
    uint32_t *scratch = NULL;
    size_t i, needed;
    CBatError err = CBAT_OK;
    if (a == NULL || b == NULL || b->sign == 0 || (quotient == NULL && remainder == NULL) ||
        (quotient != NULL && quotient == remainder))
        return CBAT_ERR_INVALID_ARGUMENT;
    q = cbat_bigint_new_with(a->allocator);
    r = cbat_bigint_new_with(a->allocator);
    if (q == NULL || r == NULL) {
        err = CBAT_ERR_NO_MEMORY;
        goto done;
    }
    err = cbat__big_reserve(q, a->length);
    if (err != CBAT_OK)
        goto done;
    if (a->length != 0)
        memset(q->limbs, 0, a->length * sizeof(uint32_t));
    q->length = a->length;
    q->sign = a->sign * b->sign;
    if (b->length == 1) {
        uint64_t carry = 0;
        err = cbat__big_reserve(r, 1);
        if (err != CBAT_OK)
            goto done;
        for (i = a->length; i != 0; i--) {
            uint64_t current = carry * CBAT__BIG_BASE + a->limbs[i - 1];
            q->limbs[i - 1] = (uint32_t)(current / b->limbs[0]);
            carry = current % b->limbs[0];
        }
        if (carry != 0) {
            r->limbs[0] = (uint32_t)carry;
            r->length = 1;
        }
    } else {
        if (!cbat_size_add(b->length, 1, &needed)) {
            err = CBAT_ERR_OVERFLOW;
            goto done;
        }
        err = cbat__big_reserve(r, needed);
        if (err != CBAT_OK)
            goto done;
        if (needed > SIZE_MAX / sizeof(uint32_t)) {
            err = CBAT_ERR_OVERFLOW;
            goto done;
        }
        scratch = (uint32_t *)cbat_alloc(a->allocator, needed * sizeof(uint32_t));
        if (scratch == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            goto done;
        }
        for (i = a->length; i != 0; i--) {
            uint32_t low = 0, high = CBAT__BIG_BASE - 1;
            if (r->length != 0)
                memmove(r->limbs + 1, r->limbs, r->length * sizeof(uint32_t));
            r->limbs[0] = a->limbs[i - 1];
            r->length++;
            cbat__big_normalize(r);
            while (low < high) {
                uint32_t middle = low + (high - low + 1) / 2;
                size_t length = cbat__big_multiply_small(b, middle, scratch);
                if (cbat__big_magnitude(scratch, length, r->limbs, r->length) <= 0)
                    low = middle;
                else
                    high = middle - 1;
            }
            q->limbs[i - 1] = low;
            if (low != 0) {
                size_t length = cbat__big_multiply_small(b, low, scratch);
                cbat__big_subtract_raw(r->limbs, r->length, scratch, length);
                cbat__big_normalize(r);
            }
        }
    }
    cbat__big_normalize(q);
    r->sign = r->length == 0 ? 0 : a->sign;
    if (quotient != NULL) {
        *quotient = q;
        q = NULL;
    }
    if (remainder != NULL) {
        *remainder = r;
        r = NULL;
    }
done:
    cbat_release(a->allocator, scratch);
    cbat_bigint_free(q);
    cbat_bigint_free(r);
    return err;
}
CBatError cbat_bigint_pow(const CBatBigInt *value, uint64_t exponent, CBatBigInt **out) {
    CBatBigInt *result = NULL, *base = NULL, *next = NULL;
    CBatError err;
    if (value == NULL || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_bigint_new_with(value->allocator);
    if (result == NULL)
        return CBAT_ERR_NO_MEMORY;
    err = cbat__big_reserve(result, 1);
    if (err != CBAT_OK)
        goto done;
    result->limbs[0] = 1;
    result->length = 1;
    result->sign = 1;
    if (exponent != 0) {
        err = cbat_bigint_copy(value, &base);
        if (err != CBAT_OK)
            goto done;
    }
    while (exponent != 0) {
        if (exponent & 1u) {
            err = cbat_bigint_multiply(result, base, &next);
            if (err != CBAT_OK)
                goto done;
            cbat_bigint_free(result);
            result = next;
            next = NULL;
        }
        exponent >>= 1;
        if (exponent != 0) {
            err = cbat_bigint_multiply(base, base, &next);
            if (err != CBAT_OK)
                goto done;
            cbat_bigint_free(base);
            base = next;
            next = NULL;
        }
    }
    *out = result;
    result = NULL;
done:
    cbat_bigint_free(result);
    cbat_bigint_free(base);
    cbat_bigint_free(next);
    return err;
}
