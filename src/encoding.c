/*
 * cbat/encoding.c - Base64 and hex.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/encoding.h"

static const char cbat__base64_standard[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char cbat__base64_url[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static CBatError cbat__base64_encode(const void *data, size_t length, const char *alphabet,
                                     bool pad, CBatString **out) {
    const unsigned char *bytes = (const unsigned char *)data;
    CBatString *s;
    size_t i;
    size_t groups;
    CBatError err;
    if ((data == NULL && length > 0) || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    s = cbat_string_new();
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    groups = length / 3 + 1;
    if (groups > SIZE_MAX / 4) {
        cbat_string_free(s);
        return CBAT_ERR_OVERFLOW;
    }
    err = cbat_string_reserve(s, groups * 4);
    if (err != CBAT_OK) {
        cbat_string_free(s);
        return err;
    }
    for (i = 0; i + 2 < length; i += 3) {
        uint32_t v = ((uint32_t)bytes[i] << 16) | ((uint32_t)bytes[i + 1] << 8) | bytes[i + 2];
        s->data[s->length++] = alphabet[(v >> 18) & 63];
        s->data[s->length++] = alphabet[(v >> 12) & 63];
        s->data[s->length++] = alphabet[(v >> 6) & 63];
        s->data[s->length++] = alphabet[v & 63];
    }
    if (length - i == 1) {
        uint32_t v = (uint32_t)bytes[i] << 16;
        s->data[s->length++] = alphabet[(v >> 18) & 63];
        s->data[s->length++] = alphabet[(v >> 12) & 63];
        if (pad) {
            s->data[s->length++] = '=';
            s->data[s->length++] = '=';
        }
    } else if (length - i == 2) {
        uint32_t v = ((uint32_t)bytes[i] << 16) | ((uint32_t)bytes[i + 1] << 8);
        s->data[s->length++] = alphabet[(v >> 18) & 63];
        s->data[s->length++] = alphabet[(v >> 12) & 63];
        s->data[s->length++] = alphabet[(v >> 6) & 63];
        if (pad) {
            s->data[s->length++] = '=';
        }
    }
    s->data[s->length] = '\0';
    *out = s;
    return CBAT_OK;
}

CBatError cbat_base64_encode(const void *data, size_t length, CBatString **out) {
    return cbat__base64_encode(data, length, cbat__base64_standard, true, out);
}

CBatError cbat_base64url_encode(const void *data, size_t length, CBatString **out) {
    return cbat__base64_encode(data, length, cbat__base64_url, false, out);
}

static int cbat__base64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

static bool cbat__encoding_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

CBatError cbat_base64_decode(const char *text, size_t length, CBatString **out) {
    CBatString *s;
    uint32_t accumulator = 0;
    int bits = 0;
    size_t digits = 0;
    size_t padding = 0;
    size_t i;
    CBatError err;
    if ((text == NULL && length > 0) || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    s = cbat_string_new();
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_string_reserve(s, length / 4 * 3 + 3);
    for (i = 0; i < length && err == CBAT_OK; i++) {
        char c = text[i];
        int value;
        if (cbat__encoding_is_space(c)) {
            continue;
        }
        if (c == '=') {
            padding++;
            continue;
        }
        value = cbat__base64_value(c);
        if (value < 0 || padding > 0) {
            err = CBAT_ERR_PARSE; /* bad character, or data after padding */
            break;
        }
        accumulator = (accumulator << 6) | (uint32_t)value;
        bits += 6;
        digits++;
        if (bits >= 8) {
            bits -= 8;
            s->data[s->length++] = (char)((accumulator >> bits) & 0xFF);
        }
    }
    /* A lone final digit (length % 4 == 1) cannot encode a byte. */
    if (err == CBAT_OK && (digits % 4 == 1 || padding > 2 ||
                           (padding > 0 && (digits + padding) % 4 != 0))) {
        err = CBAT_ERR_PARSE;
    }
    if (err != CBAT_OK) {
        cbat_string_free(s);
        return err;
    }
    s->data[s->length] = '\0';
    *out = s;
    return CBAT_OK;
}

CBatError cbat_hex_encode(const void *data, size_t length, CBatString **out) {
    static const char digits[] = "0123456789abcdef";
    const unsigned char *bytes = (const unsigned char *)data;
    CBatString *s;
    size_t i;
    CBatError err;
    if ((data == NULL && length > 0) || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (length > SIZE_MAX / 2 - 1) {
        return CBAT_ERR_OVERFLOW;
    }
    s = cbat_string_new();
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_string_reserve(s, length * 2);
    if (err != CBAT_OK) {
        cbat_string_free(s);
        return err;
    }
    for (i = 0; i < length; i++) {
        s->data[s->length++] = digits[bytes[i] >> 4];
        s->data[s->length++] = digits[bytes[i] & 0x0F];
    }
    s->data[s->length] = '\0';
    *out = s;
    return CBAT_OK;
}

static int cbat__hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

CBatError cbat_hex_decode(const char *text, size_t length, CBatString **out) {
    CBatString *s;
    size_t i;
    CBatError err;
    if ((text == NULL && length > 0) || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (length % 2 != 0) {
        return CBAT_ERR_PARSE;
    }
    s = cbat_string_new();
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_string_reserve(s, length / 2);
    for (i = 0; i < length && err == CBAT_OK; i += 2) {
        int high = cbat__hex_value(text[i]);
        int low = cbat__hex_value(text[i + 1]);
        if (high < 0 || low < 0) {
            err = CBAT_ERR_PARSE;
            break;
        }
        s->data[s->length++] = (char)((high << 4) | low);
    }
    if (err != CBAT_OK) {
        cbat_string_free(s);
        return err;
    }
    s->data[s->length] = '\0';
    *out = s;
    return CBAT_OK;
}
