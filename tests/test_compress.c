#include "cbat/compress.h"
#include "cbat/hash.h"
#include "cbat/test.h"
#include "compress_vectors.h"
#include <string.h>

CBAT_TEST(test_dynamic_and_lz_roundtrips) {
    CBatBytes *decoded = NULL, *raw = NULL, *gzip = NULL, *restored = NULL;
    unsigned char payload[8192];
    size_t i;
    for (i = 0; i < sizeof(payload); i++)
        payload[i] = (unsigned char)(i % 97 == 0 ? (i * 17) % 256 : 'a' + i % 7);
    CBAT_REQUIRE_OK(cbat_inflate(dynamic_deflate, sizeof(dynamic_deflate), &decoded));
    CBAT_CHECK_EQ_INT(decoded->length, sizeof(payload));
    CBAT_CHECK_EQ_MEM(decoded->data, payload, sizeof(payload));
    CBAT_REQUIRE_OK(cbat_deflate(payload, sizeof(payload), &raw));
    CBAT_CHECK(raw->length < sizeof(payload) / 2);
    CBAT_REQUIRE_OK(cbat_inflate(raw->data, raw->length, &restored));
    CBAT_CHECK_EQ_MEM(restored->data, payload, sizeof(payload));
    cbat_bytes_free(restored);
    restored = NULL;
    CBAT_REQUIRE_OK(cbat_gzip_compress(payload, sizeof(payload), &gzip));
    CBAT_REQUIRE_OK(cbat_gzip_decompress(gzip->data, gzip->length, &restored));
    CBAT_CHECK_EQ_MEM(restored->data, payload, sizeof(payload));
    cbat_bytes_free(decoded);
    cbat_bytes_free(raw);
    cbat_bytes_free(gzip);
    cbat_bytes_free(restored);
}
CBAT_TEST(test_zip32_zip64_and_descriptors) {
    const unsigned char *data[] = {normal_zip, stream_zip, zip64_archive};
    size_t sizes[] = {sizeof(normal_zip), sizeof(stream_zip), sizeof(zip64_archive)}, i, j;
    for (i = 0; i < 3; i++) {
        CBatList *entries = NULL;
        CBatBytes *bytes = NULL;
        CBAT_REQUIRE_OK(cbat_zip_list(data[i], sizes[i], &entries));
        CBAT_CHECK_EQ_INT(entries->length, 3);
        CBAT_CHECK_EQ_STR(((CBatZipEntry *)cbat_list_get(entries, 0))->name->data, "plain.bin");
        CBAT_CHECK(((CBatZipEntry *)cbat_list_get(entries, 2))->directory);
        CBAT_REQUIRE_OK(cbat_zip_read_entry(data[i], sizes[i], 0, &bytes));
        CBAT_CHECK_EQ_INT(bytes->length, 11);
        CBAT_CHECK_EQ_MEM(bytes->data, "plain\0bytes", 11);
        cbat_bytes_free(bytes);
        bytes = NULL;
        CBAT_REQUIRE_OK(cbat_zip_read_entry(data[i], sizes[i], 1, &bytes));
        CBAT_CHECK_EQ_INT(bytes->length, 6000);
        for (j = 0; j < bytes->length; j++)
            CBAT_CHECK_EQ_INT(bytes->data[j], "hello "[j % 6]);
        cbat_bytes_free(bytes);
        bytes = NULL;
        CBAT_REQUIRE_OK(cbat_zip_read_entry(data[i], sizes[i], 2, &bytes));
        CBAT_CHECK_EQ_INT(bytes->length, 0);
        cbat_bytes_free(bytes);
        cbat_list_free(entries);
    }
}
CBAT_TEST(test_limits_truncation_and_oom) {
    CBatBytes *out = NULL, *gzip = NULL;
    CBatCompressOptions options = {10};
    size_t n, op;
    CBAT_CHECK_ERR(cbat_inflate_with(dynamic_deflate, sizeof(dynamic_deflate), &options, &out),
                   CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_NULL(out);
    CBAT_CHECK_ERR(cbat_inflate(dynamic_deflate, sizeof(dynamic_deflate) - 1, &out),
                   CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_zip_read_entry_with(normal_zip, sizeof(normal_zip), 1, &options, &out),
                   CBAT_ERR_OUT_OF_RANGE);
    CBAT_REQUIRE_OK(cbat_gzip_compress("abcabcabc", 9, &gzip));
    gzip->data[gzip->length - 8] ^= 1;
    CBAT_CHECK_ERR(cbat_gzip_decompress(gzip->data, gzip->length, &out), CBAT_ERR_PARSE);
    gzip->data[gzip->length - 8] ^= 1;
    for (op = 0; op < 6; op++) {
        bool succeeded = false;
        for (n = 0; n < 100; n++) {
            CBatBytes *bytes = NULL;
            CBatList *entries = NULL;
            CBatError err;
            cbat_test_fail_allocations_after(n);
            switch (op) {
            case 0:
                err = cbat_deflate("abcabcabc", 9, &bytes);
                break;
            case 1:
                err = cbat_inflate(dynamic_deflate, sizeof(dynamic_deflate), &bytes);
                break;
            case 2:
                err = cbat_gzip_compress("abcabcabc", 9, &bytes);
                break;
            case 3:
                err = cbat_gzip_decompress(gzip->data, gzip->length, &bytes);
                break;
            case 4:
                err = cbat_zip_list(normal_zip, sizeof(normal_zip), &entries);
                break;
            default:
                err = cbat_zip_read_entry(normal_zip, sizeof(normal_zip), 1, &bytes);
                break;
            }
            cbat_test_stop_failing_allocations();
            if (err == CBAT_OK)
                succeeded = true;
            else {
                CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY);
                CBAT_CHECK_NULL(bytes);
                CBAT_CHECK_NULL(entries);
            }
            cbat_bytes_free(bytes);
            cbat_list_free(entries);
            if (succeeded)
                break;
        }
        CBAT_CHECK(succeeded);
    }
    cbat_bytes_free(gzip);
}
CBAT_TEST(test_stored_invalid_and_empty_limits) {
    static const unsigned char stored[] = {1, 3, 0, 252, 255, 'a', 'b', 'c'};
    static const unsigned char invalid_backref[] = {3, 2, 0};
    CBatBytes *out = NULL, *encoded = NULL;
    CBatCompressOptions options;
    unsigned char bad[sizeof(stored) + 1];
    cbat_compress_options_init(&options);
    cbat_compress_options_init(NULL);
    CBAT_CHECK_EQ_INT(options.max_output_size, CBAT_DECOMPRESS_DEFAULT_LIMIT);
    CBAT_REQUIRE_OK(cbat_inflate(stored, sizeof(stored), &out));
    CBAT_CHECK_EQ_MEM(out->data, "abc", 3);
    cbat_bytes_free(out);
    out = NULL;
    memcpy(bad, stored, sizeof(stored));
    bad[3] ^= 1;
    CBAT_CHECK_ERR(cbat_inflate(bad, sizeof(stored), &out), CBAT_ERR_PARSE);
    memcpy(bad, stored, sizeof(stored));
    bad[sizeof(stored)] = 0;
    CBAT_CHECK_ERR(cbat_inflate(bad, sizeof(bad), &out), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_inflate(invalid_backref, sizeof(invalid_backref), &out), CBAT_ERR_PARSE);
    bad[0] = 7;
    CBAT_CHECK_ERR(cbat_inflate(bad, 1, &out), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_inflate(NULL, 0, &out), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_inflate(NULL, 1, &out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_deflate(NULL, 1, &out), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_gzip_compress(NULL, 0, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_gzip_decompress(NULL, 0, &out), CBAT_ERR_PARSE);
    CBAT_REQUIRE_OK(cbat_deflate(NULL, 0, &encoded));
    options.max_output_size = 0;
    CBAT_REQUIRE_OK(cbat_inflate_with(encoded->data, encoded->length, &options, &out));
    CBAT_CHECK_EQ_INT(out->length, 0);
    cbat_bytes_free(out);
    cbat_bytes_free(encoded);
    out = NULL;
    encoded = NULL;
    CBAT_REQUIRE_OK(cbat_gzip_compress(NULL, 0, &encoded));
    CBAT_REQUIRE_OK(cbat_gzip_decompress_with(encoded->data, encoded->length, &options, &out));
    CBAT_CHECK_EQ_INT(out->length, 0);
    cbat_bytes_free(out);
    cbat_bytes_free(encoded);
}
CBAT_TEST(test_gzip_headers_concatenation_and_all_truncations) {
    unsigned char header[] = {31, 139, 8,   30,  0,   0,   0, 0,   0, 255,
                              3,  0,   'A', 'B', 'C', 'n', 0, 'c', 0};
    unsigned char zeros[3] = {0};
    CBatBytes *base = NULL, *custom = cbat_bytes_new(), *out = NULL, *joined = cbat_bytes_new();
    size_t n;
    CBAT_REQUIRE_NOT_NULL(custom);
    CBAT_REQUIRE_NOT_NULL(joined);
    CBAT_REQUIRE_OK(cbat_gzip_compress("abc", 3, &base));
    CBAT_REQUIRE_OK(cbat_bytes_append(custom, header, sizeof(header)));
    CBAT_REQUIRE_OK(cbat_bytes_append_u16_le(custom, (uint16_t)cbat_crc32(header, sizeof(header))));
    CBAT_REQUIRE_OK(cbat_bytes_append(custom, base->data + 10, base->length - 10));
    CBAT_REQUIRE_OK(cbat_gzip_decompress(custom->data, custom->length, &out));
    CBAT_CHECK_EQ_MEM(out->data, "abc", 3);
    cbat_bytes_free(out);
    out = NULL;
    custom->data[sizeof(header)] ^= 1;
    CBAT_CHECK_ERR(cbat_gzip_decompress(custom->data, custom->length, &out), CBAT_ERR_PARSE);
    custom->data[sizeof(header)] ^= 1;
    CBAT_REQUIRE_OK(cbat_bytes_append(joined, base->data, base->length));
    CBAT_REQUIRE_OK(cbat_bytes_append(joined, base->data, base->length));
    CBAT_REQUIRE_OK(cbat_bytes_append(joined, zeros, 3));
    CBAT_REQUIRE_OK(cbat_gzip_decompress(joined->data, joined->length, &out));
    CBAT_CHECK_EQ_MEM(out->data, "abcabc", 6);
    cbat_bytes_free(out);
    out = NULL;
    for (n = 0; n < custom->length; n++) {
        CBAT_CHECK_ERR(cbat_gzip_decompress(custom->data, n, &out), CBAT_ERR_PARSE);
        CBAT_CHECK_NULL(out);
    }
    for (n = 0; n < sizeof(dynamic_deflate); n++) {
        CBAT_CHECK_ERR(cbat_inflate(dynamic_deflate, n, &out), CBAT_ERR_PARSE);
        CBAT_CHECK_NULL(out);
    }
    cbat_bytes_free(base);
    cbat_bytes_free(custom);
    cbat_bytes_free(joined);
}
static uint32_t test_zip_u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void test_zip_put16(unsigned char *p, unsigned value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}
CBAT_TEST(test_zip_corruption_and_unsupported) {
    CBatBytes *archive = cbat_bytes_from(normal_zip, sizeof(normal_zip)), *out = NULL;
    CBatList *entries = NULL;
    size_t central, n;
    CBAT_REQUIRE_NOT_NULL(archive);
    central = test_zip_u32(normal_zip + sizeof(normal_zip) - 6);
    for (n = 0; n < sizeof(normal_zip); n++) {
        CBAT_CHECK(cbat_zip_list(normal_zip, n, &entries) != CBAT_OK);
        CBAT_CHECK_NULL(entries);
    }
    archive->data[30 + 9] ^= 1;
    CBAT_CHECK_ERR(cbat_zip_read_entry(archive->data, archive->length, 0, &out), CBAT_ERR_PARSE);
    archive->data[30 + 9] ^= 1;
    test_zip_put16(archive->data + 8, 12);
    test_zip_put16(archive->data + central + 10, 12);
    CBAT_REQUIRE_OK(cbat_zip_list(archive->data, archive->length, &entries));
    cbat_list_free(entries);
    entries = NULL;
    CBAT_CHECK_ERR(cbat_zip_read_entry(archive->data, archive->length, 0, &out),
                   CBAT_ERR_UNSUPPORTED);
    test_zip_put16(archive->data + 8, 0);
    test_zip_put16(archive->data + central + 10, 0);
    test_zip_put16(archive->data + 6, 1);
    test_zip_put16(archive->data + central + 8, 1);
    CBAT_CHECK_ERR(cbat_zip_read_entry(archive->data, archive->length, 0, &out),
                   CBAT_ERR_UNSUPPORTED);
    CBAT_CHECK_ERR(cbat_zip_read_entry(normal_zip, sizeof(normal_zip), 3, &out),
                   CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_ERR(cbat_zip_list(NULL, 0, &entries), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_zip_list(NULL, 1, &entries), CBAT_ERR_INVALID_ARGUMENT);
    cbat_bytes_free(archive);
}
static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_dynamic_and_lz_roundtrips),
    CBAT_TEST_ENTRY(test_zip32_zip64_and_descriptors),
    CBAT_TEST_ENTRY(test_limits_truncation_and_oom),
    CBAT_TEST_ENTRY(test_stored_invalid_and_empty_limits),
    CBAT_TEST_ENTRY(test_gzip_headers_concatenation_and_all_truncations),
    CBAT_TEST_ENTRY(test_zip_corruption_and_unsupported)};
CBAT_TEST_MAIN("compress", tests)
