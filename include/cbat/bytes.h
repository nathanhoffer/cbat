/*
 * cbat/bytes.h - growable binary buffers and endian-aware read cursors.
 * Bytes have an explicit length and no terminator requirement. Fields may
 * be read, but should only be modified through these functions. No internal
 * locking. Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_BYTES_H
#define CBAT_BYTES_H
#include "cbat/base.h"

CBAT_BEGIN_DECLS
typedef struct CBatBytes {
    uint8_t *data;           /* NULL until storage is first needed */
    size_t length;
    size_t capacity;
    const CBatAllocator *allocator;
} CBatBytes;

/** New empty buffer; NULL on OOM. */
CBAT_API CBatBytes *cbat_bytes_new(void);
/** Custom allocator (NULL = default), retained for the object's lifetime. */
CBAT_API CBatBytes *cbat_bytes_new_with(const CBatAllocator *allocator);
/** Copies bytes; NULL for invalid NULL/nonempty input or allocation failure. */
CBAT_API CBatBytes *cbat_bytes_from(const void *data, size_t length);
/** Releases buffer and wrapper. NULL is ignored. */
CBAT_API void cbat_bytes_free(CBatBytes *bytes);
/**
 * Appends length bytes, including self-appends. NULL data is allowed for
 * length 0. NO_MEMORY/OVERFLOW leaves the buffer unchanged; internal input
 * must lie within initialized bytes (otherwise OUT_OF_RANGE).
 */
CBAT_API CBatError cbat_bytes_append(CBatBytes *bytes, const void *data, size_t length);
/** Append one unsigned byte. */
CBAT_API CBatError cbat_bytes_append_u8(CBatBytes *bytes, uint8_t value);
/** Append unsigned integers in explicit little/big-endian byte order. */
CBAT_API CBatError cbat_bytes_append_u16_le(CBatBytes *bytes, uint16_t value);
CBAT_API CBatError cbat_bytes_append_u16_be(CBatBytes *bytes, uint16_t value);
CBAT_API CBatError cbat_bytes_append_u32_le(CBatBytes *bytes, uint32_t value);
CBAT_API CBatError cbat_bytes_append_u32_be(CBatBytes *bytes, uint32_t value);
CBAT_API CBatError cbat_bytes_append_u64_le(CBatBytes *bytes, uint64_t value);
CBAT_API CBatError cbat_bytes_append_u64_be(CBatBytes *bytes, uint64_t value);
/** IEEE-754 binary32/64 bit representations; UNSUPPORTED on incompatible hosts. */
CBAT_API CBatError cbat_bytes_append_f32_le(CBatBytes *bytes, float value);
CBAT_API CBatError cbat_bytes_append_f32_be(CBatBytes *bytes, float value);
CBAT_API CBatError cbat_bytes_append_f64_le(CBatBytes *bytes, double value);
CBAT_API CBatError cbat_bytes_append_f64_be(CBatBytes *bytes, double value);

/** Cursor over borrowed memory. Keep the memory alive and unchanged while reading. */
typedef struct CBatByteReader {
    const uint8_t *data;
    size_t length;
    size_t position;
} CBatByteReader;

/** Initialize/reset; NULL reader is ignored. NULL data with nonzero length gives an invalid cursor. */
CBAT_API void cbat_byte_reader_init(CBatByteReader *reader, const void *data, size_t length);
/** Remaining bytes, or 0 for NULL/invalid cursor. */
CBAT_API size_t cbat_byte_reader_remaining(const CBatByteReader *reader);
/** Advances count bytes; END or INVALID_ARGUMENT leaves position unchanged. */
CBAT_API CBatError cbat_byte_reader_skip(CBatByteReader *reader, size_t count);
/** Copies length bytes; NULL out allowed for length 0. On error, output/cursor unchanged. */
CBAT_API CBatError cbat_byte_reader_read(CBatByteReader *reader, void *out, size_t length);
/** Typed reads: NULL arguments are invalid; END leaves output and cursor unchanged. */
CBAT_API CBatError cbat_byte_reader_read_u8(CBatByteReader *reader, uint8_t *out);
CBAT_API CBatError cbat_byte_reader_read_u16_le(CBatByteReader *reader, uint16_t *out);
CBAT_API CBatError cbat_byte_reader_read_u16_be(CBatByteReader *reader, uint16_t *out);
CBAT_API CBatError cbat_byte_reader_read_u32_le(CBatByteReader *reader, uint32_t *out);
CBAT_API CBatError cbat_byte_reader_read_u32_be(CBatByteReader *reader, uint32_t *out);
CBAT_API CBatError cbat_byte_reader_read_u64_le(CBatByteReader *reader, uint64_t *out);
CBAT_API CBatError cbat_byte_reader_read_u64_be(CBatByteReader *reader, uint64_t *out);
CBAT_API CBatError cbat_byte_reader_read_f32_le(CBatByteReader *reader, float *out);
CBAT_API CBatError cbat_byte_reader_read_f32_be(CBatByteReader *reader, float *out);
CBAT_API CBatError cbat_byte_reader_read_f64_le(CBatByteReader *reader, double *out);
CBAT_API CBatError cbat_byte_reader_read_f64_be(CBatByteReader *reader, double *out);

/** Whole-file binary read into a new owned buffer; output unchanged on error. */
CBAT_API CBatError cbat_file_read_bytes(const char *path, CBatBytes **out);
/** Writes exactly length bytes (including NULs); creates/truncates the file. */
CBAT_API CBatError cbat_file_write_bytes(const char *path, const CBatBytes *bytes);
CBAT_END_DECLS
#endif /* CBAT_BYTES_H */
