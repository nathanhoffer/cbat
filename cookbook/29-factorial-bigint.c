/* Recipe 29: Exact factorial(100).
 * PROBLEM: Fixed-width integers overflow long before 100!.
 * SOLUTION: Multiply owned arbitrary-precision integers, replacing the old
 * result only after each operation succeeds.
 * DISCUSSION: Bigints are signed values with explicit allocators/ownership.
 * Division truncates toward zero; decimal conversion is exact, without floats.
 */
#include "cbat/bigint.h"
#include <stdio.h>

int main(void) {
    CBatBigInt *factorial = NULL, *factor = NULL, *next = NULL;
    CBatString *decimal = NULL;
    CBatError err = cbat_bigint_from_int64(1, &factorial);
    int64_t i;
    if (err != CBAT_OK)
        goto cleanup;
    for (i = 2; i <= 100; i++) {
        err = cbat_bigint_from_int64(i, &factor);
        if (err != CBAT_OK)
            goto cleanup;
        err = cbat_bigint_multiply(factorial, factor, &next);
        if (err != CBAT_OK)
            goto cleanup;
        cbat_bigint_free(factorial);
        factorial = next;
        next = NULL;
        cbat_bigint_free(factor);
        factor = NULL;
    }
    err = cbat_bigint_to_string(factorial, &decimal);
    if (err == CBAT_OK)
        printf("100! = %s\n%lu decimal digits\n", decimal->data, (unsigned long)decimal->length);
cleanup:
    if (err != CBAT_OK)
        fprintf(stderr, "%s\n", cbat_error_str(err));
    cbat_string_free(decimal);
    cbat_bigint_free(factorial);
    cbat_bigint_free(factor);
    cbat_bigint_free(next);
    return err == CBAT_OK ? 0 : 1;
}
