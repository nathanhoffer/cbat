/*
 * cbat/string.c - CBatString, number parsing and UTF-8.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/string.h"

#include <math.h>
#include <locale.h>
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

static CBatError cbat__string_pad(CBatString *s, size_t width, char fill, int side) {
    size_t left, padding, original;
    CBatError err;
    if (s == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (width <= s->length) return CBAT_OK;
    original = s->length;
    padding = width - original;
    left = side < 0 ? padding : side > 0 ? 0 : padding / 2;
    err = cbat__string_ensure(s, width);
    if (err != CBAT_OK) return err;
    memmove(s->data + left, s->data, original);
    memset(s->data, (unsigned char)fill, left);
    memset(s->data + left + original, (unsigned char)fill, padding - left);
    s->length = width;
    s->data[width] = '\0';
    return CBAT_OK;
}

CBatError cbat_string_pad_left(CBatString *s, size_t width, char fill) { return cbat__string_pad(s, width, fill, -1); }
CBatError cbat_string_pad_right(CBatString *s, size_t width, char fill) { return cbat__string_pad(s, width, fill, 1); }
CBatError cbat_string_center(CBatString *s, size_t width, char fill) { return cbat__string_pad(s, width, fill, 0); }

void cbat_string_strip_chars(CBatString *s, const char *characters) {
    size_t left = 0, right;
    if (s == NULL || characters == NULL) return;
    right = s->length;
    while (left < right && s->data[left] != '\0' && strchr(characters, s->data[left]) != NULL) left++;
    while (right > left && s->data[right - 1] != '\0' && strchr(characters, s->data[right - 1]) != NULL) right--;
    /* Compute bounds before modifying anything: characters may alias s. */
    memmove(s->data, s->data + left, right - left);
    s->length = right - left;
    s->data[s->length] = '\0';
}

