#include <float.h>
#include <math.h>
#include <string.h>
#include "cbat/bytes.h"
#include "cbat/fs.h"
#include "cbat/path.h"
#include "cbat/test.h"

CBAT_TEST(test_buffer_and_alias_growth) {
    uint8_t data[64];
    CBatBytes *bytes;
    size_t i;
    for (i = 0; i < sizeof(data); i++) data[i] = (uint8_t)i;
    bytes = cbat_bytes_from(data, sizeof(data));
    CBAT_REQUIRE_NOT_NULL(bytes);
    CBAT_CHECK_EQ_SIZE(bytes->length, 64);
    CBAT_REQUIRE_OK(cbat_bytes_append(bytes, bytes->data, bytes->length));
    CBAT_CHECK_EQ_SIZE(bytes->length, 128);
    CBAT_CHECK_EQ_MEM(bytes->data, data, 64);
    CBAT_CHECK_EQ_MEM(bytes->data + 64, data, 64);
    CBAT_REQUIRE_OK(cbat_bytes_append(bytes, NULL, 0));
    CBAT_CHECK_ERR(cbat_bytes_append(bytes, NULL, 1), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_bytes_append(bytes, data, SIZE_MAX), CBAT_ERR_OVERFLOW);
    CBAT_CHECK_EQ_SIZE(bytes->length, 128);
    cbat_bytes_free(bytes);
    bytes = cbat_bytes_from(data, 3); CBAT_REQUIRE_NOT_NULL(bytes);
    CBAT_CHECK_ERR(cbat_bytes_append(bytes, bytes->data + 2, 2), CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_EQ_SIZE(bytes->length, 3);
    cbat_bytes_free(bytes);
    bytes = cbat_bytes_from(NULL, 0); CBAT_REQUIRE_NOT_NULL(bytes);
    CBAT_CHECK_EQ_SIZE(bytes->length, 0); CBAT_CHECK_NULL(bytes->data);
    cbat_bytes_free(bytes); cbat_bytes_free(NULL);
    CBAT_CHECK_NULL(cbat_bytes_from(NULL, 1));
    CBAT_CHECK_ERR(cbat_bytes_append(NULL, data, 1), CBAT_ERR_INVALID_ARGUMENT);
}

CBAT_TEST(test_integer_wire_order_and_readback) {
    const uint8_t expected[] = {0xab, 0x34,0x12, 0x56,0x78, 0x44,0x33,0x22,0x11,
        0x55,0x66,0x77,0x88, 8,7,6,5,4,3,2,1, 0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80};
    CBatBytes *bytes = cbat_bytes_new();
    CBatByteReader reader;
    uint8_t a; uint16_t b, c; uint32_t d, e; uint64_t f, g;
    CBAT_REQUIRE_NOT_NULL(bytes);
    CBAT_REQUIRE_OK(cbat_bytes_append_u8(bytes, 0xab));
    CBAT_REQUIRE_OK(cbat_bytes_append_u16_le(bytes, 0x1234));
    CBAT_REQUIRE_OK(cbat_bytes_append_u16_be(bytes, 0x5678));
    CBAT_REQUIRE_OK(cbat_bytes_append_u32_le(bytes, UINT32_C(0x11223344)));
    CBAT_REQUIRE_OK(cbat_bytes_append_u32_be(bytes, UINT32_C(0x55667788)));
    CBAT_REQUIRE_OK(cbat_bytes_append_u64_le(bytes, UINT64_C(0x0102030405060708)));
    CBAT_REQUIRE_OK(cbat_bytes_append_u64_be(bytes, UINT64_C(0x1020304050607080)));
    CBAT_CHECK_EQ_SIZE(bytes->length, sizeof(expected));
    CBAT_CHECK_EQ_MEM(bytes->data, expected, sizeof(expected));
    cbat_byte_reader_init(&reader, bytes->data, bytes->length);
    CBAT_CHECK_EQ_SIZE(cbat_byte_reader_remaining(&reader), sizeof(expected));
    CBAT_REQUIRE_OK(cbat_byte_reader_read_u8(&reader, &a)); CBAT_CHECK_EQ_UINT(a, 0xab);
    CBAT_REQUIRE_OK(cbat_byte_reader_read_u16_le(&reader, &b)); CBAT_CHECK_EQ_UINT(b, 0x1234);
    CBAT_REQUIRE_OK(cbat_byte_reader_read_u16_be(&reader, &c)); CBAT_CHECK_EQ_UINT(c, 0x5678);
    CBAT_REQUIRE_OK(cbat_byte_reader_read_u32_le(&reader, &d)); CBAT_CHECK_EQ_UINT(d, UINT32_C(0x11223344));
    CBAT_REQUIRE_OK(cbat_byte_reader_read_u32_be(&reader, &e)); CBAT_CHECK_EQ_UINT(e, UINT32_C(0x55667788));
    CBAT_REQUIRE_OK(cbat_byte_reader_read_u64_le(&reader, &f)); CBAT_CHECK_EQ_UINT(f, UINT64_C(0x0102030405060708));
    CBAT_REQUIRE_OK(cbat_byte_reader_read_u64_be(&reader, &g)); CBAT_CHECK_EQ_UINT(g, UINT64_C(0x1020304050607080));
    CBAT_CHECK_EQ_SIZE(cbat_byte_reader_remaining(&reader), 0);
    cbat_bytes_free(bytes);
}

CBAT_TEST(test_float_wire_order_and_special_values) {
    const uint8_t expected[] = {0,0,0x80,0x3f, 0x3f,0x80,0,0,
        0,0,0,0,0,0,0,0x80, 0x80,0,0,0,0,0,0,0};
    const double values[] = {DBL_MAX, DBL_MIN, 4.9406564584124654e-324, -1.25, HUGE_VAL, NAN};
    CBatBytes *bytes = cbat_bytes_new();
    CBatByteReader reader;
    float a, b;
    double c, d;
    size_t i;
    CBAT_REQUIRE_NOT_NULL(bytes);
    CBAT_REQUIRE_OK(cbat_bytes_append_f32_le(bytes, 1.0f));
    CBAT_REQUIRE_OK(cbat_bytes_append_f32_be(bytes, 1.0f));
    CBAT_REQUIRE_OK(cbat_bytes_append_f64_le(bytes, -0.0));
    CBAT_REQUIRE_OK(cbat_bytes_append_f64_be(bytes, -0.0));
    CBAT_CHECK_EQ_SIZE(bytes->length, sizeof(expected));
    CBAT_CHECK_EQ_MEM(bytes->data, expected, sizeof(expected));
    cbat_byte_reader_init(&reader, bytes->data, bytes->length);
    CBAT_REQUIRE_OK(cbat_byte_reader_read_f32_le(&reader, &a)); CBAT_CHECK(a == 1.0f);
    CBAT_REQUIRE_OK(cbat_byte_reader_read_f32_be(&reader, &b)); CBAT_CHECK(b == 1.0f);
    CBAT_REQUIRE_OK(cbat_byte_reader_read_f64_le(&reader, &c)); CBAT_CHECK(c == 0 && signbit(c));
    CBAT_REQUIRE_OK(cbat_byte_reader_read_f64_be(&reader, &d)); CBAT_CHECK(d == 0 && signbit(d));
    for (i = 0; i < CBAT_ARRAY_LEN(values); i++) {
        size_t position = bytes->length;
        CBAT_REQUIRE_OK(cbat_bytes_append_f64_le(bytes, values[i]));
        CBAT_REQUIRE_OK(cbat_bytes_append_f64_be(bytes, values[i]));
        cbat_byte_reader_init(&reader, bytes->data + position, 16);
        CBAT_REQUIRE_OK(cbat_byte_reader_read_f64_le(&reader, &c));
        CBAT_REQUIRE_OK(cbat_byte_reader_read_f64_be(&reader, &d));
        if (isnan(values[i])) CBAT_CHECK(isnan(c) && isnan(d));
        else { CBAT_CHECK_EQ_MEM(&c, &values[i], 8); CBAT_CHECK_EQ_MEM(&d, &values[i], 8); }
    }
    cbat_bytes_free(bytes);
}

#define CBAT__CHECK_READER_FAILURES(function, type) do { \
    type value = (type)7; \
    CBAT_CHECK_ERR(function(&reader, &value), CBAT_ERR_END); \
    CBAT_CHECK(value == (type)7); CBAT_CHECK_EQ_SIZE(reader.position, 0); \
    CBAT_CHECK_ERR(function(&reader, NULL), CBAT_ERR_INVALID_ARGUMENT); \
    CBAT_CHECK_ERR(function(NULL, &value), CBAT_ERR_INVALID_ARGUMENT); \
} while (0)

