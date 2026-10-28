/*
 * Recipe 21: Cleaning and slicing text
 *
 * PROBLEM
 *   Split mixed line endings, separate a setting at its first '=', and
 *   manipulate Unicode text without cutting a multibyte character in half.
 *
 * SOLUTION
 *   Use split_lines and partition for record structure, strip_chars for
 *   explicit byte trimming, then UTF-8 substring/reverse for code points.
 *
 * DISCUSSION
 *   Byte-oriented operations are useful for ASCII syntax. UTF-8 operations
 *   validate the whole input and count code points, not display glyphs or
 *   grapheme clusters. Natural comparison handles arbitrarily long digit
 *   runs, so file9 sorts before file10 without integer conversion.
 */
#include <stdio.h>
#include "cbat/cbat.h"

int main(void) {
    CBatList *lines = NULL;
    CBatString *key = NULL, *value = NULL, *slice = NULL, *reversed = NULL;
    CBatError err = cbat_string_split_lines("name = Ada\r\nversion = 12\n", &lines);
    size_t i;
    if (err != CBAT_OK) goto cleanup;
    for (i = 0; i < cbat_list_length(lines); i++) {
        const char *line = cbat_string_cstr(CBAT_LIST_GET(lines, CBatString *, i));
        err = cbat_string_partition(line, "=", &key, &value);
        if (err != CBAT_OK) goto cleanup;
        cbat_string_strip_chars(key, " \t");
        cbat_string_strip_chars(value, " \t");
        printf("%s: %s\n", key->data, value->data);
        cbat_string_free(key); cbat_string_free(value); key = value = NULL;
    }
    err = cbat_utf8_substring("A\xC3\xA9\xF0\x9F\x98\x80", 1, 2, &slice);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_utf8_reverse(slice->data, &reversed);
    if (err != CBAT_OK) goto cleanup;
    printf("Slice: %s\nReversed: %s\n", slice->data, reversed->data);
    printf("file9 before file10: %s\n", cbat_compare_natural("file9", "file10") < 0 ? "yes" : "no");
cleanup:
    if (err != CBAT_OK) fprintf(stderr, "text: %s\n", cbat_error_str(err));
    cbat_string_free(reversed); cbat_string_free(slice);
    cbat_string_free(value); cbat_string_free(key); cbat_list_free(lines);
    return err == CBAT_OK ? 0 : 1;
}
