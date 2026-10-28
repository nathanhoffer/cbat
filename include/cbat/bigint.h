/* cbat/bigint.h - signed arbitrary-precision integers. MIT License.
 * Optional CBAT_WITH_BIGINT (default ON); include explicitly.
 * Values own their limbs and retain their allocator. No fixed bit limit;
 * checked allocation sizes bound representable storage. Arithmetic creates
 * new values using the first operand's allocator; inputs remain unchanged.
 * All fallible APIs preserve output parameters on failure. Objects require
 * external synchronization for concurrent mutation/free.
 */
#ifndef CBAT_BIGINT_H
#define CBAT_BIGINT_H
#include "cbat/base.h"
#include "cbat/string.h"
CBAT_BEGIN_DECLS
typedef struct CBatBigInt CBatBigInt;
/** New zero; NULL on OOM. */
CBAT_API CBatBigInt *cbat_bigint_new(void);
/** New zero with retained allocator (NULL = default); allocator must outlive value/results. */
CBAT_API CBatBigInt *cbat_bigint_new_with(const CBatAllocator *allocator);
/** Frees value/limbs; NULL ignored. */
CBAT_API void cbat_bigint_free(CBatBigInt *value);
/** Parses decimal digits with optional sign and surrounding ASCII whitespace.
 * Empty input, internal whitespace, hex and underscores give PARSE. */
CBAT_API CBatError cbat_bigint_from_string(const char *text, CBatBigInt **out);
/** Same with retained allocator. */
CBAT_API CBatError cbat_bigint_from_string_with(const char *text, const CBatAllocator *allocator, CBatBigInt **out);
/** New exact signed 64-bit value. */
CBAT_API CBatError cbat_bigint_from_int64(int64_t value, CBatBigInt **out);
/** Deep copy using value's allocator. */
CBAT_API CBatError cbat_bigint_copy(const CBatBigInt *value, CBatBigInt **out);
/** New owned canonical decimal string (no plus/leading zeros, zero is "0"). */
CBAT_API CBatError cbat_bigint_to_string(const CBatBigInt *value, CBatString **out);
/** -1/0/+1 ordering; NULL is treated as zero. */
CBAT_API int cbat_bigint_compare(const CBatBigInt *a, const CBatBigInt *b);
/** New a+b. */
CBAT_API CBatError cbat_bigint_add(const CBatBigInt *a, const CBatBigInt *b, CBatBigInt **out);
/** New a-b. */
CBAT_API CBatError cbat_bigint_subtract(const CBatBigInt *a, const CBatBigInt *b, CBatBigInt **out);
/** New a*b (schoolbook multiplication). */
CBAT_API CBatError cbat_bigint_multiply(const CBatBigInt *a, const CBatBigInt *b, CBatBigInt **out);
/** Quotient truncates toward zero; remainder has dividend's sign and |r|<|b|.
 * Either output may be NULL, but not both; non-NULL outputs must be distinct.
 * Zero divisor gives INVALID_ARGUMENT. Both outputs publish atomically.
 * Multi-limb long division uses a bounded binary search per quotient limb. */
CBAT_API CBatError cbat_bigint_divmod(const CBatBigInt *a, const CBatBigInt *b,
    CBatBigInt **quotient, CBatBigInt **remainder);
/** New value^exponent via repeated squaring. 0^0 is 1. */
CBAT_API CBatError cbat_bigint_pow(const CBatBigInt *value, uint64_t exponent, CBatBigInt **out);
CBAT_END_DECLS
#endif