CBAT_TEST(test_cursor_errors_leave_state_unchanged) {
    CBatByteReader reader;
    uint8_t data[] = {1,2,3}, out[] = {9,9,9};
    cbat_byte_reader_init(&reader, NULL, 0);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_u8, uint8_t);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_u16_le, uint16_t);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_u16_be, uint16_t);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_u32_le, uint32_t);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_u32_be, uint32_t);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_u64_le, uint64_t);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_u64_be, uint64_t);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_f32_le, float);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_f32_be, float);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_f64_le, double);
    CBAT__CHECK_READER_FAILURES(cbat_byte_reader_read_f64_be, double);
    cbat_byte_reader_init(&reader, data, sizeof(data));
    CBAT_CHECK_ERR(cbat_byte_reader_read(&reader, out, 4), CBAT_ERR_END);
    CBAT_CHECK_EQ_MEM(out, "\x09\x09\x09", 3); CBAT_CHECK_EQ_SIZE(reader.position, 0);
    CBAT_CHECK_ERR(cbat_byte_reader_skip(&reader, SIZE_MAX), CBAT_ERR_END);
    CBAT_REQUIRE_OK(cbat_byte_reader_skip(&reader, 1));
    CBAT_REQUIRE_OK(cbat_byte_reader_read(&reader, out, 2));
    CBAT_CHECK_EQ_INT(out[0], 2); CBAT_CHECK_EQ_INT(out[1], 3);
    CBAT_REQUIRE_OK(cbat_byte_reader_read(&reader, NULL, 0));
    CBAT_CHECK_ERR(cbat_byte_reader_read(&reader, NULL, 1), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_byte_reader_skip(NULL, 0), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_byte_reader_read(NULL, out, 1), CBAT_ERR_INVALID_ARGUMENT);
    cbat_byte_reader_init(&reader, NULL, 1);
    CBAT_CHECK_ERR(cbat_byte_reader_skip(&reader, 0), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_EQ_SIZE(cbat_byte_reader_remaining(&reader), 0);
    cbat_byte_reader_init(&reader, data, sizeof(data)); reader.position = 4;
    CBAT_CHECK_ERR(cbat_byte_reader_skip(&reader, 0), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_EQ_SIZE(cbat_byte_reader_remaining(&reader), 0);
    CBAT_CHECK_EQ_SIZE(cbat_byte_reader_remaining(NULL), 0);
    cbat_byte_reader_init(NULL, data, sizeof(data));
}
#undef CBAT__CHECK_READER_FAILURES

CBAT_TEST(test_allocations_and_custom_allocator) {
    size_t allowed;
    bool succeeded = false;
    const uint8_t data[] = {0,1,2,3};
    CBatArena *arena = cbat_arena_new(256);
    CBatBytes *bytes;
    CBAT_REQUIRE_NOT_NULL(arena);
    bytes = cbat_bytes_new_with(cbat_arena_allocator(arena)); CBAT_REQUIRE_NOT_NULL(bytes);
    CBAT_REQUIRE_OK(cbat_bytes_append(bytes, data, sizeof(data)));
    cbat_bytes_free(bytes); cbat_arena_free(arena);
    for (allowed = 0; allowed < 8; allowed++) {
        cbat_test_fail_allocations_after(allowed);
        bytes = cbat_bytes_from(data, sizeof(data));
        cbat_test_stop_failing_allocations();
        if (bytes != NULL) { succeeded = true; CBAT_CHECK_EQ_MEM(bytes->data, data, sizeof(data)); }
        cbat_bytes_free(bytes);
        CBAT_CHECK_EQ_SIZE(cbat_test_live_allocations(), 0);
        if (succeeded) break;
    }
    CBAT_CHECK(succeeded);
    bytes = cbat_bytes_new(); CBAT_REQUIRE_NOT_NULL(bytes);
    cbat_test_fail_allocations_after(0);
    CBAT_CHECK_ERR(cbat_bytes_append_u8(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_u16_le(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_u16_be(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_u32_le(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_u32_be(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_u64_le(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_u64_be(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_f32_le(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_f32_be(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_f64_le(bytes, 1), CBAT_ERR_NO_MEMORY);
    CBAT_CHECK_ERR(cbat_bytes_append_f64_be(bytes, 1), CBAT_ERR_NO_MEMORY);
    cbat_test_stop_failing_allocations();
    CBAT_CHECK_EQ_SIZE(bytes->length, 0); CBAT_CHECK_NULL(bytes->data);
    cbat_bytes_free(bytes);
}

CBAT_TEST(test_files_and_oom) {
    const uint8_t data[] = {0,1,0xff,0,0x80};
    CBatString *directory = NULL, *path;
    CBatBytes *original = cbat_bytes_from(data, sizeof(data)), *copy = NULL;
    size_t allowed;
    bool succeeded = false;
    CBAT_REQUIRE_NOT_NULL(original);
    CBAT_REQUIRE_OK(cbat_fs_make_temp_dir("cbat-bytes-", &directory));
    path = cbat_path_join(directory->data, "binary.bin"); CBAT_REQUIRE_NOT_NULL(path);
    CBAT_REQUIRE_OK(cbat_file_write_bytes(path->data, original));
    CBAT_REQUIRE_OK(cbat_file_read_bytes(path->data, &copy));
    CBAT_CHECK_EQ_SIZE(copy->length, sizeof(data)); CBAT_CHECK_EQ_MEM(copy->data, data, sizeof(data));
    cbat_bytes_free(copy); copy = NULL;
    CBAT_CHECK_ERR(cbat_file_read_bytes(NULL, &copy), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_file_read_bytes(path->data, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_file_read_bytes("no/such/file.bin", &copy), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_ERR(cbat_file_write_bytes(NULL, original), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_file_write_bytes(path->data, NULL), CBAT_ERR_INVALID_ARGUMENT);
    for (allowed = 0; allowed < 12; allowed++) {
        CBatError err;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_file_read_bytes(path->data, &copy);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) { succeeded = true; CBAT_CHECK_EQ_MEM(copy->data, data, sizeof(data)); }
        else { CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(copy); }
        cbat_bytes_free(copy); copy = NULL;
        if (succeeded) break;
    }
    CBAT_CHECK(succeeded);
    CBAT_REQUIRE_OK(cbat_fs_remove_tree(directory->data));
    cbat_string_free(path); cbat_string_free(directory); cbat_bytes_free(original);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_buffer_and_alias_growth),
    CBAT_TEST_ENTRY(test_integer_wire_order_and_readback),
    CBAT_TEST_ENTRY(test_float_wire_order_and_special_values),
    CBAT_TEST_ENTRY(test_cursor_errors_leave_state_unchanged),
    CBAT_TEST_ENTRY(test_allocations_and_custom_allocator),
    CBAT_TEST_ENTRY(test_files_and_oom)
};
CBAT_TEST_MAIN("bytes", tests)
