/* Recipe 30: Compressing a binary buffer and validating its round trip.
 * PROBLEM: Store repeated data compactly without an external dependency.
 * SOLUTION: Encode gzip, decode with an explicit output limit, compare bytes.
 * DISCUSSION: Buffers have lengths and can contain NULs. Gzip validates CRC
 * and size. Raw DEFLATE helpers omit the gzip wrapper; ZIP readers list/read
 * stored or deflated entries in memory, without extracting filesystem paths.
 */
#include "cbat/compress.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char record[] = "sensor=42,value=12.5\n";
    CBatBytes *input = cbat_bytes_new(), *compressed = NULL, *restored = NULL;
    CBatCompressOptions options;
    CBatError err = CBAT_OK;
    size_t i;
    if (input == NULL)
        return 1;
    for (i = 0; i < 2000; i++) {
        err = cbat_bytes_append(input, record, sizeof(record) - 1);
        if (err != CBAT_OK)
            goto cleanup;
    }
    err = cbat_gzip_compress(input->data, input->length, &compressed);
    if (err != CBAT_OK)
        goto cleanup;
    cbat_compress_options_init(&options);
    options.max_output_size = input->length;
    err = cbat_gzip_decompress_with(compressed->data, compressed->length, &options, &restored);
    if (err != CBAT_OK)
        goto cleanup;
    if (restored->length != input->length ||
        memcmp(restored->data, input->data, input->length) != 0) {
        err = CBAT_ERR_PARSE;
        goto cleanup;
    }
    printf("%lu bytes -> %lu gzip bytes; exact round trip\n", (unsigned long)input->length,
           (unsigned long)compressed->length);
cleanup:
    if (err != CBAT_OK)
        fprintf(stderr, "%s\n", cbat_error_str(err));
    cbat_bytes_free(input);
    cbat_bytes_free(compressed);
    cbat_bytes_free(restored);
    return err == CBAT_OK ? 0 : 1;
}
