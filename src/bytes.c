/* Binary buffers and cursors. Part of cbat. MIT License. */
#include "cbat/bytes.h"
#include "cbat/file.h"
#include <float.h>
#include <string.h>

CBatBytes *cbat_bytes_new_with(const CBatAllocator *allocator) {
    CBatBytes *bytes;
    if (allocator == NULL) allocator = cbat_default_allocator();
    bytes = (CBatBytes *)cbat_alloc_zeroed(allocator, sizeof(CBatBytes));
    if (bytes != NULL) bytes->allocator = allocator;
    return bytes;
}

CBatBytes *cbat_bytes_new(void) { return cbat_bytes_new_with(NULL); }

void cbat_bytes_free(CBatBytes *bytes) {
    if (bytes == NULL) return;
    cbat_release(bytes->allocator, bytes->data);
    cbat_release(bytes->allocator, bytes);
}

CBatError cbat_bytes_append(CBatBytes *bytes, const void *data, size_t length) {
    size_t needed, capacity, offset = 0;
    bool internal = false;
    if (bytes == NULL || (data == NULL && length != 0) || bytes->length > bytes->capacity ||
        (bytes->data == NULL && bytes->capacity != 0))
        return CBAT_ERR_INVALID_ARGUMENT;
    if (length == 0) return CBAT_OK;
    if (!cbat_size_add(bytes->length, length, &needed)) return CBAT_ERR_OVERFLOW;
    if (bytes->data != NULL && (uintptr_t)data >= (uintptr_t)bytes->data &&
        (uintptr_t)data - (uintptr_t)bytes->data < bytes->capacity) {
        offset = (size_t)((uintptr_t)data - (uintptr_t)bytes->data);
        if (offset > bytes->length || length > bytes->length - offset) return CBAT_ERR_OUT_OF_RANGE;
        internal = true;
    }
    capacity = bytes->capacity;
    if (needed > capacity) {
        void *fresh;
        if (capacity == 0) capacity = 64;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2) { capacity = needed; break; }
            capacity *= 2;
        }
        fresh = cbat_resize(bytes->allocator, bytes->data, bytes->capacity, capacity);
        if (fresh == NULL) return CBAT_ERR_NO_MEMORY;
        bytes->data = (uint8_t *)fresh;
        bytes->capacity = capacity;
    }
    if (internal) data = bytes->data + offset;
    memmove(bytes->data + bytes->length, data, length);
    bytes->length = needed;
    return CBAT_OK;
}

CBatBytes *cbat_bytes_from(const void *data, size_t length) {
    CBatBytes *bytes;
    if (data == NULL && length != 0) return NULL;
    bytes = cbat_bytes_new();
    if (bytes == NULL) return NULL;
    if (cbat_bytes_append(bytes, data, length) != CBAT_OK) { cbat_bytes_free(bytes); return NULL; }
    return bytes;
}

static CBatError cbat__bytes_write_integer(CBatBytes *bytes, uint64_t value, size_t count, bool big) {
    uint8_t encoded[8];
    size_t i;
    for (i = 0; i < count; i++) encoded[big ? count - i - 1 : i] = (uint8_t)(value >> (i * 8));
    return cbat_bytes_append(bytes, encoded, count);
}

CBatError cbat_bytes_append_u8(CBatBytes *bytes, uint8_t value) { return cbat_bytes_append(bytes, &value, 1); }
#define CBAT__BYTES_INTEGER_WRITERS(bits, type, count) \
    CBatError cbat_bytes_append_u##bits##_le(CBatBytes *bytes, type value) { \
        return cbat__bytes_write_integer(bytes, value, count, false); } \
    CBatError cbat_bytes_append_u##bits##_be(CBatBytes *bytes, type value) { \
        return cbat__bytes_write_integer(bytes, value, count, true); }
CBAT__BYTES_INTEGER_WRITERS(16, uint16_t, 2)
CBAT__BYTES_INTEGER_WRITERS(32, uint32_t, 4)
CBAT__BYTES_INTEGER_WRITERS(64, uint64_t, 8)
#undef CBAT__BYTES_INTEGER_WRITERS

static bool cbat__bytes_float32_supported(void) { return sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24; }
static bool cbat__bytes_float64_supported(void) { return sizeof(double) == 8 && FLT_RADIX == 2 && DBL_MANT_DIG == 53; }

#define CBAT__BYTES_FLOAT_WRITERS(bits, type, integer_type, supported, count) \
    CBatError cbat_bytes_append_f##bits##_le(CBatBytes *bytes, type value) { \
        integer_type encoded; \
        if (!supported()) return CBAT_ERR_UNSUPPORTED; \
        memcpy(&encoded, &value, sizeof(encoded)); \
        return cbat__bytes_write_integer(bytes, encoded, count, false); } \
    CBatError cbat_bytes_append_f##bits##_be(CBatBytes *bytes, type value) { \
        integer_type encoded; \
        if (!supported()) return CBAT_ERR_UNSUPPORTED; \
        memcpy(&encoded, &value, sizeof(encoded)); \
        return cbat__bytes_write_integer(bytes, encoded, count, true); }
CBAT__BYTES_FLOAT_WRITERS(32, float, uint32_t, cbat__bytes_float32_supported, 4)
CBAT__BYTES_FLOAT_WRITERS(64, double, uint64_t, cbat__bytes_float64_supported, 8)
#undef CBAT__BYTES_FLOAT_WRITERS

