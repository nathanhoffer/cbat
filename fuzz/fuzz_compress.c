#include "fuzz_common.h"
#include "cbat/compress.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    CBatCompressOptions options = {65536};
    CBatBytes *out = NULL;
    CBatList *entries = NULL;
    size_t i;
    if (cbat_inflate_with(data, size, &options, &out) == CBAT_OK) {
        CBAT_FUZZ_CHECK(out->length <= options.max_output_size);
        cbat_bytes_free(out);
    }
    out = NULL;
    if (cbat_gzip_decompress_with(data, size, &options, &out) == CBAT_OK) {
        CBAT_FUZZ_CHECK(out->length <= options.max_output_size);
        cbat_bytes_free(out);
    }
    if (cbat_zip_list(data, size, &entries) == CBAT_OK) {
        for (i = 0; i < entries->length && i < 8; i++) {
            out = NULL;
            if (cbat_zip_read_entry_with(data, size, i, &options, &out) == CBAT_OK) {
                const CBatZipEntry *entry = (const CBatZipEntry *)cbat_list_get(entries, i);
                CBAT_FUZZ_CHECK(out->length == entry->uncompressed_size);
                cbat_bytes_free(out);
            }
        }
        cbat_list_free(entries);
    }
    if (size <= 4096) {
        CBatBytes *encoded = NULL;
        if (cbat_deflate(data, size, &encoded) == CBAT_OK) {
            out = NULL;
            CBAT_FUZZ_CHECK(cbat_inflate_with(encoded->data, encoded->length, &options, &out) == CBAT_OK);
            CBAT_FUZZ_CHECK(out->length == size);
            CBAT_FUZZ_CHECK(size == 0 || memcmp(data, out->data, size) == 0);
            cbat_bytes_free(out);
            cbat_bytes_free(encoded);
        }
    }
    return 0;
}
