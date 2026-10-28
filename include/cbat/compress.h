/* cbat/compress.h - DEFLATE, gzip and memory-only ZIP reading. MIT License.
 * Optional CBAT_WITH_COMPRESS (default ON); include explicitly. No external
 * compression dependency. Inputs are borrowed; outputs are owned and use the
 * default allocator. Outputs remain unchanged on failure. NULL data is valid
 * only for zero input length. Malformed/truncated/checksum-invalid data: PARSE.
 */
#ifndef CBAT_COMPRESS_H
#define CBAT_COMPRESS_H
#include "cbat/base.h"
#include "cbat/bytes.h"
#include "cbat/list.h"
#include "cbat/string.h"
CBAT_BEGIN_DECLS
/** Default maximum decompressed bytes (64 MiB), per call across gzip members. */
#define CBAT_DECOMPRESS_DEFAULT_LIMIT ((size_t)67108864)
/** Output limit; zero permits only empty output, SIZE_MAX permits any representable size. */
typedef struct CBatCompressOptions { size_t max_output_size; } CBatCompressOptions;
/** Initializes default output limit; NULL ignored. */
CBAT_API void cbat_compress_options_init(CBatCompressOptions *options);
/** Raw RFC 1951 DEFLATE decoder: stored, fixed and dynamic blocks. No trailing bytes. */
CBAT_API CBatError cbat_inflate(const void *data, size_t length, CBatBytes **out);
/** Same with options (NULL = defaults); limit exceeded gives OUT_OF_RANGE. */
CBAT_API CBatError cbat_inflate_with(const void *data, size_t length, const CBatCompressOptions *options, CBatBytes **out);
/** Raw DEFLATE encoder, greedy LZ77 with fixed Huffman codes (32 KiB window,
 * up to 64 chain probes per position). Valid compressed output; no level option. */
CBAT_API CBatError cbat_deflate(const void *data, size_t length, CBatBytes **out);
/** RFC 1952 decoder with CRC/size/header-CRC checks; concatenated members and trailing zero padding accepted. */
CBAT_API CBatError cbat_gzip_decompress(const void *data, size_t length, CBatBytes **out);
/** Same with decompressed output limit (NULL = defaults). */
CBAT_API CBatError cbat_gzip_decompress_with(const void *data, size_t length, const CBatCompressOptions *options, CBatBytes **out);
/** Deterministic single-member gzip encoder (mtime=0, OS=unknown). */
CBAT_API CBatError cbat_gzip_compress(const void *data, size_t length, CBatBytes **out);
/** ZIP central-directory metadata. name owns exact filename bytes (legacy
 * encodings preserved; UTF-8-flagged names validated). Sizes/offset support ZIP64. */
typedef struct CBatZipEntry {
    CBatString *name;
    uint64_t compressed_size, uncompressed_size, local_header_offset;
    uint32_t crc32;
    uint16_t method, flags;
    bool directory;
} CBatZipEntry;
/** Owned list of CBatZipEntry values; freeing it frees names. Single-disk ZIP32/ZIP64.
 * Metadata listing allows methods/encryption that entry reading may not support.
 * Invalid UTF-8 in a UTF-8-flagged name gives INVALID_UTF8. */
CBAT_API CBatError cbat_zip_list(const void *data, size_t length, CBatList **out);
/** Reads entry by central-directory index, validating local header and CRC/size.
 * Stored/DEFLATE methods supported; encryption, other methods and multi-disk
 * archives give UNSUPPORTED. Returns bytes only, never extracts paths to disk. */
CBAT_API CBatError cbat_zip_read_entry(const void *data, size_t length, size_t index, CBatBytes **out);
/** Same with decompressed output limit (NULL = defaults). */
CBAT_API CBatError cbat_zip_read_entry_with(const void *data, size_t length, size_t index,
    const CBatCompressOptions *options, CBatBytes **out);
CBAT_END_DECLS
#endif
