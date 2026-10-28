/*
 * Recipe 20: Portable numeric text
 *
 * PROBLEM
 *   Parse machine-readable decimal numbers and produce round-tripping text
 *   without changing the process locale or losing large unsigned integers.
 *
 * SOLUTION
 *   Use cbat_string_to_double/uint64 for input, cbat_format_double for
 *   compact exact output, and grouped int64 formatting for human reports.
 *
 * DISCUSSION
 *   Decimal input always uses '.', including when LC_NUMERIC uses ','.
 *   Commas in input are rejected. Double formatting preserves signed zero
 *   and subnormals; JSON uses the same formatting (and rejects inf/nan).
 *   uint64 parsing keeps all 64 bits rather than going through a double.
 */
#include <stdio.h>
#include "cbat/cbat.h"

int main(void) {
    CBatString *report = cbat_string_new();
    CBatError err = CBAT_ERR_NO_MEMORY;
    double value;
    uint64_t identifier;
    char number[32];
    if (report == NULL) return 1;
    err = cbat_string_to_double("1.25e4", &value);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_string_to_uint64("18446744073709551615", &identifier);
    if (err != CBAT_OK) goto cleanup;
    cbat_format_double(value, number);
    printf("Value: %s\nIdentifier: %llu\n", number, (unsigned long long)identifier);
    err = cbat_string_append(report, "Population: ");
    if (err != CBAT_OK) goto cleanup;
    err = cbat_string_append_int64_grouped(report, INT64_C(1234567890), ',');
    if (err == CBAT_OK) puts(report->data);
cleanup:
    if (err != CBAT_OK) fprintf(stderr, "numbers: %s\n", cbat_error_str(err));
    cbat_string_free(report);
    return err == CBAT_OK ? 0 : 1;
}
