/*
 * cbat/string.c - CBatString, number parsing and UTF-8.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/string.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CBAT__STRING_MIN_CAPACITY 16

static bool cbat__string_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Ensures capacity for `length` bytes plus the terminator. */
static CBatError cbat__string_ensure(CBatString *s, size_t length) {
    size_t needed;
    size_t new_capacity;
    char *fresh;
    if (!cbat_size_add(length, 1, &needed)) {
        return CBAT_ERR_OVERFLOW;
    }
    if (needed <= s->capacity) {
        return CBAT_OK;
    }
    new_capacity = s->capacity < CBAT__STRING_MIN_CAPACITY ? CBAT__STRING_MIN_CAPACITY
                                                           : s->capacity;
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = needed;
            break;
        }
        new_capacity *= 2;
    }
    fresh = (char *)cbat_resize(s->allocator, s->data, s->capacity, new_capacity);
    if (fresh == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    s->data = fresh;
    s->capacity = new_capacity;
    return CBAT_OK;
}

/* ---- Creating and freeing ----------------------------------------------- */

CBatString *cbat_string_new_with(const CBatAllocator *allocator) {
    CBatString *s;
    if (allocator == NULL) {
        allocator = cbat_default_allocator();
    }
    s = (CBatString *)cbat_alloc(allocator, sizeof(CBatString));
    if (s == NULL) {
        return NULL;
    }
    s->data = (char *)cbat_alloc(allocator, CBAT__STRING_MIN_CAPACITY);
    if (s->data == NULL) {
        cbat_release(allocator, s);
        return NULL;
    }
    s->data[0] = '\0';
    s->length = 0;
    s->capacity = CBAT__STRING_MIN_CAPACITY;
    s->allocator = allocator;
    return s;
}

CBatString *cbat_string_new(void) {
    return cbat_string_new_with(NULL);
}

CBatString *cbat_string_from_length(const char *text, size_t length) {
    CBatString *s = cbat_string_new();
    if (s == NULL) {
        return NULL;
    }
    if (text != NULL && cbat_string_append_length(s, text, length) != CBAT_OK) {
        cbat_string_free(s);
        return NULL;
    }
    return s;
}

CBatString *cbat_string_from(const char *text) {
    return cbat_string_from_length(text, text == NULL ? 0 : strlen(text));
}

CBatString *cbat_string_from_format(const char *format, ...) {
    CBatString *s = cbat_string_new();
    va_list args;
    CBatError err;
    if (s == NULL) {
        return NULL;
    }
    va_start(args, format);
    err = cbat_string_append_vformat(s, format, args);
    va_end(args);
    if (err != CBAT_OK) {
        cbat_string_free(s);
        return NULL;
    }
    return s;
}

CBatString *cbat_string_copy(const CBatString *s) {
    if (s == NULL) {
        return NULL;
    }
    return cbat_string_from_length(s->data, s->length);
}

void cbat_string_free(CBatString *s) {
    if (s == NULL) {
        return;
    }
    cbat_release(s->allocator, s->data);
    cbat_release(s->allocator, s);
}

char *cbat_string_steal(CBatString *s) {
    char *data;
    if (s == NULL) {
        return NULL;
    }
    data = s->data;
    cbat_release(s->allocator, s);
    return data;
}

void cbat_string_destructor(void *element) {
    if (element != NULL) {
        cbat_string_free(*(CBatString **)element);
    }
}

/* ---- Reading ------------------------------------------------------------ */

const char *cbat_string_cstr(const CBatString *s) {
    return s == NULL ? "" : s->data;
}

size_t cbat_string_length(const CBatString *s) {
    return s == NULL ? 0 : s->length;
}

bool cbat_string_is_empty(const CBatString *s) {
    return cbat_string_length(s) == 0;
}

/* ---- Modifying ---------------------------------------------------------- */

void cbat_string_clear(CBatString *s) {
    if (s != NULL) {
        s->length = 0;
        s->data[0] = '\0';
    }
}