void cbat_byte_reader_init(CBatByteReader *reader, const void *data, size_t length) {
    if (reader == NULL) return;
    reader->data = (const uint8_t *)data; reader->length = length; reader->position = 0;
}

static bool cbat__byte_reader_valid(const CBatByteReader *reader) {
    return reader != NULL && reader->position <= reader->length &&
        (reader->data != NULL || reader->length == 0);
}

size_t cbat_byte_reader_remaining(const CBatByteReader *reader) {
    return cbat__byte_reader_valid(reader) ? reader->length - reader->position : 0;
}

CBatError cbat_byte_reader_skip(CBatByteReader *reader, size_t count) {
    if (!cbat__byte_reader_valid(reader)) return CBAT_ERR_INVALID_ARGUMENT;
    if (count > reader->length - reader->position) return CBAT_ERR_END;
    reader->position += count;
    return CBAT_OK;
}

CBatError cbat_byte_reader_read(CBatByteReader *reader, void *out, size_t length) {
    if (!cbat__byte_reader_valid(reader) || (out == NULL && length != 0)) return CBAT_ERR_INVALID_ARGUMENT;
    if (length > reader->length - reader->position) return CBAT_ERR_END;
    if (length != 0) memmove(out, reader->data + reader->position, length);
    reader->position += length;
    return CBAT_OK;
}

static CBatError cbat__bytes_read_integer(CBatByteReader *reader, uint64_t *out, size_t count, bool big) {
    uint64_t value = 0;
    size_t i;
    if (!cbat__byte_reader_valid(reader)) return CBAT_ERR_INVALID_ARGUMENT;
    if (count > reader->length - reader->position) return CBAT_ERR_END;
    for (i = 0; i < count; i++) {
        size_t index = big ? count - i - 1 : i;
        value |= (uint64_t)reader->data[reader->position + index] << (8 * i);
    }
    *out = value;
    reader->position += count;
    return CBAT_OK;
}

CBatError cbat_byte_reader_read_u8(CBatByteReader *reader, uint8_t *out) { return out == NULL ? CBAT_ERR_INVALID_ARGUMENT : cbat_byte_reader_read(reader, out, 1); }
#define CBAT__BYTES_INTEGER_READERS(bits, type, count) \
    CBatError cbat_byte_reader_read_u##bits##_le(CBatByteReader *reader, type *out) { \
        uint64_t value; CBatError err; \
        if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT; \
        err = cbat__bytes_read_integer(reader, &value, count, false); \
        if (err == CBAT_OK) { *out = (type)value; } return err; } \
    CBatError cbat_byte_reader_read_u##bits##_be(CBatByteReader *reader, type *out) { \
        uint64_t value; CBatError err; \
        if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT; \
        err = cbat__bytes_read_integer(reader, &value, count, true); \
        if (err == CBAT_OK) { *out = (type)value; } return err; }
CBAT__BYTES_INTEGER_READERS(16, uint16_t, 2)
CBAT__BYTES_INTEGER_READERS(32, uint32_t, 4)
CBAT__BYTES_INTEGER_READERS(64, uint64_t, 8)
#undef CBAT__BYTES_INTEGER_READERS

#define CBAT__BYTES_FLOAT_READERS(bits, type, integer_type, supported) \
    CBatError cbat_byte_reader_read_f##bits##_le(CBatByteReader *reader, type *out) { \
        integer_type value; CBatError err; \
        if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT; \
        if (!supported()) return CBAT_ERR_UNSUPPORTED; \
        err = cbat_byte_reader_read_u##bits##_le(reader, &value); \
        if (err == CBAT_OK) { memcpy(out, &value, sizeof(value)); } return err; } \
    CBatError cbat_byte_reader_read_f##bits##_be(CBatByteReader *reader, type *out) { \
        integer_type value; CBatError err; \
        if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT; \
        if (!supported()) return CBAT_ERR_UNSUPPORTED; \
        err = cbat_byte_reader_read_u##bits##_be(reader, &value); \
        if (err == CBAT_OK) { memcpy(out, &value, sizeof(value)); } return err; }
CBAT__BYTES_FLOAT_READERS(32, float, uint32_t, cbat__bytes_float32_supported)
CBAT__BYTES_FLOAT_READERS(64, double, uint64_t, cbat__bytes_float64_supported)
#undef CBAT__BYTES_FLOAT_READERS

CBatError cbat_file_read_bytes(const char *path, CBatBytes **out) {
    CBatString *text = NULL;
    CBatBytes *bytes;
    CBatError err;
    if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat_file_read(path, &text);
    if (err != CBAT_OK) return err;
    bytes = cbat_bytes_new_with(text->allocator);
    if (bytes == NULL) { cbat_string_free(text); return CBAT_ERR_NO_MEMORY; }
    bytes->length = text->length;
    bytes->capacity = text->capacity;
    bytes->data = (uint8_t *)cbat_string_steal(text);
    *out = bytes;
    return CBAT_OK;
}

CBatError cbat_file_write_bytes(const char *path, const CBatBytes *bytes) {
    if (bytes == NULL || bytes->length > bytes->capacity) return CBAT_ERR_INVALID_ARGUMENT;
    return cbat_file_write(path, bytes->data, bytes->length);
}
