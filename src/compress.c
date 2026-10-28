/* Independent RFC 1951/1952 implementation and ZIP reader. MIT License.
 * Specifications: https://www.rfc-editor.org/rfc/rfc1951
 * https://www.rfc-editor.org/rfc/rfc1952
 * https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
 */
#include "cbat/compress.h"
#include "cbat/hash.h"
#include <string.h>

static const uint16_t cbat__length_base[29] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                               15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                               67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t cbat__length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                               2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t cbat__distance_base[30] = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t cbat__distance_extra[30] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                                 4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                                 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static uint16_t cbat__compress_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
static uint32_t cbat__compress_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t cbat__compress_u64(const uint8_t *p) {
    return (uint64_t)cbat__compress_u32(p) | (uint64_t)cbat__compress_u32(p + 4) << 32;
}
void cbat_compress_options_init(CBatCompressOptions *options) {
    if (options != NULL)
        options->max_output_size = CBAT_DECOMPRESS_DEFAULT_LIMIT;
}
static size_t cbat__compress_limit(const CBatCompressOptions *options) {
    return options == NULL ? CBAT_DECOMPRESS_DEFAULT_LIMIT : options->max_output_size;
}
static CBatError cbat__compress_append(CBatBytes *out, const void *data, size_t length,
                                       size_t limit) {
    if (out->length > limit || length > limit - out->length)
        return CBAT_ERR_OUT_OF_RANGE;
    return cbat_bytes_append(out, data, length);
}
typedef struct cbat__bits {
    const uint8_t *data;
    size_t length, position;
    unsigned bit;
    CBatError error;
} cbat__bits;
static unsigned cbat__bits_read(cbat__bits *bits, unsigned count) {
    unsigned result = 0, i;
    for (i = 0; i < count; i++) {
        if (bits->position == bits->length) {
            bits->error = CBAT_ERR_PARSE;
            return 0;
        }
        result |= (unsigned)((bits->data[bits->position] >> bits->bit) & 1u) << i;
        if (++bits->bit == 8) {
            bits->bit = 0;
            bits->position++;
        }
    }
    return result;
}
typedef struct cbat__huffman {
    uint16_t count[16], symbols[288];
    unsigned used;
} cbat__huffman;
static CBatError cbat__huffman_build(cbat__huffman *tree, const uint8_t *lengths, unsigned n,
                                     int kind) {
    unsigned offsets[16] = {0}, i;
    int left = 1;
    memset(tree, 0, sizeof(*tree));
    for (i = 0; i < n; i++) {
        if (lengths[i] > 15)
            return CBAT_ERR_PARSE;
        if (lengths[i] != 0) {
            tree->count[lengths[i]]++;
            tree->used++;
        }
    }
    if (tree->used == 0)
        return kind == 2 ? CBAT_OK : CBAT_ERR_PARSE;
    for (i = 1; i <= 15; i++) {
        left = 2 * left - (int)tree->count[i];
        if (left < 0)
            return CBAT_ERR_PARSE;
    }
    if (left != 0 && !(kind != 0 && tree->used == 1 && tree->count[1] == 1))
        return CBAT_ERR_PARSE;
    for (i = 1; i < 15; i++)
        offsets[i + 1] = offsets[i] + tree->count[i];
    for (i = 0; i < n; i++)
        if (lengths[i] != 0)
            tree->symbols[offsets[lengths[i]]++] = (uint16_t)i;
    return CBAT_OK;
}
static int cbat__huffman_decode(cbat__bits *bits, const cbat__huffman *tree) {
    unsigned code = 0, first = 0, index = 0, length;
    if (tree->used == 0) {
        bits->error = CBAT_ERR_PARSE;
        return -1;
    }
    for (length = 1; length <= 15; length++) {
        unsigned count = tree->count[length];
        code |= cbat__bits_read(bits, 1);
        if (bits->error != CBAT_OK)
            return -1;
        if (code >= first && code - first < count)
            return tree->symbols[index + code - first];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    bits->error = CBAT_ERR_PARSE;
    return -1;
}
static CBatError cbat__inflate_trees(cbat__bits *bits, unsigned type, cbat__huffman *literal,
                                     cbat__huffman *distance) {
    uint8_t lengths[320] = {0};
    unsigned i, nlit, ndist;
    CBatError err;
    if (type == 1) {
        for (i = 0; i <= 143; i++)
            lengths[i] = 8;
        for (; i <= 255; i++)
            lengths[i] = 9;
        for (; i <= 279; i++)
            lengths[i] = 7;
        for (; i < 288; i++)
            lengths[i] = 8;
        err = cbat__huffman_build(literal, lengths, 288, 1);
        if (err != CBAT_OK)
            return err;
        memset(lengths, 5, 32);
        return cbat__huffman_build(distance, lengths, 32, 2);
    }
    nlit = cbat__bits_read(bits, 5) + 257;
    ndist = cbat__bits_read(bits, 5) + 1;
    {
        static const unsigned order[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                           11, 4,  12, 3, 13, 2, 14, 1, 15};
        unsigned ncode = cbat__bits_read(bits, 4) + 4;
        uint8_t codes[19] = {0};
        cbat__huffman code_tree;
        if (nlit > 286 || bits->error != CBAT_OK)
            return CBAT_ERR_PARSE;
        for (i = 0; i < ncode; i++)
            codes[order[i]] = (uint8_t)cbat__bits_read(bits, 3);
        if (bits->error != CBAT_OK)
            return CBAT_ERR_PARSE;
        err = cbat__huffman_build(&code_tree, codes, 19, 0);
        if (err != CBAT_OK)
            return err;
        i = 0;
        while (i < nlit + ndist) {
            int code = cbat__huffman_decode(bits, &code_tree);
            unsigned repeat = 1, value;
            if (code < 0)
                return CBAT_ERR_PARSE;
            if (code < 16)
                value = (unsigned)code;
            else if (code == 16) {
                if (i == 0)
                    return CBAT_ERR_PARSE;
                repeat = cbat__bits_read(bits, 2) + 3;
                value = lengths[i - 1];
            } else if (code == 17) {
                repeat = cbat__bits_read(bits, 3) + 3;
                value = 0;
            } else if (code == 18) {
                repeat = cbat__bits_read(bits, 7) + 11;
                value = 0;
            } else
                return CBAT_ERR_PARSE;
            if (bits->error != CBAT_OK || repeat > nlit + ndist - i)
                return CBAT_ERR_PARSE;
            while (repeat-- != 0)
                lengths[i++] = (uint8_t)value;
        }
    }
    if (lengths[256] == 0)
        return CBAT_ERR_PARSE;
    err = cbat__huffman_build(literal, lengths, nlit, 1);
    if (err != CBAT_OK)
        return err;
    return cbat__huffman_build(distance, lengths + nlit, ndist, 2);
}
static CBatError cbat__inflate_stream(const uint8_t *data, size_t length, CBatBytes *out,
                                      size_t limit, size_t *consumed) {
    cbat__bits bits;
    size_t origin = out->length;
    unsigned final = 0;
    memset(&bits, 0, sizeof(bits));
    bits.data = data;
    bits.length = length;
    while (!final) {
        unsigned type;
        CBatError err;
        final = cbat__bits_read(&bits, 1);
        type = cbat__bits_read(&bits, 2);
        if (bits.error != CBAT_OK)
            return CBAT_ERR_PARSE;
        if (type == 0) {
            unsigned count, complement;
            if (bits.bit != 0) {
                bits.position++;
                bits.bit = 0;
            }
            count = cbat__bits_read(&bits, 16);
            complement = cbat__bits_read(&bits, 16);
            if (bits.error != CBAT_OK || ((count ^ complement) & 65535u) != 65535u ||
                count > length - bits.position)
                return CBAT_ERR_PARSE;
            err = cbat__compress_append(out, data + bits.position, count, limit);
            if (err != CBAT_OK)
                return err;
            bits.position += count;
        } else if (type == 1 || type == 2) {
            cbat__huffman literal, distance;
            err = cbat__inflate_trees(&bits, type, &literal, &distance);
            if (err != CBAT_OK)
                return err;
            for (;;) {
                int code = cbat__huffman_decode(&bits, &literal);
                if (code < 0)
                    return CBAT_ERR_PARSE;
                if (code < 256) {
                    uint8_t byte = (uint8_t)code;
                    err = cbat__compress_append(out, &byte, 1, limit);
                    if (err != CBAT_OK)
                        return err;
                } else if (code == 256)
                    break;
                else {
                    unsigned count, back;
                    int dcode;
                    if (code > 285)
                        return CBAT_ERR_PARSE;
                    count = cbat__length_base[code - 257] +
                            cbat__bits_read(&bits, cbat__length_extra[code - 257]);
                    dcode = cbat__huffman_decode(&bits, &distance);
                    if (dcode < 0 || dcode >= 30)
                        return CBAT_ERR_PARSE;
                    back = cbat__distance_base[dcode] +
                           cbat__bits_read(&bits, cbat__distance_extra[dcode]);
                    if (bits.error != CBAT_OK || back > out->length - origin)
                        return CBAT_ERR_PARSE;
                    if (out->length > limit || count > limit - out->length)
                        return CBAT_ERR_OUT_OF_RANGE;
                    while (count != 0) {
                        unsigned take = count < back ? count : back;
                        err = cbat_bytes_append(out, out->data + out->length - back, take);
                        if (err != CBAT_OK)
                            return err;
                        count -= take;
                    }
                }
            }
        } else
            return CBAT_ERR_PARSE;
    }
    *consumed = bits.position + (bits.bit != 0);
    return CBAT_OK;
}
CBatError cbat_inflate_with(const void *data, size_t length, const CBatCompressOptions *options,
                            CBatBytes **out) {
    CBatBytes *result;
    CBatError err;
    size_t consumed = 0;
    if ((data == NULL && length != 0) || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_bytes_new();
    if (result == NULL)
        return CBAT_ERR_NO_MEMORY;
    err = cbat__inflate_stream((const uint8_t *)data, length, result, cbat__compress_limit(options),
                               &consumed);
    if (err == CBAT_OK && consumed != length)
        err = CBAT_ERR_PARSE;
    if (err != CBAT_OK) {
        cbat_bytes_free(result);
        return err;
    }
    *out = result;
    return CBAT_OK;
}
CBatError cbat_inflate(const void *data, size_t length, CBatBytes **out) {
    return cbat_inflate_with(data, length, NULL, out);
}

typedef struct cbat__bit_writer {
    CBatBytes *out;
    unsigned bits, count;
    CBatError error;
} cbat__bit_writer;
static void cbat__bits_write(cbat__bit_writer *writer, unsigned value, unsigned count) {
    if (writer->error != CBAT_OK)
        return;
    writer->bits |= value << writer->count;
    writer->count += count;
    while (writer->count >= 8) {
        writer->error = cbat_bytes_append_u8(writer->out, (uint8_t)writer->bits);
        if (writer->error != CBAT_OK)
            return;
        writer->bits >>= 8;
        writer->count -= 8;
    }
}
static unsigned cbat__reverse_bits(unsigned value, unsigned count) {
    unsigned result = 0;
    while (count-- != 0) {
        result = (result << 1) | (value & 1u);
        value >>= 1;
    }
    return result;
}
static void cbat__fixed_symbol(cbat__bit_writer *writer, unsigned symbol) {
    unsigned code, count;
    if (symbol <= 143) {
        code = 0x30u + symbol;
        count = 8;
    } else if (symbol <= 255) {
        code = 0x190u + symbol - 144;
        count = 9;
    } else if (symbol <= 279) {
        code = symbol - 256;
        count = 7;
    } else {
        code = 0xc0u + symbol - 280;
        count = 8;
    }
    cbat__bits_write(writer, cbat__reverse_bits(code, count), count);
}
static unsigned cbat__deflate_hash(const uint8_t *data) {
    return ((unsigned)data[0] * 251u + (unsigned)data[1] * 31u + data[2]) & 4095u;
}
static CBatError cbat__deflate_stream(const uint8_t *data, size_t length, CBatBytes *out) {
    size_t *heads = NULL, *previous = NULL, at = 0, i;
    cbat__bit_writer writer;
    memset(&writer, 0, sizeof(writer));
    writer.out = out;
    if (length >= 3) {
        heads = (size_t *)cbat_alloc(out->allocator, 4096 * sizeof(size_t));
        previous = (size_t *)cbat_alloc(out->allocator, 32768 * sizeof(size_t));
        if (heads == NULL || previous == NULL) {
            cbat_release(out->allocator, heads);
            cbat_release(out->allocator, previous);
            return CBAT_ERR_NO_MEMORY;
        }
        for (i = 0; i < 4096; i++)
            heads[i] = SIZE_MAX;
    }
    cbat__bits_write(&writer, 3, 3);
    while (at < length && writer.error == CBAT_OK) {
        size_t best = 0, distance = 0, take = 1;
        if (length - at >= 3) {
            size_t candidate = heads[cbat__deflate_hash(data + at)], probes = 0;
            while (candidate != SIZE_MAX && candidate < at && at - candidate <= 32768 &&
                   probes++ < 64) {
                size_t matched = 0, maximum = length - at < 258 ? length - at : 258;
                while (matched < maximum && data[candidate + matched] == data[at + matched])
                    matched++;
                if (matched >= 3 && matched > best) {
                    best = matched;
                    distance = at - candidate;
                    if (best == 258)
                        break;
                }
                candidate = previous[candidate & 32767u];
            }
        }
        if (best >= 3) {
            unsigned lcode = 0, dcode = 0;
            while (lcode < 28 && best >= cbat__length_base[lcode + 1])
                lcode++;
            while (dcode < 29 && distance >= cbat__distance_base[dcode + 1])
                dcode++;
            cbat__fixed_symbol(&writer, 257 + lcode);
            cbat__bits_write(&writer, (unsigned)(best - cbat__length_base[lcode]),
                             cbat__length_extra[lcode]);
            cbat__bits_write(&writer, cbat__reverse_bits(dcode, 5), 5);
            cbat__bits_write(&writer, (unsigned)(distance - cbat__distance_base[dcode]),
                             cbat__distance_extra[dcode]);
            take = best;
        } else
            cbat__fixed_symbol(&writer, data[at]);
        for (i = 0; i < take; i++)
            if (length - at - i >= 3) {
                unsigned hash = cbat__deflate_hash(data + at + i);
                previous[(at + i) & 32767u] = heads[hash];
                heads[hash] = at + i;
            }
        at += take;
    }
    cbat__fixed_symbol(&writer, 256);
    if (writer.error == CBAT_OK && writer.count != 0)
        writer.error = cbat_bytes_append_u8(out, (uint8_t)writer.bits);
    cbat_release(out->allocator, heads);
    cbat_release(out->allocator, previous);
    return writer.error;
}
CBatError cbat_deflate(const void *data, size_t length, CBatBytes **out) {
    CBatBytes *result;
    CBatError err;
    if ((data == NULL && length != 0) || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_bytes_new();
    if (result == NULL)
        return CBAT_ERR_NO_MEMORY;
    err = cbat__deflate_stream((const uint8_t *)data, length, result);
    if (err != CBAT_OK) {
        cbat_bytes_free(result);
        return err;
    }
    *out = result;
    return CBAT_OK;
}
CBatError cbat_gzip_compress(const void *data, size_t length, CBatBytes **out) {
    static const uint8_t header[10] = {31, 139, 8, 0, 0, 0, 0, 0, 0, 255};
    CBatBytes *result;
    CBatError err;
    if ((data == NULL && length != 0) || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_bytes_new();
    if (result == NULL)
        return CBAT_ERR_NO_MEMORY;
    err = cbat_bytes_append(result, header, sizeof(header));
    if (err == CBAT_OK)
        err = cbat__deflate_stream((const uint8_t *)data, length, result);
    if (err == CBAT_OK)
        err = cbat_bytes_append_u32_le(result, cbat_crc32(data, length));
    if (err == CBAT_OK)
        err = cbat_bytes_append_u32_le(result, (uint32_t)length);
    if (err != CBAT_OK) {
        cbat_bytes_free(result);
        return err;
    }
    *out = result;
    return CBAT_OK;
}
CBatError cbat_gzip_decompress_with(const void *data, size_t length,
                                    const CBatCompressOptions *options, CBatBytes **out) {
    const uint8_t *bytes = (const uint8_t *)data;
    CBatBytes *result;
    CBatError err = CBAT_OK;
    size_t position = 0;
    if ((data == NULL && length != 0) || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_bytes_new();
    if (result == NULL)
        return CBAT_ERR_NO_MEMORY;
    if (length == 0) {
        err = CBAT_ERR_PARSE;
        goto done;
    }
    while (position < length) {
        size_t start = position, origin = result->length, consumed = 0;
        unsigned flags;
        if (length - position < 10 || bytes[position] != 31 || bytes[position + 1] != 139 ||
            bytes[position + 2] != 8 || (bytes[position + 3] & 224u) != 0) {
            err = CBAT_ERR_PARSE;
            goto done;
        }
        flags = bytes[position + 3];
        position += 10;
        if (flags & 4u) {
            size_t extra;
            if (length - position < 2) {
                err = CBAT_ERR_PARSE;
                goto done;
            }
            extra = cbat__compress_u16(bytes + position);
            position += 2;
            if (extra > length - position) {
                err = CBAT_ERR_PARSE;
                goto done;
            }
            position += extra;
        }
        if (flags & 8u) {
            while (position < length && bytes[position] != 0)
                position++;
            if (position == length) {
                err = CBAT_ERR_PARSE;
                goto done;
            }
            position++;
        }
        if (flags & 16u) {
            while (position < length && bytes[position] != 0)
                position++;
            if (position == length) {
                err = CBAT_ERR_PARSE;
                goto done;
            }
            position++;
        }
        if (flags & 2u) {
            if (length - position < 2 || (cbat_crc32(bytes + start, position - start) & 65535u) !=
                                             cbat__compress_u16(bytes + position)) {
                err = CBAT_ERR_PARSE;
                goto done;
            }
            position += 2;
        }
        err = cbat__inflate_stream(bytes + position, length - position, result,
                                   cbat__compress_limit(options), &consumed);
        if (err != CBAT_OK)
            goto done;
        position += consumed;
        if (length - position < 8 ||
            cbat__compress_u32(bytes + position) !=
                cbat_crc32(result->data == NULL ? NULL : result->data + origin,
                           result->length - origin) ||
            cbat__compress_u32(bytes + position + 4) != (uint32_t)(result->length - origin)) {
            err = CBAT_ERR_PARSE;
            goto done;
        }
        position += 8;
        if (position < length && bytes[position] == 0) {
            while (position < length && bytes[position] == 0)
                position++;
            if (position != length) {
                err = CBAT_ERR_PARSE;
                goto done;
            }
        }
    }
done:
    if (err != CBAT_OK) {
        cbat_bytes_free(result);
        return err;
    }
    *out = result;
    return CBAT_OK;
}
CBatError cbat_gzip_decompress(const void *data, size_t length, CBatBytes **out) {
    return cbat_gzip_decompress_with(data, length, NULL, out);
}

typedef struct cbat__zip_directory {
    size_t start, end;
    uint64_t count;
} cbat__zip_directory;
typedef struct cbat__zip_item {
    CBatZipEntry entry;
    const uint8_t *name;
    size_t name_length, next;
    bool zip64;
} cbat__zip_item;
static bool cbat__compress_range(size_t length, size_t offset, size_t count) {
    return offset <= length && count <= length - offset;
}
static CBatError cbat__zip_directory_read(const uint8_t *data, size_t length,
                                          cbat__zip_directory *out) {
    size_t position, minimum, metadata;
    uint64_t offset, bytes, count;
    if (length < 22)
        return CBAT_ERR_PARSE;
    position = length - 22;
    minimum = length > 65557 ? length - 65557 : 0;
    for (;;) {
        if (cbat__compress_u32(data + position) == UINT32_C(0x06054b50) &&
            cbat__compress_u16(data + position + 20) == length - position - 22)
            break;
        if (position == minimum)
            return CBAT_ERR_PARSE;
        position--;
    }
    if ((cbat__compress_u16(data + position + 4) != 0 &&
         cbat__compress_u16(data + position + 4) != 65535) ||
        (cbat__compress_u16(data + position + 6) != 0 &&
         cbat__compress_u16(data + position + 6) != 65535))
        return CBAT_ERR_UNSUPPORTED;
    metadata = position;
    count = cbat__compress_u16(data + position + 10);
    bytes = cbat__compress_u32(data + position + 12);
    offset = cbat__compress_u32(data + position + 16);
    if (count == 65535 || bytes == UINT32_MAX || offset == UINT32_MAX ||
        cbat__compress_u16(data + position + 4) == 65535 ||
        cbat__compress_u16(data + position + 6) == 65535 ||
        cbat__compress_u16(data + position + 8) == 65535) {
        size_t record;
        uint64_t record_offset, record_size;
        if (position < 20 || cbat__compress_u32(data + position - 20) != UINT32_C(0x07064b50))
            return CBAT_ERR_PARSE;
        if (cbat__compress_u32(data + position - 16) != 0 ||
            cbat__compress_u32(data + position - 4) != 1)
            return CBAT_ERR_UNSUPPORTED;
        record_offset = cbat__compress_u64(data + position - 12);
        record = (size_t)record_offset;
        if ((uint64_t)record != record_offset || !cbat__compress_range(position - 20, record, 56) ||
            cbat__compress_u32(data + record) != UINT32_C(0x06064b50))
            return CBAT_ERR_PARSE;
        record_size = cbat__compress_u64(data + record + 4);
        if (record_size < 44 || record_size > (uint64_t)(position - 20 - record - 12))
            return CBAT_ERR_PARSE;
        if (cbat__compress_u32(data + record + 16) != 0 ||
            cbat__compress_u32(data + record + 20) != 0 ||
            cbat__compress_u64(data + record + 24) != cbat__compress_u64(data + record + 32))
            return CBAT_ERR_UNSUPPORTED;
        count = cbat__compress_u64(data + record + 32);
        bytes = cbat__compress_u64(data + record + 40);
        offset = cbat__compress_u64(data + record + 48);
        metadata = record;
    } else if (cbat__compress_u16(data + position + 4) != 0 ||
               cbat__compress_u16(data + position + 6) != 0 ||
               cbat__compress_u16(data + position + 8) != count)
        return CBAT_ERR_UNSUPPORTED;
    if ((uint64_t)(size_t)offset != offset || (uint64_t)(size_t)bytes != bytes ||
        !cbat__compress_range(metadata, (size_t)offset, (size_t)bytes) || count > bytes / 46)
        return CBAT_ERR_PARSE;
    out->start = (size_t)offset;
    out->end = out->start + (size_t)bytes;
    out->count = count;
    return CBAT_OK;
}
static CBatError cbat__zip64_extra(const uint8_t *data, size_t length, bool need_size,
                                   bool need_compressed, bool need_offset, bool need_disk,
                                   uint64_t *size, uint64_t *compressed, uint64_t *offset,
                                   uint32_t *disk, bool *present) {
    size_t at = 0;
    bool found = false;
    while (at < length) {
        unsigned id, count;
        size_t end;
        if (length - at < 4)
            return CBAT_ERR_PARSE;
        id = cbat__compress_u16(data + at);
        count = cbat__compress_u16(data + at + 2);
        at += 4;
        if (count > length - at)
            return CBAT_ERR_PARSE;
        end = at + count;
        if (id == 1) {
            if (found)
                return CBAT_ERR_PARSE;
            found = true;
            if (need_size) {
                if (end - at < 8)
                    return CBAT_ERR_PARSE;
                *size = cbat__compress_u64(data + at);
                at += 8;
            }
            if (need_compressed) {
                if (end - at < 8)
                    return CBAT_ERR_PARSE;
                *compressed = cbat__compress_u64(data + at);
                at += 8;
            }
            if (need_offset) {
                if (end - at < 8)
                    return CBAT_ERR_PARSE;
                *offset = cbat__compress_u64(data + at);
                at += 8;
            }
            if (need_disk) {
                if (end - at < 4)
                    return CBAT_ERR_PARSE;
                *disk = cbat__compress_u32(data + at);
            }
        }
        at = end;
    }
    if ((need_size || need_compressed || need_offset || need_disk) && !found)
        return CBAT_ERR_PARSE;
    *present = found;
    return CBAT_OK;
}
static CBatError cbat__zip_item_read(const uint8_t *data, const cbat__zip_directory *directory,
                                     size_t at, cbat__zip_item *out) {
    size_t name_length, extra_length, comment_length, total;
    uint32_t disk;
    CBatError err;
    memset(out, 0, sizeof(*out));
    if (!cbat__compress_range(directory->end, at, 46) ||
        cbat__compress_u32(data + at) != UINT32_C(0x02014b50))
        return CBAT_ERR_PARSE;
    name_length = cbat__compress_u16(data + at + 28);
    extra_length = cbat__compress_u16(data + at + 30);
    comment_length = cbat__compress_u16(data + at + 32);
    total = 46 + name_length + extra_length + comment_length;
    if (!cbat__compress_range(directory->end, at, total))
        return CBAT_ERR_PARSE;
    out->entry.flags = cbat__compress_u16(data + at + 8);
    out->entry.method = cbat__compress_u16(data + at + 10);
    out->entry.crc32 = cbat__compress_u32(data + at + 16);
    out->entry.compressed_size = cbat__compress_u32(data + at + 20);
    out->entry.uncompressed_size = cbat__compress_u32(data + at + 24);
    out->entry.local_header_offset = cbat__compress_u32(data + at + 42);
    disk = cbat__compress_u16(data + at + 34);
    err = cbat__zip64_extra(
        data + at + 46 + name_length, extra_length, out->entry.uncompressed_size == UINT32_MAX,
        out->entry.compressed_size == UINT32_MAX, out->entry.local_header_offset == UINT32_MAX,
        disk == 65535, &out->entry.uncompressed_size, &out->entry.compressed_size,
        &out->entry.local_header_offset, &disk, &out->zip64);
    if (err != CBAT_OK)
        return err;
    if (disk != 0)
        return CBAT_ERR_UNSUPPORTED;
    if (out->entry.local_header_offset > directory->start ||
        directory->start - out->entry.local_header_offset < 30)
        return CBAT_ERR_PARSE;
    out->name = data + at + 46;
    out->name_length = name_length;
    out->next = at + total;
    out->entry.directory = (name_length != 0 && out->name[name_length - 1] == '/') ||
                           (cbat__compress_u32(data + at + 38) & 16u) != 0;
    if ((out->entry.flags & 2048u) && !cbat_utf8_is_valid((const char *)out->name, name_length))
        return CBAT_ERR_INVALID_UTF8;
    return CBAT_OK;
}
static CBatError cbat__zip_tail(const uint8_t *data, size_t at, size_t end) {
    if (at == end)
        return CBAT_OK;
    if (!cbat__compress_range(end, at, 6) ||
        cbat__compress_u32(data + at) != UINT32_C(0x05054b50) ||
        cbat__compress_u16(data + at + 4) != end - at - 6)
        return CBAT_ERR_PARSE;
    return CBAT_OK;
}
static void cbat__zip_destroy(void *slot) { cbat_string_free(((CBatZipEntry *)slot)->name); }
CBatError cbat_zip_list(const void *data, size_t length, CBatList **out) {
    const uint8_t *bytes = (const uint8_t *)data;
    cbat__zip_directory directory;
    CBatList *entries;
    CBatError err;
    size_t at;
    uint64_t i;
    if ((data == NULL && length != 0) || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat__zip_directory_read(bytes, length, &directory);
    if (err != CBAT_OK)
        return err;
    entries = cbat_list_new(sizeof(CBatZipEntry));
    if (entries == NULL)
        return CBAT_ERR_NO_MEMORY;
    cbat_list_set_destructor(entries, cbat__zip_destroy);
    at = directory.start;
    for (i = 0; i < directory.count; i++) {
        cbat__zip_item item;
        err = cbat__zip_item_read(bytes, &directory, at, &item);
        if (err != CBAT_OK)
            goto done;
        item.entry.name = cbat_string_from_length((const char *)item.name, item.name_length);
        if (item.entry.name == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            goto done;
        }
        err = cbat_list_push(entries, &item.entry);
        if (err != CBAT_OK) {
            cbat_string_free(item.entry.name);
            goto done;
        }
        at = item.next;
    }
    err = cbat__zip_tail(bytes, at, directory.end);
done:
    if (err != CBAT_OK) {
        cbat_list_free(entries);
        return err;
    }
    *out = entries;
    return CBAT_OK;
}
static bool cbat__zip_descriptor(const uint8_t *data, size_t position, size_t end,
                                 const CBatZipEntry *entry, bool zip64) {
    unsigned attempt;
    for (attempt = 0; attempt < 2; attempt++) {
        size_t at = position, needed = zip64 ? 20 : 12;
        if (attempt == 0) {
            if (!cbat__compress_range(end, at, 4) ||
                cbat__compress_u32(data + at) != UINT32_C(0x08074b50))
                continue;
            at += 4;
        }
        if (!cbat__compress_range(end, at, needed) || cbat__compress_u32(data + at) != entry->crc32)
            continue;
        if (zip64) {
            if (cbat__compress_u64(data + at + 4) == entry->compressed_size &&
                cbat__compress_u64(data + at + 12) == entry->uncompressed_size)
                return true;
        } else if (cbat__compress_u32(data + at + 4) == entry->compressed_size &&
                   cbat__compress_u32(data + at + 8) == entry->uncompressed_size)
            return true;
    }
    return false;
}
CBatError cbat_zip_read_entry_with(const void *data, size_t length, size_t index,
                                   const CBatCompressOptions *options, CBatBytes **out) {
    const uint8_t *bytes = (const uint8_t *)data;
    cbat__zip_directory directory;
    cbat__zip_item chosen;
    CBatZipEntry *entry = &chosen.entry;
    CBatBytes *result = NULL;
    CBatError err;
    size_t at, local, name_length, extra_length, payload, compressed, consumed = 0;
    uint64_t i, local_size, local_compressed, unused_offset = 0;
    uint32_t unused_disk = 0;
    bool local_zip64 = false;
    if ((data == NULL && length != 0) || out == NULL)
        return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat__zip_directory_read(bytes, length, &directory);
    if (err != CBAT_OK)
        return err;
    if (index >= directory.count)
        return CBAT_ERR_OUT_OF_RANGE;
    memset(&chosen, 0, sizeof(chosen));
    at = directory.start;
    for (i = 0; i < directory.count; i++) {
        cbat__zip_item item;
        err = cbat__zip_item_read(bytes, &directory, at, &item);
        if (err != CBAT_OK)
            return err;
        if (i == index)
            chosen = item;
        at = item.next;
    }
    err = cbat__zip_tail(bytes, at, directory.end);
    if (err != CBAT_OK)
        return err;
    if ((entry->flags & (1u | 32u | 64u | 8192u)) != 0 ||
        (entry->method != 0 && entry->method != 8))
        return CBAT_ERR_UNSUPPORTED;
    if (entry->uncompressed_size > cbat__compress_limit(options))
        return CBAT_ERR_OUT_OF_RANGE;
    local = (size_t)entry->local_header_offset;
    compressed = (size_t)entry->compressed_size;
    if ((uint64_t)local != entry->local_header_offset ||
        (uint64_t)compressed != entry->compressed_size ||
        !cbat__compress_range(directory.start, local, 30) ||
        cbat__compress_u32(bytes + local) != UINT32_C(0x04034b50))
        return CBAT_ERR_PARSE;
    if (cbat__compress_u16(bytes + local + 6) != entry->flags ||
        cbat__compress_u16(bytes + local + 8) != entry->method)
        return CBAT_ERR_PARSE;
    name_length = cbat__compress_u16(bytes + local + 26);
    extra_length = cbat__compress_u16(bytes + local + 28);
    if (!cbat__compress_range(directory.start, local, 30 + name_length + extra_length) ||
        name_length != chosen.name_length ||
        memcmp(bytes + local + 30, chosen.name, name_length) != 0)
        return CBAT_ERR_PARSE;
    payload = local + 30 + name_length + extra_length;
    if (!cbat__compress_range(directory.start, payload, compressed))
        return CBAT_ERR_PARSE;
    local_size = cbat__compress_u32(bytes + local + 22);
    local_compressed = cbat__compress_u32(bytes + local + 18);
    err =
        cbat__zip64_extra(bytes + local + 30 + name_length, extra_length, local_size == UINT32_MAX,
                          local_compressed == UINT32_MAX, false, false, &local_size,
                          &local_compressed, &unused_offset, &unused_disk, &local_zip64);
    if (err != CBAT_OK)
        return err;
    if (entry->flags & 8u) {
        if (!cbat__zip_descriptor(bytes, payload + compressed, directory.start, entry,
                                  chosen.zip64 || local_zip64))
            return CBAT_ERR_PARSE;
    } else if (local_size != entry->uncompressed_size ||
               local_compressed != entry->compressed_size ||
               cbat__compress_u32(bytes + local + 14) != entry->crc32)
        return CBAT_ERR_PARSE;
    result = cbat_bytes_new();
    if (result == NULL)
        return CBAT_ERR_NO_MEMORY;
    if (entry->method == 0) {
        if (entry->compressed_size != entry->uncompressed_size) {
            err = CBAT_ERR_PARSE;
            goto done;
        }
        err = cbat__compress_append(result, bytes + payload, compressed,
                                    cbat__compress_limit(options));
    } else {
        err = cbat__inflate_stream(bytes + payload, compressed, result,
                                   cbat__compress_limit(options), &consumed);
        if (err == CBAT_OK && consumed != compressed)
            err = CBAT_ERR_PARSE;
    }
    if (err == CBAT_OK && (result->length != entry->uncompressed_size ||
                           cbat_crc32(result->data, result->length) != entry->crc32))
        err = CBAT_ERR_PARSE;
done:
    if (err != CBAT_OK) {
        cbat_bytes_free(result);
        return err;
    }
    *out = result;
    return CBAT_OK;
}
CBatError cbat_zip_read_entry(const void *data, size_t length, size_t index, CBatBytes **out) {
    return cbat_zip_read_entry_with(data, length, index, NULL, out);
}
