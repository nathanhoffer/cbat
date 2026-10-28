/*
 * Recipe 19: Reading a BMP header
 *
 * PROBLEM
 *   Inspect a binary format with little-endian integers without relying on
 *   C struct padding, unaligned loads, or the machine's byte order.
 *
 * SOLUTION
 *   Wrap bytes in CBatBytes, then decode fields with a CBatByteReader.
 *   Every typed read checks bounds and advances only after success.
 *
 * DISCUSSION
 *   This sample is a 1x1, 24-bit uncompressed BMP with a red pixel. For an
 *   actual file, use cbat_file_read_bytes(path, &bytes) instead of bytes_from.
 *   A full BMP loader must also check compression, signed height, row padding
 *   and arithmetic overflow. The cursor borrows storage, so keep the buffer
 *   alive and do not append to it while reading.
 */
#include <stdio.h>
#include "cbat/cbat.h"

int main(void) {
    const uint8_t bmp[] = {
        'B','M', 58,0,0,0, 0,0,0,0, 54,0,0,0,
        40,0,0,0, 1,0,0,0, 1,0,0,0, 1,0, 24,0,
        0,0,0,0, 4,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,255,0
    };
    CBatBytes *bytes = cbat_bytes_from(bmp, sizeof(bmp));
    CBatByteReader reader;
    uint8_t magic[2];
    uint32_t file_size = 0, offset = 0, header_size = 0, width = 0, height = 0;
    uint16_t planes = 0, depth = 0;
    CBatError err;
    if (bytes == NULL) return 1;
    cbat_byte_reader_init(&reader, bytes->data, bytes->length);
    err = cbat_byte_reader_read(&reader, magic, 2);
    if (err != CBAT_OK || magic[0] != 'B' || magic[1] != 'M') { err = CBAT_ERR_PARSE; goto cleanup; }
    err = cbat_byte_reader_read_u32_le(&reader, &file_size);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_byte_reader_skip(&reader, 4); /* reserved */
    if (err != CBAT_OK) goto cleanup;
    err = cbat_byte_reader_read_u32_le(&reader, &offset);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_byte_reader_read_u32_le(&reader, &header_size);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_byte_reader_read_u32_le(&reader, &width);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_byte_reader_read_u32_le(&reader, &height);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_byte_reader_read_u16_le(&reader, &planes);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_byte_reader_read_u16_le(&reader, &depth);
    if (err != CBAT_OK) goto cleanup;
    if (file_size != bytes->length || offset != 54 || header_size != 40 ||
        width != 1 || height != 1 || planes != 1 || depth != 24) { err = CBAT_ERR_PARSE; goto cleanup; }
    printf("BMP: %lu x %lu, %u bits/pixel; pixel data at byte %lu\n",
        (unsigned long)width, (unsigned long)height, (unsigned)depth, (unsigned long)offset);
cleanup:
    if (err != CBAT_OK) fprintf(stderr, "BMP: %s\n", cbat_error_str(err));
    cbat_bytes_free(bytes);
    return err == CBAT_OK ? 0 : 1;
}