CBatError cbat_string_remove_prefix(CBatString *s, const char *prefix) {
    if (s == NULL || prefix == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    return cbat_string_starts_with(s, prefix) ? cbat_string_erase(s, 0, strlen(prefix)) : CBAT_OK;
}

CBatError cbat_string_remove_suffix(CBatString *s, const char *suffix) {
    if (s == NULL || suffix == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (cbat_string_ends_with(s, suffix)) cbat_string_truncate(s, s->length - strlen(suffix));
    return CBAT_OK;
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

static unsigned char cbat__string_ascii_lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

bool cbat_string_starts_with_ignore_case(const CBatString *s, const char *prefix) {
    size_t i = 0;
    if (s == NULL || prefix == NULL) return false;
    while (prefix[i] != '\0') {
        if (i == s->length || cbat__string_ascii_lower((unsigned char)s->data[i]) !=
            cbat__string_ascii_lower((unsigned char)prefix[i])) return false;
        i++;
    }
    return true;
}

bool cbat_string_equals_ignore_case(const CBatString *s, const char *text) {
    return s != NULL && text != NULL && strlen(text) == s->length &&
        cbat_string_starts_with_ignore_case(s, text);
}

bool cbat_string_is_ascii_digits(const char *text) {
    if (text == NULL || *text == '\0') return false;
    while (*text != '\0') { if (*text < '0' || *text > '9') return false; text++; }
    return true;
}

int cbat_compare_natural(const char *a, const char *b) {
    if (a == NULL || b == NULL) return (a != NULL) - (b != NULL);
    while (*a != '\0' && *b != '\0') {
        if (*a >= '0' && *a <= '9' && *b >= '0' && *b <= '9') {
            const char *a_start = a, *b_start = b, *a_digits, *b_digits;
            size_t a_length, b_length, a_run, b_run;
            int order;
            while (*a == '0') a++;
            while (*b == '0') b++;
            a_digits = a; b_digits = b;
            while (*a >= '0' && *a <= '9') a++;
            while (*b >= '0' && *b <= '9') b++;
            a_length = (size_t)(a - a_digits); b_length = (size_t)(b - b_digits);
            if (a_length != b_length) return a_length < b_length ? -1 : 1;
            order = memcmp(a_digits, b_digits, a_length);
            if (order != 0) return order < 0 ? -1 : 1;
            a_run = (size_t)(a - a_start); b_run = (size_t)(b - b_start);
            if (a_run != b_run) return a_run < b_run ? -1 : 1;
        } else {
            unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
            if (x != y) return x < y ? -1 : 1;
        }
    }
    return (*a != '\0') - (*b != '\0');
}

int cbat_compare_natural_elements(const void *a, const void *b) {
    return cbat_compare_natural(a == NULL ? NULL : *(const char *const *)a,
                               b == NULL ? NULL : *(const char *const *)b);
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

CBatError cbat_string_split_max(const char *text, const char *separator, size_t max_splits, CBatList **out) {
    CBatList *list;
    size_t text_length;
    size_t separator_length;
    size_t position = 0;
    size_t splits = 0;
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
        size_t hit = splits < max_splits ?
            cbat__string_search(text, text_length, separator, separator_length, position) : CBAT_NPOS;
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
        splits++;
    }
    *out = list;
    return CBAT_OK;
}

CBatError cbat_string_split(const char *text, const char *separator, CBatList **out) {
    return cbat_string_split_max(text, separator, SIZE_MAX, out);
}

CBatError cbat_string_split_lines(const char *text, CBatList **out) {
    CBatList *list;
    size_t position = 0;
    if (text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    list = cbat__string_list_new();
    if (list == NULL) return CBAT_ERR_NO_MEMORY;
    while (text[position] != '\0') {
        size_t start = position;
        CBatError err;
        while (text[position] != '\0' && text[position] != '\n' && text[position] != '\r') position++;
        err = cbat__string_list_push(list, text + start, position - start);
        if (err != CBAT_OK) { cbat_list_free(list); return err; }
        if (text[position] == '\r') { position++; if (text[position] == '\n') position++; }
        else if (text[position] == '\n') position++;
    }
    *out = list;
    return CBAT_OK;
}

CBatError cbat_string_partition(const char *text, const char *separator, CBatString **before, CBatString **after) {
    const char *hit;
    CBatString *left, *right;
    if (text == NULL || separator == NULL || separator[0] == '\0' ||
        before == NULL || after == NULL || before == after) return CBAT_ERR_INVALID_ARGUMENT;
    hit = strstr(text, separator);
    if (hit == NULL) return CBAT_ERR_NOT_FOUND;
    left = cbat_string_from_length(text, (size_t)(hit - text));
    if (left == NULL) return CBAT_ERR_NO_MEMORY;
    right = cbat_string_from(hit + strlen(separator));
    if (right == NULL) { cbat_string_free(left); return CBAT_ERR_NO_MEMORY; }
    *before = left; *after = right;
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

static bool cbat__number_word(const char *a, const char *end, const char *word) {
    while (a < end && *word != '\0') {
        char c = *a++;
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        if (c != *word++) return false;
    }
    return a == end && *word == '\0';
}

CBatError cbat_string_to_double(const char *text, double *out) {
    const char *start, *end, *p, *point_at = NULL;
    const char *point;
    const struct lconv *conv;
    size_t digits = 0, point_length, length, bytes;
    bool negative = false;
    char small[128], *localized = small, *after;
    double value;
    if (text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    start = text;
    while (cbat__string_is_space(*start)) start++;
    end = start + strlen(start);
    while (end > start && cbat__string_is_space(end[-1])) end--;
    p = start;
    if (p < end && (*p == '-' || *p == '+')) { negative = *p == '-'; p++; }
    if (cbat__number_word(p, end, "inf") || cbat__number_word(p, end, "infinity")) {
        *out = negative ? -HUGE_VAL : HUGE_VAL;
        return CBAT_OK;
    }
    if (cbat__number_word(p, end, "nan")) { *out = negative ? -NAN : NAN; return CBAT_OK; }
    while (p < end && *p >= '0' && *p <= '9') { digits++; p++; }
    if (p < end && *p == '.') {
        point_at = p++;
        while (p < end && *p >= '0' && *p <= '9') { digits++; p++; }
    }
    if (digits == 0) return CBAT_ERR_PARSE;
    if (p < end && (*p == 'e' || *p == 'E')) {
        const char *exponent;
        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        exponent = p;
        while (p < end && *p >= '0' && *p <= '9') p++;
        if (p == exponent) return CBAT_ERR_PARSE;
    }
    if (p != end) return CBAT_ERR_PARSE;
    /* Validate ASCII grammar first, then translate only the decimal point for
     * strtod. No setlocale calls, including transient process-wide changes.
     * Locale decimal points may contain more than one byte. */
    conv = localeconv();
    point = conv != NULL && conv->decimal_point != NULL && conv->decimal_point[0] != '\0' ?
        conv->decimal_point : ".";
    if (point_at == NULL || strcmp(point, ".") == 0) {
        value = strtod(start, &after);
        if (after != end) return CBAT_ERR_PARSE;
    } else {
        point_length = strlen(point);
        length = (size_t)(end - start);
        if (!cbat_size_add(length, point_length, &bytes)) return CBAT_ERR_OVERFLOW;
        if (bytes > sizeof(small)) {
            localized = (char *)cbat_alloc(NULL, bytes);
            if (localized == NULL) return CBAT_ERR_NO_MEMORY;
        }
        memcpy(localized, start, (size_t)(point_at - start));
        memcpy(localized + (point_at - start), point, point_length);
        memcpy(localized + (point_at - start) + point_length, point_at + 1, (size_t)(end - point_at - 1));
        localized[bytes - 1] = '\0';
        value = strtod(localized, &after);
        {
            bool consumed = after == localized + bytes - 1;
            if (localized != small) cbat_free(localized);
            if (!consumed) return CBAT_ERR_PARSE;
        }
    }
    if (value == HUGE_VAL || value == -HUGE_VAL) return CBAT_ERR_OVERFLOW;
    *out = value;
    return CBAT_OK;
}

CBatError cbat_string_to_uint64(const char *text, uint64_t *out) {
    const char *p = text;
    uint64_t value = 0;
    bool any = false, overflow = false;
    if (text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    while (cbat__string_is_space(*p)) p++;
    if (*p == '+') p++;
    while (*p >= '0' && *p <= '9') {
        unsigned digit = (unsigned)(*p++ - '0');
        if (value > (UINT64_MAX - digit) / 10) overflow = true;
        if (!overflow) value = value * 10 + digit;
        any = true;
    }
    while (cbat__string_is_space(*p)) p++;
    if (!any || *p != '\0') return CBAT_ERR_PARSE;
    if (overflow) return CBAT_ERR_OVERFLOW;
    *out = value;
    return CBAT_OK;
}

size_t cbat_format_int64(int64_t value, char out[21]) {
    char reverse[20];
    size_t count = 0, position = 0;
    uint64_t magnitude = value < 0 ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
    if (out == NULL) return 0;
    do { reverse[count++] = (char)('0' + magnitude % 10); magnitude /= 10; } while (magnitude != 0);
    if (value < 0) out[position++] = '-';
    while (count != 0) out[position++] = reverse[--count];
    out[position] = '\0';
    return position;
}

/* Turn an integer significand times 10^power into its shorter conventional
 * decimal notation. At most 17 significant digits and a binary64 exponent. */
static size_t cbat__double_notation(uint64_t significand, int power, bool negative, char out[32]) {
    char digits[21], scientific[32], fixed[32], exponent[21];
    size_t count = cbat_format_int64((int64_t)significand, digits);
    int decimal_exponent = power + (int)count - 1;
    size_t n = 0, i;
    int point;
    while (count > 1 && digits[count - 1] == '0') count--;
    if (negative) scientific[n++] = '-';
    scientific[n++] = digits[0];
    if (count > 1) {
        scientific[n++] = '.';
        for (i = 1; i < count; i++) scientific[n++] = digits[i];
    }
    if (decimal_exponent != 0) {
        size_t e = cbat_format_int64(decimal_exponent, exponent);
        scientific[n++] = 'e';
        memcpy(scientific + n, exponent, e); n += e;
    }
    scientific[n] = '\0';
    point = decimal_exponent + 1;
    {
        size_t fixed_length = (negative ? 1u : 0u) +
            (point <= 0 ? 2 + (size_t)(-point) + count :
             point >= (int)count ? (size_t)point : count + 1);
        if (fixed_length <= n) {
            size_t pos = 0;
            if (negative) fixed[pos++] = '-';
            if (point <= 0) {
                fixed[pos++] = '0'; fixed[pos++] = '.';
                for (i = 0; i < (size_t)(-point); i++) fixed[pos++] = '0';
                memcpy(fixed + pos, digits, count); pos += count;
            } else {
                for (i = 0; i < count || i < (size_t)point; i++) {
                    if (i == (size_t)point && i < count) fixed[pos++] = '.';
                    fixed[pos++] = i < count ? digits[i] : '0';
                }
            }
            fixed[pos] = '\0';
            memcpy(out, fixed, pos + 1);
            return pos;
        }
    }
    memcpy(out, scientific, n + 1);
    return n;
}

size_t cbat_format_double(double value, char out[32]) {
    char raw[128], candidate[64], digits[21], exponent[21], best[32];
    int precision;
    bool negative = signbit(value) != 0;
    if (out == NULL) return 0;
    if (isnan(value)) { memcpy(out, "nan", 4); return 3; }
    if (isinf(value)) { memcpy(out, negative ? "-inf" : "inf", negative ? 5 : 4); return negative ? 4 : 3; }
    if (value == 0) { memcpy(out, negative ? "-0" : "0", negative ? 3 : 2); return negative ? 2 : 1; }
    for (precision = 1; precision <= 17; precision++) {
        const char *e, *p;
        uint64_t significand = 0;
        int power, adjustment;
        size_t best_length = SIZE_MAX;
        int written = snprintf(raw, sizeof(raw), "%.*e", precision - 1, fabs(value));
        if (written < 0 || (size_t)written >= sizeof(raw)) continue;
        e = strrchr(raw, 'e');
        if (e == NULL) continue;
        power = (int)strtol(e + 1, NULL, 10) - (precision - 1);
        for (p = raw; p < e; p++) {
            if (*p >= '0' && *p <= '9') significand = significand * 10 + (unsigned)(*p - '0');
        }
        /* Check the nearest decimal grid point and its neighbors. This also
         * covers the asymmetric rounding interval at powers of two. Parsing
         * integer-significand exponent form needs no locale decimal point. */
        for (adjustment = 0; adjustment < 3; adjustment++) {
            uint64_t trial = adjustment == 1 ? significand - 1 : adjustment == 2 ? significand + 1 : significand;
            size_t pos = 0, length, exp_length;
            double parsed;
            if (trial == 0) continue;
            length = cbat_format_int64((int64_t)trial, digits);
            exp_length = cbat_format_int64(power, exponent);
            if (negative) candidate[pos++] = '-';
            memcpy(candidate + pos, digits, length); pos += length;
            candidate[pos++] = 'e';
            memcpy(candidate + pos, exponent, exp_length + 1);
            parsed = strtod(candidate, NULL);
            if (parsed == value) {
                char rendered[32];
                size_t rendered_length = cbat__double_notation(trial, power, negative, rendered);
                if (rendered_length < best_length) {
                    memcpy(best, rendered, rendered_length + 1);
                    best_length = rendered_length;
                }
            }
        }
        if (best_length != SIZE_MAX) { memcpy(out, best, best_length + 1); return best_length; }
    }
    out[0] = '\0';
    return 0;
}

CBatError cbat_string_append_double(CBatString *s, double value) {
    char buffer[32];
    size_t length;
    if (s == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    length = cbat_format_double(value, buffer);
    return length == 0 ? CBAT_ERR_UNSUPPORTED : cbat_string_append_length(s, buffer, length);
}

CBatError cbat_string_append_int64_grouped(CBatString *s, int64_t value, char separator) {
    char plain[21], grouped[27];
    size_t length, i, pos = 0, start;
    if (s == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    length = cbat_format_int64(value, plain);
    start = plain[0] == '-' ? 1 : 0;
    if (start != 0) grouped[pos++] = '-';
    for (i = start; i < length; i++) {
        if (separator != 0 && i > start && (length - i) % 3 == 0) grouped[pos++] = separator;
        grouped[pos++] = plain[i];
    }
    return cbat_string_append_length(s, grouped, pos);
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

CBatError cbat_utf8_reverse(const char *text, CBatString **out) {
    CBatString *result;
    size_t length, position = 0, count;
    CBatError err;
    if (text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    length = strlen(text);
    err = cbat_utf8_count(text, length, &count);
    if (err != CBAT_OK) return err;
    result = cbat_string_new();
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat_string_reserve(result, length);
    if (err != CBAT_OK) { cbat_string_free(result); return err; }
    while (position < length) {
        size_t used = cbat_utf8_decode(text + position, length - position, NULL);
        memcpy(result->data + length - position - used, text + position, used);
        position += used;
    }
    result->length = length;
    result->data[length] = '\0';
    *out = result;
    return CBAT_OK;
}

CBatError cbat_utf8_substring(const char *text, size_t start, size_t count, CBatString **out) {
    size_t total, length, position = 0, begin, i;
    CBatString *result;
    CBatError err;
    if (text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    length = strlen(text);
    err = cbat_utf8_count(text, length, &total);
    if (err != CBAT_OK) return err;
    start = CBAT_MIN(start, total);
    count = CBAT_MIN(count, total - start);
    for (i = 0; i < start; i++) position += cbat_utf8_decode(text + position, length - position, NULL);
    begin = position;
    for (i = 0; i < count; i++) position += cbat_utf8_decode(text + position, length - position, NULL);
    result = cbat_string_from_length(text + begin, position - begin);
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    *out = result;
    return CBAT_OK;
}