CBatError cbat_string_reserve(CBatString *s, size_t additional) {
    size_t needed;
    if (s == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (!cbat_size_add(s->length, additional, &needed)) {
        return CBAT_ERR_OVERFLOW;
    }
    return cbat__string_ensure(s, needed);
}

CBatError cbat_string_set(CBatString *s, const char *text) {
    size_t length;
    CBatError err;
    if (s == NULL || text == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    length = strlen(text);
    err = cbat__string_ensure(s, length);
    if (err != CBAT_OK) {
        return err;
    }
    memmove(s->data, text, length);
    s->length = length;
    s->data[length] = '\0';
    return CBAT_OK;
}

CBatError cbat_string_append_length(CBatString *s, const char *text, size_t length) {
    size_t new_length;
    size_t offset = 0;
    bool aliased;
    CBatError err;
    if (s == NULL || (text == NULL && length > 0)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (length == 0) {
        return CBAT_OK;
    }
    if (!cbat_size_add(s->length, length, &new_length)) {
        return CBAT_ERR_OVERFLOW;
    }
    /* Appending part of ourselves: remember the offset across reallocation. */
    aliased = text >= s->data && text < s->data + s->capacity;
    if (aliased) {
        offset = (size_t)(text - s->data);
    }
    err = cbat__string_ensure(s, new_length);
    if (err != CBAT_OK) {
        return err;
    }
    if (aliased) {
        text = s->data + offset;
    }
    memmove(s->data + s->length, text, length);
    s->length = new_length;
    s->data[new_length] = '\0';
    return CBAT_OK;
}

CBatError cbat_string_append(CBatString *s, const char *text) {
    if (text == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat_string_append_length(s, text, strlen(text));
}

CBatError cbat_string_append_char(CBatString *s, char c) {
    return cbat_string_append_length(s, &c, 1);
}

CBatError cbat_string_append_string(CBatString *s, const CBatString *other) {
    if (other == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat_string_append_length(s, other->data, other->length);
}

CBatError cbat_string_append_vformat(CBatString *s, const char *format, va_list args) {
    va_list measure;
    int needed;
    CBatError err;
    if (s == NULL || format == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    va_copy(measure, args);
    needed = vsnprintf(NULL, 0, format, measure);
    va_end(measure);
    if (needed < 0) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat_string_reserve(s, (size_t)needed);
    if (err != CBAT_OK) {
        return err;
    }
    vsnprintf(s->data + s->length, (size_t)needed + 1, format, args);
    s->length += (size_t)needed;
    return CBAT_OK;
}

CBatError cbat_string_append_format(CBatString *s, const char *format, ...) {
    va_list args;
    CBatError err;
    va_start(args, format);
    err = cbat_string_append_vformat(s, format, args);
    va_end(args);
    return err;
}

CBatError cbat_string_append_codepoint(CBatString *s, uint32_t codepoint) {
    char buffer[4];
    size_t n = cbat_utf8_encode(codepoint, buffer);
    if (s == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (n == 0) {
        return CBAT_ERR_INVALID_UTF8;
    }
    return cbat_string_append_length(s, buffer, n);
}

CBatError cbat_string_insert(CBatString *s, size_t position, const char *text) {
    CBatString *copy;
    size_t length;
    size_t new_length;
    CBatError err;
    if (s == NULL || text == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (position > s->length) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    length = strlen(text);
    if (length == 0) {
        return CBAT_OK;
    }
    if (!cbat_size_add(s->length, length, &new_length)) {
        return CBAT_ERR_OVERFLOW;
    }
    /* Inserting part of ourselves: work from a private copy. */
    copy = NULL;
    if (text >= s->data && text < s->data + s->capacity) {
        copy = cbat_string_from_length(text, length);
        if (copy == NULL) {
            return CBAT_ERR_NO_MEMORY;
        }
        text = copy->data;
    }
    err = cbat__string_ensure(s, new_length);
    if (err == CBAT_OK) {
        memmove(s->data + position + length, s->data + position, s->length - position + 1);
        memcpy(s->data + position, text, length);
        s->length = new_length;
    }
    cbat_string_free(copy);
    return err;
}

CBatError cbat_string_erase(CBatString *s, size_t position, size_t count) {
    if (s == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (position > s->length) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    if (count > s->length - position) {
        count = s->length - position;
    }
    memmove(s->data + position, s->data + position + count, s->length - position - count + 1);
    s->length -= count;
    return CBAT_OK;
}

void cbat_string_truncate(CBatString *s, size_t length) {
    if (s != NULL && length < s->length) {
        s->length = length;
        s->data[length] = '\0';
    }
}

/* Length-aware substring search starting at `start`. */
static size_t cbat__string_search(const char *haystack, size_t haystack_length,
                                  const char *needle, size_t needle_length, size_t start) {
    size_t i;
    if (start > haystack_length || needle_length > haystack_length - start) {
        return CBAT_NPOS;
    }
    if (needle_length == 0) {
        return start;
    }
    for (i = start; i + needle_length <= haystack_length; i++) {
        const char *hit = (const char *)memchr(haystack + i, needle[0],
                                               haystack_length - needle_length + 1 - i);
        if (hit == NULL) {
            return CBAT_NPOS;
        }
        i = (size_t)(hit - haystack);
        if (memcmp(hit, needle, needle_length) == 0) {
            return i;
        }
    }
    return CBAT_NPOS;
}

CBatError cbat_string_replace(CBatString *s, const char *from, const char *to, size_t *count) {
    CBatString *result;
    size_t from_length;
    size_t to_length;
    size_t position = 0;
    size_t replaced = 0;
    CBatError err = CBAT_OK;
    if (s == NULL || from == NULL || to == NULL || from[0] == '\0') {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    from_length = strlen(from);
    to_length = strlen(to);
    result = cbat_string_new_with(s->allocator);
    if (result == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    for (;;) {
        size_t hit = cbat__string_search(s->data, s->length, from, from_length, position);
        if (hit == CBAT_NPOS) {
            break;
        }
        err = cbat_string_append_length(result, s->data + position, hit - position);
        if (err == CBAT_OK) {
            err = cbat_string_append_length(result, to, to_length);
        }
        if (err != CBAT_OK) {
            cbat_string_free(result);
            return err;
        }
        position = hit + from_length;
        replaced++;
    }
    if (replaced > 0) {
        char *old_data = s->data;
        err = cbat_string_append_length(result, s->data + position, s->length - position);
        if (err != CBAT_OK) {
            cbat_string_free(result);
            return err;
        }
        s->data = result->data;
        s->length = result->length;
        s->capacity = result->capacity;
        result->data = old_data;
    }
    cbat_string_free(result);
    if (count != NULL) {
        *count = replaced;
    }
    return CBAT_OK;
}

void cbat_string_to_upper(CBatString *s) {
    size_t i;
    for (i = 0; s != NULL && i < s->length; i++) {
        if (s->data[i] >= 'a' && s->data[i] <= 'z') {
            s->data[i] = (char)(s->data[i] - 'a' + 'A');
        }
    }
}

void cbat_string_to_lower(CBatString *s) {
    size_t i;
    for (i = 0; s != NULL && i < s->length; i++) {
        if (s->data[i] >= 'A' && s->data[i] <= 'Z') {
            s->data[i] = (char)(s->data[i] - 'A' + 'a');
        }
    }
}

void cbat_string_trim_right(CBatString *s) {
    if (s == NULL) {
        return;
    }
    while (s->length > 0 && cbat__string_is_space(s->data[s->length - 1])) {
        s->length--;
    }
    s->data[s->length] = '\0';
}

void cbat_string_trim_left(CBatString *s) {
    size_t start = 0;
    if (s == NULL) {
        return;
    }
    while (start < s->length && cbat__string_is_space(s->data[start])) {
        start++;
    }
    if (start > 0) {
        cbat_string_erase(s, 0, start);
    }
}

void cbat_string_trim(CBatString *s) {
    cbat_string_trim_right(s);
    cbat_string_trim_left(s);
}

/* ---- Searching and comparing -------------------------------------------- */

size_t cbat_string_find(const CBatString *s, const char *needle, size_t start) {
    if (s == NULL || needle == NULL) {
        return CBAT_NPOS;
    }
    return cbat__string_search(s->data, s->length, needle, strlen(needle), start);
}

size_t cbat_string_rfind(const CBatString *s, const char *needle) {
    size_t needle_length;
    size_t i;
    if (s == NULL || needle == NULL) {
        return CBAT_NPOS;
    }
    needle_length = strlen(needle);
    if (needle_length > s->length) {
        return CBAT_NPOS;
    }
    i = s->length - needle_length + 1;
    while (i-- > 0) {
        if (memcmp(s->data + i, needle, needle_length) == 0) {
            return i;
        }
    }
    return CBAT_NPOS;
}

bool cbat_string_contains(const CBatString *s, const char *needle) {
    return cbat_string_find(s, needle, 0) != CBAT_NPOS;
}

size_t cbat_string_count(const CBatString *s, const char *needle) {
    size_t needle_length;
    size_t position = 0;
    size_t total = 0;
    if (s == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }
    needle_length = strlen(needle);
    for (;;) {
        size_t hit = cbat__string_search(s->data, s->length, needle, needle_length, position);
        if (hit == CBAT_NPOS) {
            return total;
        }
        total++;
        position = hit + needle_length;
    }
}

bool cbat_string_starts_with(const CBatString *s, const char *prefix) {
    size_t length;
    if (s == NULL || prefix == NULL) {
        return false;
    }
    length = strlen(prefix);
    return length <= s->length && memcmp(s->data, prefix, length) == 0;
}

bool cbat_string_ends_with(const CBatString *s, const char *suffix) {
    size_t length;
    if (s == NULL || suffix == NULL) {
        return false;
    }
    length = strlen(suffix);
    return length <= s->length && memcmp(s->data + s->length - length, suffix, length) == 0;
}

bool cbat_string_equals(const CBatString *s, const char *text) {
    size_t length;
    if (s == NULL || text == NULL) {
        return false;
    }
    length = strlen(text);
    return length == s->length && memcmp(s->data, text, length) == 0;
}

int cbat_string_compare(const CBatString *a, const CBatString *b) {
    size_t common;
    int result;
    if (a == NULL || b == NULL) {
        return (a != NULL) - (b != NULL);
    }
    common = CBAT_MIN(a->length, b->length);
    result = memcmp(a->data, b->data, common);
    if (result != 0) {
        return result < 0 ? -1 : 1;
    }
    return (a->length > b->length) - (a->length < b->length);
}

int cbat_string_compare_elements(const void *a, const void *b) {
    return cbat_string_compare(*(const CBatString *const *)a, *(const CBatString *const *)b);
}

/* ---- Producing new strings ---------------------------------------------- */

CBatString *cbat_string_substring(const CBatString *s, size_t start, size_t count) {
    if (s == NULL) {
        return NULL;
    }
    if (start > s->length) {
        start = s->length;
    }
    if (count > s->length - start) {
        count = s->length - start;
    }
    return cbat_string_from_length(s->data + start, count);
}

CBatString *cbat_string_repeat(const char *text, size_t times) {
    CBatString *s;
    size_t length;
    size_t total;
    size_t i;
    if (text == NULL) {
        return NULL;
    }
    s = cbat_string_new();
    if (s == NULL) {
        return NULL;
    }
    length = strlen(text);
    if (!cbat_size_mul(length, times, &total) || cbat_string_reserve(s, total) != CBAT_OK) {
        cbat_string_free(s);
        return NULL;
    }
    for (i = 0; i < times; i++) {
        cbat_string_append_length(s, text, length); /* cannot fail: reserved */
    }
    return s;
}

static CBatList *cbat__string_list_new(void) {
    CBatList *list = cbat_list_new(sizeof(CBatString *));
    if (list != NULL) {
        cbat_list_set_destructor(list, cbat_string_destructor);
    }
    return list;
}

static CBatError cbat__string_list_push(CBatList *list, const char *text, size_t length) {
    CBatString *piece = cbat_string_from_length(text, length);
    CBatError err;
    if (piece == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_list_push(list, &piece);
    if (err != CBAT_OK) {
        cbat_string_free(piece);
    }
    return err;
}

CBatError cbat_string_split(const char *text, const char *separator, CBatList **out) {
    CBatList *list;
    size_t text_length;
    size_t separator_length;
    size_t position = 0;
    CBatError err;
    if (text == NULL || separator == NULL || separator[0] == '\0' || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    list = cbat__string_list_new();
    if (list == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    text_length = strlen(text);
    separator_length = strlen(separator);
    for (;;) {
        size_t hit = cbat__string_search(text, text_length, separator, separator_length, position);
        size_t end = hit == CBAT_NPOS ? text_length : hit;
        err = cbat__string_list_push(list, text + position, end - position);
        if (err != CBAT_OK) {
            cbat_list_free(list);
            return err;
        }
        if (hit == CBAT_NPOS) {
            break;
        }
        position = hit + separator_length;
    }
    *out = list;
    return CBAT_OK;
}

CBatError cbat_string_split_whitespace(const char *text, CBatList **out) {
    CBatList *list;
    size_t i = 0;
    if (text == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    list = cbat__string_list_new();
    if (list == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    while (text[i] != '\0') {
        size_t start;
        CBatError err;
        while (text[i] != '\0' && cbat__string_is_space(text[i])) {
            i++;
        }
        if (text[i] == '\0') {
            break;
        }
        start = i;
        while (text[i] != '\0' && !cbat__string_is_space(text[i])) {
            i++;
        }
        err = cbat__string_list_push(list, text + start, i - start);
        if (err != CBAT_OK) {
            cbat_list_free(list);
            return err;
        }
    }
    *out = list;
    return CBAT_OK;
}

CBatError cbat_string_join(const CBatList *strings, const char *separator, CBatString **out) {
    CBatString *result;
    size_t i;
    if (strings == NULL || separator == NULL || out == NULL ||
        strings->element_size != sizeof(CBatString *)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    result = cbat_string_new();
    if (result == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    for (i = 0; i < strings->length; i++) {
        const CBatString *item = *(CBatString *const *)cbat_list_get(strings, i);
        CBatError err = CBAT_OK;
        if (i > 0) {
            err = cbat_string_append(result, separator);
        }
        if (err == CBAT_OK && item != NULL) {
            err = cbat_string_append_string(result, item);
        }
        if (err != CBAT_OK) {
            cbat_string_free(result);
            return err;
        }
    }
    *out = result;
    return CBAT_OK;
}

/* ---- Parsing numbers ---------------------------------------------------- */

static int cbat__digit_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    return 99;
}

CBatError cbat_string_to_int64_base(const char *text, int base, int64_t *out) {
    const char *p = text;
    bool negative = false;
    uint64_t limit;
    uint64_t value = 0;
    bool any = false;
    if (text == NULL || out == NULL || base < 2 || base > 36) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    while (cbat__string_is_space(*p)) {
        p++;
    }
    if (*p == '+' || *p == '-') {
        negative = *p == '-';
        p++;
    }
    /* Magnitude limit: 2^63 for negatives, 2^63 - 1 for positives. */
    limit = negative ? (uint64_t)INT64_MAX + 1u : (uint64_t)INT64_MAX;
    while (*p != '\0' && !cbat__string_is_space(*p)) {
        int digit = cbat__digit_value(*p);
        if (digit >= base) {
            return CBAT_ERR_PARSE;
        }
        if (value > (limit - (uint64_t)digit) / (uint64_t)base) {
            /* Keep scanning so "12x...9" reports PARSE, not OVERFLOW. */
            while (*p != '\0' && !cbat__string_is_space(*p)) {
                if (cbat__digit_value(*p) >= base) {
                    return CBAT_ERR_PARSE;
                }
                p++;
            }
            while (cbat__string_is_space(*p)) {
                p++;
            }
            return *p == '\0' ? CBAT_ERR_OVERFLOW : CBAT_ERR_PARSE;
        }
        value = value * (uint64_t)base + (uint64_t)digit;
        any = true;
        p++;
    }
    while (cbat__string_is_space(*p)) {
        p++;
    }
    if (!any || *p != '\0') {
        return CBAT_ERR_PARSE;
    }
    if (negative) {
        *out = value == (uint64_t)INT64_MAX + 1u ? INT64_MIN : -(int64_t)value;
    } else {
        *out = (int64_t)value;
    }
    return CBAT_OK;
}

CBatError cbat_string_to_int64(const char *text, int64_t *out) {
    return cbat_string_to_int64_base(text, 10, out);
}

CBatError cbat_string_to_double(const char *text, double *out) {
    char *end;
    double value;
    const char *p = text;
    if (text == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    while (cbat__string_is_space(*p)) {
        p++;
    }
    if (*p == '\0') {
        return CBAT_ERR_PARSE;
    }
    /* Hex floats are a C-ism that surprises people parsing data; reject. */
    {
        const char *q = p;
        if (*q == '+' || *q == '-') {
            q++;
        }
        if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X')) {
            return CBAT_ERR_PARSE;
        }
    }
    value = strtod(p, &end);
    if (end == p) {
        return CBAT_ERR_PARSE;
    }
    while (cbat__string_is_space(*end)) {
        end++;
    }
    if (*end != '\0') {
        return CBAT_ERR_PARSE;
    }
    if (value == HUGE_VAL || value == -HUGE_VAL) {
        /* Only an overflow if the text did not literally say infinity. */
        const char *q = p;
        if (*q == '+' || *q == '-') {
            q++;
        }
        if (*q != 'i' && *q != 'I') {
            return CBAT_ERR_OVERFLOW;
        }
    }
    *out = value;
    return CBAT_OK;
}

/* ---- UTF-8 -------------------------------------------------------------- */

size_t cbat_utf8_decode(const char *text, size_t length, uint32_t *codepoint) {
    const unsigned char *s = (const unsigned char *)text;
    uint32_t cp;
    size_t needed;
    size_t i;
    if (text == NULL || length == 0) {
        return 0;
    }
    if (s[0] < 0x80) {
        cp = s[0];
        needed = 1;
    } else if (s[0] >= 0xC2 && s[0] <= 0xDF) {
        cp = s[0] & 0x1Fu;
        needed = 2;
    } else if (s[0] >= 0xE0 && s[0] <= 0xEF) {
        cp = s[0] & 0x0Fu;
        needed = 3;
    } else if (s[0] >= 0xF0 && s[0] <= 0xF4) {
        cp = s[0] & 0x07u;
        needed = 4;
    } else {
        return 0; /* continuation byte, overlong 2-byte lead, or > U+10FFFF */
    }
    if (needed > length) {
        return 0;
    }
    for (i = 1; i < needed; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            return 0;
        }
        cp = (cp << 6) | (s[i] & 0x3Fu);
    }
    if ((needed == 3 && cp < 0x800) || (needed == 4 && (cp < 0x10000 || cp > 0x10FFFF)) ||
        (cp >= 0xD800 && cp <= 0xDFFF)) {
        return 0;
    }
    if (codepoint != NULL) {
        *codepoint = cp;
    }
    return needed;
}

size_t cbat_utf8_encode(uint32_t codepoint, char out[4]) {
    if (out == NULL || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        return 0;
    }
    if (codepoint < 0x80) {
        out[0] = (char)codepoint;
        return 1;
    }
    if (codepoint < 0x800) {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    }
    if (codepoint < 0x10000) {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (codepoint >> 18));
    out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = (char)(0x80 | (codepoint & 0x3F));
    return 4;
}

CBatError cbat_utf8_count(const char *text, size_t length, size_t *count) {
    size_t i = 0;
    size_t total = 0;
    if ((text == NULL && length > 0) || count == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    while (i < length) {
        size_t n = cbat_utf8_decode(text + i, length - i, NULL);
        if (n == 0) {
            return CBAT_ERR_INVALID_UTF8;
        }
        i += n;
        total++;
    }
    *count = total;
    return CBAT_OK;
}

bool cbat_utf8_is_valid(const char *text, size_t length) {
    size_t count;
    return cbat_utf8_count(text, length, &count) == CBAT_OK;
}
