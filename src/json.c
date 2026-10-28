/*
 * cbat/json.c - JSON DOM: recursive-descent parser (depth-limited) and
 * serializer. Objects are CBatDicts so they keep insertion order.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbat/dict.h"
#include "cbat/file.h"
#include "cbat/list.h"

struct CBatJson {
    CBatJsonType type;
    bool is_integer; /* numbers: `integer` holds the exact value */
    union {
        bool boolean;
        struct {
            double value;
            int64_t integer;
        } number;
        CBatString *string;
        CBatList *array;  /* of CBatJson * */
        CBatDict *object; /* values are CBatJson * */
    } as;
};

static void cbat__json_destructor(void *element) {
    if (element != NULL) {
        cbat_json_free(*(CBatJson **)element);
    }
}

/* ---- Creating ----------------------------------------------------------- */

static CBatJson *cbat__json_alloc(CBatJsonType type) {
    CBatJson *value = (CBatJson *)cbat_alloc_zeroed(NULL, sizeof(CBatJson));
    if (value != NULL) {
        value->type = type;
    }
    return value;
}

CBatJson *cbat_json_new_null(void) {
    return cbat__json_alloc(CBAT_JSON_NULL);
}

CBatJson *cbat_json_new_bool(bool flag) {
    CBatJson *value = cbat__json_alloc(CBAT_JSON_BOOL);
    if (value != NULL) {
        value->as.boolean = flag;
    }
    return value;
}

CBatJson *cbat_json_new_number(double number) {
    CBatJson *value = cbat__json_alloc(CBAT_JSON_NUMBER);
    if (value != NULL) {
        value->as.number.value = number;
    }
    return value;
}

CBatJson *cbat_json_new_int(int64_t integer) {
    CBatJson *value = cbat__json_alloc(CBAT_JSON_NUMBER);
    if (value != NULL) {
        value->is_integer = true;
        value->as.number.integer = integer;
        value->as.number.value = (double)integer;
    }
    return value;
}

CBatJson *cbat_json_new_string_length(const char *text, size_t length) {
    CBatJson *value;
    if (text == NULL) {
        return cbat_json_new_null();
    }
    value = cbat__json_alloc(CBAT_JSON_STRING);
    if (value == NULL) {
        return NULL;
    }
    value->as.string = cbat_string_from_length(text, length);
    if (value->as.string == NULL) {
        cbat_free(value);
        return NULL;
    }
    return value;
}

CBatJson *cbat_json_new_string(const char *text) {
    return cbat_json_new_string_length(text, text == NULL ? 0 : strlen(text));
}

CBatJson *cbat_json_new_array(void) {
    CBatJson *value = cbat__json_alloc(CBAT_JSON_ARRAY);
    if (value == NULL) {
        return NULL;
    }
    value->as.array = cbat_list_new(sizeof(CBatJson *));
    if (value->as.array == NULL) {
        cbat_free(value);
        return NULL;
    }
    cbat_list_set_destructor(value->as.array, cbat__json_destructor);
    return value;
}

CBatJson *cbat_json_new_object(void) {
    CBatJson *value = cbat__json_alloc(CBAT_JSON_OBJECT);
    if (value == NULL) {
        return NULL;
    }
    value->as.object = cbat_dict_new(sizeof(CBatJson *));
    if (value->as.object == NULL) {
        cbat_free(value);
        return NULL;
    }
    cbat_dict_set_destructor(value->as.object, cbat__json_destructor);
    return value;
}

void cbat_json_free(CBatJson *value) {
    if (value == NULL) {
        return;
    }
    switch (value->type) {
    case CBAT_JSON_STRING:
        cbat_string_free(value->as.string);
        break;
    case CBAT_JSON_ARRAY:
        cbat_list_free(value->as.array);
        break;
    case CBAT_JSON_OBJECT:
        cbat_dict_free(value->as.object);
        break;
    default:
        break;
    }
    cbat_free(value);
}

CBatJson *cbat_json_copy(const CBatJson *value) {
    CBatJson *copy;
    size_t i;
    if (value == NULL) {
        return NULL;
    }
    switch (value->type) {
    case CBAT_JSON_STRING:
        return cbat_json_new_string_length(value->as.string->data, value->as.string->length);
    case CBAT_JSON_ARRAY:
        copy = cbat_json_new_array();
        for (i = 0; copy != NULL && i < value->as.array->length; i++) {
            CBatJson *item = cbat_json_copy(cbat_json_array_get(value, i));
            if (item == NULL || cbat_json_array_append(copy, item) != CBAT_OK) {
                cbat_json_free(item);
                cbat_json_free(copy);
                return NULL;
            }
        }
        return copy;
    case CBAT_JSON_OBJECT: {
        size_t it = 0;
        const char *key;
        CBatJson *member;
        copy = cbat_json_new_object();
        while (copy != NULL && cbat_json_object_next(value, &it, &key, &member)) {
            CBatJson *item = cbat_json_copy(member);
            if (item == NULL || cbat_json_object_set(copy, key, item) != CBAT_OK) {
                cbat_json_free(item);
                cbat_json_free(copy);
                return NULL;
            }
        }
        return copy;
    }
    default:
        copy = cbat__json_alloc(value->type);
        if (copy != NULL) {
            *copy = *value;
        }
        return copy;
    }
}

/* ---- Inspecting --------------------------------------------------------- */

CBatJsonType cbat_json_type(const CBatJson *value) {
    return value == NULL ? CBAT_JSON_NULL : value->type;
}

const char *cbat_json_type_name(CBatJsonType type) {
    switch (type) {
    case CBAT_JSON_NULL: return "null";
    case CBAT_JSON_BOOL: return "bool";
    case CBAT_JSON_NUMBER: return "number";
    case CBAT_JSON_STRING: return "string";
    case CBAT_JSON_ARRAY: return "array";
    case CBAT_JSON_OBJECT: return "object";
    default: return "unknown";
    }
}

bool cbat_json_equals(const CBatJson *a, const CBatJson *b) {
    size_t i;
    if (a == NULL || b == NULL) {
        return a == b;
    }
    if (a->type != b->type) {
        return false;
    }
    switch (a->type) {
    case CBAT_JSON_NULL:
        return true;
    case CBAT_JSON_BOOL:
        return a->as.boolean == b->as.boolean;
    case CBAT_JSON_NUMBER:
        if (a->is_integer && b->is_integer) {
            return a->as.number.integer == b->as.number.integer;
        }
        return a->as.number.value == b->as.number.value;
    case CBAT_JSON_STRING:
        return cbat_string_compare(a->as.string, b->as.string) == 0;
    case CBAT_JSON_ARRAY:
        if (a->as.array->length != b->as.array->length) {
            return false;
        }
        for (i = 0; i < a->as.array->length; i++) {
            if (!cbat_json_equals(cbat_json_array_get(a, i), cbat_json_array_get(b, i))) {
                return false;
            }
        }
        return true;
    case CBAT_JSON_OBJECT: {
        size_t it = 0;
        const char *key;
        CBatJson *member;
        if (cbat_dict_length(a->as.object) != cbat_dict_length(b->as.object)) {
            return false;
        }
        while (cbat_json_object_next(a, &it, &key, &member)) {
            CBatJson *other = cbat_json_object_get(b, key);
            if (other == NULL || !cbat_json_equals(member, other)) {
                return false;
            }
        }
        return true;
    }
    default:
        return false;
    }
}

CBatError cbat_json_get_bool(const CBatJson *value, bool *out) {
    if (value == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (value->type != CBAT_JSON_BOOL) {
        return CBAT_ERR_TYPE;
    }
    *out = value->as.boolean;
    return CBAT_OK;
}

CBatError cbat_json_get_number(const CBatJson *value, double *out) {
    if (value == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (value->type != CBAT_JSON_NUMBER) {
        return CBAT_ERR_TYPE;
    }
    *out = value->as.number.value;
    return CBAT_OK;
}

CBatError cbat_json_get_int64(const CBatJson *value, int64_t *out) {
    double d;
    if (value == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (value->type != CBAT_JSON_NUMBER) {
        return CBAT_ERR_TYPE;
    }
    if (value->is_integer) {
        *out = value->as.number.integer;
        return CBAT_OK;
    }
    d = value->as.number.value;
    if (d != floor(d)) {
        return CBAT_ERR_TYPE;
    }
    /* 2^63 is exactly representable; anything >= it does not fit. */
    if (d >= 9223372036854775808.0 || d < -9223372036854775808.0) {
        return CBAT_ERR_OVERFLOW;
    }
    *out = (int64_t)d;
    return CBAT_OK;
}

CBatError cbat_json_get_string(const CBatJson *value, const char **out, size_t *length) {
    if (value == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (value->type != CBAT_JSON_STRING) {
        return CBAT_ERR_TYPE;
    }
    *out = value->as.string->data;
    if (length != NULL) {
        *length = value->as.string->length;
    }
    return CBAT_OK;
}

bool cbat_json_as_bool(const CBatJson *value, bool fallback) {
    bool result;
    return cbat_json_get_bool(value, &result) == CBAT_OK ? result : fallback;
}

double cbat_json_as_number(const CBatJson *value, double fallback) {
    double result;
    return cbat_json_get_number(value, &result) == CBAT_OK ? result : fallback;
}

int64_t cbat_json_as_int64(const CBatJson *value, int64_t fallback) {
    int64_t result;
    return cbat_json_get_int64(value, &result) == CBAT_OK ? result : fallback;
}

const char *cbat_json_as_string(const CBatJson *value, const char *fallback) {
    const char *result;
    return cbat_json_get_string(value, &result, NULL) == CBAT_OK ? result : fallback;
}

/* ---- Arrays ------------------------------------------------------------- */

size_t cbat_json_length(const CBatJson *value) {
    if (value == NULL) {
        return 0;
    }
    if (value->type == CBAT_JSON_ARRAY) {
        return value->as.array->length;
    }
    if (value->type == CBAT_JSON_OBJECT) {
        return cbat_dict_length(value->as.object);
    }
    return 0;
}

CBatJson *cbat_json_array_get(const CBatJson *array, size_t index) {
    CBatJson **slot;
    if (array == NULL || array->type != CBAT_JSON_ARRAY) {
        return NULL;
    }
    slot = (CBatJson **)cbat_list_get(array->as.array, index);
    return slot == NULL ? NULL : *slot;
}

CBatError cbat_json_array_append(CBatJson *array, CBatJson *item) {
    if (array == NULL || item == NULL || item == array) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (array->type != CBAT_JSON_ARRAY) {
        return CBAT_ERR_TYPE;
    }
    return cbat_list_push(array->as.array, &item);
}

CBatError cbat_json_array_remove(CBatJson *array, size_t index) {
    if (array == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (array->type != CBAT_JSON_ARRAY) {
        return CBAT_ERR_TYPE;
    }
    return cbat_list_remove(array->as.array, index, NULL);
}

/* Appends a freshly created item, freeing it if the append fails. */
static CBatError cbat__json_append_new(CBatJson *array, CBatJson *item) {
    CBatError err;
    if (item == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_json_array_append(array, item);
    if (err != CBAT_OK) {
        cbat_json_free(item);
    }
    return err;
}

CBatError cbat_json_array_append_null(CBatJson *array) {
    return cbat__json_append_new(array, cbat_json_new_null());
}

CBatError cbat_json_array_append_bool(CBatJson *array, bool value) {
    return cbat__json_append_new(array, cbat_json_new_bool(value));
}

CBatError cbat_json_array_append_number(CBatJson *array, double value) {
    return cbat__json_append_new(array, cbat_json_new_number(value));
}

CBatError cbat_json_array_append_int(CBatJson *array, int64_t value) {
    return cbat__json_append_new(array, cbat_json_new_int(value));
}

CBatError cbat_json_array_append_string(CBatJson *array, const char *value) {
    if (value == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat__json_append_new(array, cbat_json_new_string(value));
}

/* ---- Objects ------------------------------------------------------------ */

CBatJson *cbat_json_object_get(const CBatJson *object, const char *key) {
    CBatJson **slot;
    if (object == NULL || key == NULL || object->type != CBAT_JSON_OBJECT) {
        return NULL;
    }
    slot = (CBatJson **)cbat_dict_get(object->as.object, key);
    return slot == NULL ? NULL : *slot;
}

CBatError cbat_json_object_set(CBatJson *object, const char *key, CBatJson *value) {
    if (object == NULL || key == NULL || value == NULL || value == object) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (object->type != CBAT_JSON_OBJECT) {
        return CBAT_ERR_TYPE;
    }
    return cbat_dict_set(object->as.object, key, &value);
}

CBatError cbat_json_object_remove(CBatJson *object, const char *key) {
    if (object == NULL || key == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (object->type != CBAT_JSON_OBJECT) {
        return CBAT_ERR_TYPE;
    }
    return cbat_dict_remove(object->as.object, key, NULL);
}

bool cbat_json_object_next(const CBatJson *object, size_t *iterator, const char **key,
                           CBatJson **value) {
    void *slot;
    if (object == NULL || object->type != CBAT_JSON_OBJECT) {
        return false;
    }
    if (!cbat_dict_next(object->as.object, iterator, key, &slot)) {
        return false;
    }
    if (value != NULL) {
        *value = *(CBatJson **)slot;
    }
    return true;
}

static CBatError cbat__json_set_new(CBatJson *object, const char *key, CBatJson *value) {
    CBatError err;
    if (value == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_json_object_set(object, key, value);
    if (err != CBAT_OK) {
        cbat_json_free(value);
    }
    return err;
}

CBatError cbat_json_object_set_null(CBatJson *object, const char *key) {
    return cbat__json_set_new(object, key, cbat_json_new_null());
}

CBatError cbat_json_object_set_bool(CBatJson *object, const char *key, bool value) {
    return cbat__json_set_new(object, key, cbat_json_new_bool(value));
}

CBatError cbat_json_object_set_number(CBatJson *object, const char *key, double value) {
    return cbat__json_set_new(object, key, cbat_json_new_number(value));
}

CBatError cbat_json_object_set_int(CBatJson *object, const char *key, int64_t value) {
    return cbat__json_set_new(object, key, cbat_json_new_int(value));
}

CBatError cbat_json_object_set_string(CBatJson *object, const char *key, const char *value) {
    if (value == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat__json_set_new(object, key, cbat_json_new_string(value));
}

/* ---- JSON Pointer ------------------------------------------------------- */

CBatJson *cbat_json_pointer(const CBatJson *root, const char *pointer) {
    const CBatJson *current = root;
    CBatString *token;
    const char *p = pointer;
    if (root == NULL || pointer == NULL) {
        return NULL;
    }
    if (*p == '\0') {
        return (CBatJson *)root;
    }
    if (*p != '/') {
        return NULL;
    }
    token = cbat_string_new();
    if (token == NULL) {
        return NULL;
    }
    while (current != NULL && *p == '/') {
        p++;
        cbat_string_clear(token);
        while (*p != '\0' && *p != '/') {
            char c = *p++;
            if (c == '~') {
                if (*p == '0') {
                    c = '~';
                } else if (*p == '1') {
                    c = '/';
                } else {
                    current = NULL; /* invalid escape */
                    break;
                }
                p++;
            }
            if (cbat_string_append_char(token, c) != CBAT_OK) {
                current = NULL;
                break;
            }
        }
        if (current == NULL) {
            break;
        }
        if (current->type == CBAT_JSON_OBJECT) {
            current = cbat_json_object_get(current, token->data);
        } else if (current->type == CBAT_JSON_ARRAY) {
            int64_t index;
            bool leading_zero = token->length > 1 && token->data[0] == '0';
            if (leading_zero || token->length == 0 || token->data[0] == '-' ||
                token->data[0] == '+' ||
                cbat_string_to_int64(token->data, &index) != CBAT_OK) {
                current = NULL;
            } else {
                current = cbat_json_array_get(current, (size_t)index);
            }
        } else {
            current = NULL;
        }
    }
    cbat_string_free(token);
    return (CBatJson *)current;
}

/* ---- Parser ------------------------------------------------------------- */

typedef struct cbat__json_parser {
    const char *text;
    size_t length;
    size_t position;
    int depth;
    const char *message;
    size_t error_position;
} cbat__json_parser;

static CBatError cbat__json_fail(cbat__json_parser *p, const char *message) {
    p->message = message;
    p->error_position = p->position;
    return CBAT_ERR_PARSE;
}

static void cbat__json_skip_space(cbat__json_parser *p) {
    while (p->position < p->length) {
        char c = p->text[p->position];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        p->position++;
    }
}

static int cbat__json_peek(const cbat__json_parser *p) {
    return p->position < p->length ? (unsigned char)p->text[p->position] : -1;
}

static int cbat__json_hex4(cbat__json_parser *p, uint32_t *out) {
    uint32_t value = 0;
    int i;
    if (p->length - p->position < 4) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        char c = p->text[p->position + (size_t)i];
        value <<= 4;
        if (c >= '0' && c <= '9') {
            value |= (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            value |= (uint32_t)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            value |= (uint32_t)(c - 'A' + 10);
        } else {
            return 0;
        }
    }
    p->position += 4;
    *out = value;
    return 1;
}

/* Parses a string literal (p->position at the opening quote) into `out`. */
static CBatError cbat__json_parse_string(cbat__json_parser *p, CBatString *out) {
    CBatError err = CBAT_OK;
    p->position++; /* opening quote */
    for (;;) {
        unsigned char c;
        if (p->position >= p->length) {
            return cbat__json_fail(p, "unterminated string");
        }
        c = (unsigned char)p->text[p->position];
        if (c == '"') {
            p->position++;
            return CBAT_OK;
        }
        if (c < 0x20) {
            return cbat__json_fail(p, "control character in string (use an escape)");
        }
        if (c == '\\') {
            char e;
            p->position++;
            if (p->position >= p->length) {
                return cbat__json_fail(p, "unterminated string");
            }
            e = p->text[p->position++];
            switch (e) {
            case '"': err = cbat_string_append_char(out, '"'); break;
            case '\\': err = cbat_string_append_char(out, '\\'); break;
            case '/': err = cbat_string_append_char(out, '/'); break;
            case 'b': err = cbat_string_append_char(out, '\b'); break;
            case 'f': err = cbat_string_append_char(out, '\f'); break;
            case 'n': err = cbat_string_append_char(out, '\n'); break;
            case 'r': err = cbat_string_append_char(out, '\r'); break;
            case 't': err = cbat_string_append_char(out, '\t'); break;
            case 'u': {
                uint32_t cp;
                if (!cbat__json_hex4(p, &cp)) {
                    p->position -= 2;
                    return cbat__json_fail(p, "\\u must be followed by 4 hex digits");
                }
                if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    p->position -= 6;
                    return cbat__json_fail(p, "unpaired low surrogate");
                }
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    uint32_t low;
                    if (p->length - p->position < 6 || p->text[p->position] != '\\' ||
                        p->text[p->position + 1] != 'u') {
                        p->position -= 6;
                        return cbat__json_fail(p, "unpaired high surrogate");
                    }
                    p->position += 2;
                    if (!cbat__json_hex4(p, &low) || low < 0xDC00 || low > 0xDFFF) {
                        p->position -= 8;
                        return cbat__json_fail(p, "unpaired high surrogate");
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                err = cbat_string_append_codepoint(out, cp);
                break;
            }
            default:
                p->position -= 2;
                return cbat__json_fail(p, "invalid escape sequence");
            }
        } else if (c < 0x80) {
            err = cbat_string_append_char(out, (char)c);
            p->position++;
        } else {
            size_t n = cbat_utf8_decode(p->text + p->position, p->length - p->position, NULL);
            if (n == 0) {
                return cbat__json_fail(p, "invalid UTF-8 in string");
            }
            err = cbat_string_append_length(out, p->text + p->position, n);
            p->position += n;
        }
        if (err != CBAT_OK) {
            return err;
        }
    }
}

static CBatError cbat__json_parse_number(cbat__json_parser *p, CBatJson **out) {
    size_t start = p->position;
    bool integral = true;
    char stack_buffer[64];
    char *buffer = stack_buffer;
    size_t length;
    double value;
    CBatJson *number;
    CBatError err;

    if (cbat__json_peek(p) == '-') {
        p->position++;
    }
    if (cbat__json_peek(p) == '0') {
        p->position++;
    } else if (cbat__json_peek(p) >= '1' && cbat__json_peek(p) <= '9') {
        while (cbat__json_peek(p) >= '0' && cbat__json_peek(p) <= '9') {
            p->position++;
        }
    } else {
        return cbat__json_fail(p, "invalid number");
    }
    if (cbat__json_peek(p) == '.') {
        integral = false;
        p->position++;
        if (!(cbat__json_peek(p) >= '0' && cbat__json_peek(p) <= '9')) {
            return cbat__json_fail(p, "expected digits after '.'");
        }
        while (cbat__json_peek(p) >= '0' && cbat__json_peek(p) <= '9') {
            p->position++;
        }
    }
    if (cbat__json_peek(p) == 'e' || cbat__json_peek(p) == 'E') {
        integral = false;
        p->position++;
        if (cbat__json_peek(p) == '+' || cbat__json_peek(p) == '-') {
            p->position++;
        }
        if (!(cbat__json_peek(p) >= '0' && cbat__json_peek(p) <= '9')) {
            return cbat__json_fail(p, "expected digits in exponent");
        }
        while (cbat__json_peek(p) >= '0' && cbat__json_peek(p) <= '9') {
            p->position++;
        }
    }

    length = p->position - start;
    if (length >= sizeof(stack_buffer)) {
        buffer = (char *)cbat_alloc(NULL, length + 1);
        if (buffer == NULL) {
            return CBAT_ERR_NO_MEMORY;
        }
    }
    memcpy(buffer, p->text + start, length);
    buffer[length] = '\0';

    if (integral) {
        int64_t exact;
        if (cbat_string_to_int64(buffer, &exact) == CBAT_OK) {
            if (buffer != stack_buffer) {
                cbat_free(buffer);
            }
            number = cbat_json_new_int(exact);
            if (number == NULL) {
                return CBAT_ERR_NO_MEMORY;
            }
            *out = number;
            return CBAT_OK;
        }
    }
    err = cbat_string_to_double(buffer, &value);
    if (buffer != stack_buffer) {
        cbat_free(buffer);
    }
    if (err == CBAT_ERR_OVERFLOW) {
        p->position = start;
        return cbat__json_fail(p, "number is too large");
    }
    if (err != CBAT_OK) return err;
    number = cbat_json_new_number(value);
    if (number == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    *out = number;
    return CBAT_OK;
}

static CBatError cbat__json_parse_value(cbat__json_parser *p, CBatJson **out);

static CBatError cbat__json_parse_array(cbat__json_parser *p, CBatJson **out) {
    CBatJson *array = cbat_json_new_array();
    CBatError err;
    if (array == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    p->position++; /* '[' */
    cbat__json_skip_space(p);
    if (cbat__json_peek(p) == ']') {
        p->position++;
        *out = array;
        return CBAT_OK;
    }
    for (;;) {
        CBatJson *item = NULL;
        cbat__json_skip_space(p);
        err = cbat__json_parse_value(p, &item);
        if (err == CBAT_OK) {
            err = cbat_json_array_append(array, item);
            if (err != CBAT_OK) {
                cbat_json_free(item);
            }
        }
        if (err != CBAT_OK) {
            cbat_json_free(array);
            return err;
        }
        cbat__json_skip_space(p);
        if (cbat__json_peek(p) == ',') {
            p->position++;
            continue;
        }
        if (cbat__json_peek(p) == ']') {
            p->position++;
            *out = array;
            return CBAT_OK;
        }
        cbat_json_free(array);
        return cbat__json_fail(p, "expected ',' or ']' in array");
    }
}

static CBatError cbat__json_parse_object(cbat__json_parser *p, CBatJson **out) {
    CBatJson *object = cbat_json_new_object();
    CBatString *key = cbat_string_new();
    CBatError err = CBAT_OK;
    if (object == NULL || key == NULL) {
        cbat_json_free(object);
        cbat_string_free(key);
        return CBAT_ERR_NO_MEMORY;
    }
    p->position++; /* '{' */
    cbat__json_skip_space(p);
    if (cbat__json_peek(p) == '}') {
        p->position++;
        cbat_string_free(key);
        *out = object;
        return CBAT_OK;
    }
    for (;;) {
        CBatJson *member = NULL;
        size_t key_start;
        cbat__json_skip_space(p);
        if (cbat__json_peek(p) != '"') {
            err = cbat__json_fail(p, "expected a string key");
            break;
        }
        key_start = p->position;
        cbat_string_clear(key);
        err = cbat__json_parse_string(p, key);
        if (err != CBAT_OK) {
            break;
        }
        if (memchr(key->data, '\0', key->length) != NULL) {
            p->position = key_start;
            err = cbat__json_fail(p, "object keys may not contain \\u0000");
            break;
        }
        cbat__json_skip_space(p);
        if (cbat__json_peek(p) != ':') {
            err = cbat__json_fail(p, "expected ':' after key");
            break;
        }
        p->position++;
        cbat__json_skip_space(p);
        err = cbat__json_parse_value(p, &member);
        if (err != CBAT_OK) {
            break;
        }
        err = cbat_json_object_set(object, key->data, member); /* last duplicate wins */
        if (err != CBAT_OK) {
            cbat_json_free(member);
            break;
        }
        cbat__json_skip_space(p);
        if (cbat__json_peek(p) == ',') {
            p->position++;
            continue;
        }
        if (cbat__json_peek(p) == '}') {
            p->position++;
            break;
        }
        err = cbat__json_fail(p, "expected ',' or '}' in object");
        break;
    }
    cbat_string_free(key);
    if (err != CBAT_OK) {
        cbat_json_free(object);
        return err;
    }
    *out = object;
    return CBAT_OK;
}

static bool cbat__json_match(cbat__json_parser *p, const char *word) {
    size_t n = strlen(word);
    if (p->length - p->position >= n && memcmp(p->text + p->position, word, n) == 0) {
        p->position += n;
        return true;
    }
    return false;
}

static CBatError cbat__json_parse_value(cbat__json_parser *p, CBatJson **out) {
    int c = cbat__json_peek(p);
    CBatError err;
    CBatJson *value;
    switch (c) {
    case '{':
    case '[':
        if (++p->depth > CBAT_JSON_MAX_DEPTH) {
            return cbat__json_fail(p, "nesting is too deep");
        }
        err = c == '{' ? cbat__json_parse_object(p, out) : cbat__json_parse_array(p, out);
        p->depth--;
        return err;
    case '"': {
        CBatString *text = cbat_string_new();
        if (text == NULL) {
            return CBAT_ERR_NO_MEMORY;
        }
        err = cbat__json_parse_string(p, text);
        if (err != CBAT_OK) {
            cbat_string_free(text);
            return err;
        }
        value = cbat__json_alloc(CBAT_JSON_STRING);
        if (value == NULL) {
            cbat_string_free(text);
            return CBAT_ERR_NO_MEMORY;
        }
        value->as.string = text;
        *out = value;
        return CBAT_OK;
    }
    case 't':
    case 'f':
    case 'n':
        if (cbat__json_match(p, "true")) {
            value = cbat_json_new_bool(true);
        } else if (cbat__json_match(p, "false")) {
            value = cbat_json_new_bool(false);
        } else if (cbat__json_match(p, "null")) {
            value = cbat_json_new_null();
        } else {
            return cbat__json_fail(p, "invalid literal");
        }
        if (value == NULL) {
            return CBAT_ERR_NO_MEMORY;
        }
        *out = value;
        return CBAT_OK;
    case -1:
        return cbat__json_fail(p, "unexpected end of input");
    default:
        if (c == '-' || (c >= '0' && c <= '9')) {
            return cbat__json_parse_number(p, out);
        }
        return cbat__json_fail(p, "unexpected character");
    }
}

static void cbat__json_fill_info(const cbat__json_parser *p, CBatParseInfo *info) {
    size_t i;
    size_t line = 1;
    size_t column = 1;
    if (info == NULL) {
        return;
    }
    for (i = 0; i < p->error_position && i < p->length; i++) {
        if (p->text[i] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    info->line = line;
    info->column = column;
    info->offset = p->error_position;
    info->message = p->message;
}

CBatError cbat_json_parse(const char *text, size_t length, CBatJson **out, CBatParseInfo *info) {
    cbat__json_parser p;
    CBatJson *value = NULL;
    CBatError err;
    if ((text == NULL && length > 0) || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    p.text = text;
    p.length = length;
    p.position = 0;
    p.depth = 0;
    p.message = NULL;
    p.error_position = 0;
    /* Tolerate a UTF-8 byte-order mark. */
    if (length >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0) {
        p.position = 3;
    }
    cbat__json_skip_space(&p);
    err = cbat__json_parse_value(&p, &value);
    if (err == CBAT_OK) {
        cbat__json_skip_space(&p);
        if (p.position < p.length) {
            cbat_json_free(value);
            err = cbat__json_fail(&p, "unexpected text after the JSON value");
        }
    }
    if (err == CBAT_ERR_PARSE) {
        cbat__json_fill_info(&p, info);
    }
    if (err != CBAT_OK) {
        return err;
    }
    *out = value;
    return CBAT_OK;
}

CBatError cbat_json_parse_file(const char *path, CBatJson **out, CBatParseInfo *info) {
    CBatString *text = NULL;
    CBatError err = cbat_file_read(path, &text);
    if (err != CBAT_OK) {
        return err;
    }
    err = cbat_json_parse(text->data, text->length, out, info);
    cbat_string_free(text);
    return err;
}

/* ---- Serializer --------------------------------------------------------- */

static CBatError cbat__json_write_string(CBatString *out, const char *text, size_t length) {
    static const char hex[] = "0123456789abcdef";
    CBatError err = cbat_string_append_char(out, '"');
    size_t i;
    for (i = 0; i < length && err == CBAT_OK; i++) {
        unsigned char c = (unsigned char)text[i];
        switch (c) {
        case '"': err = cbat_string_append(out, "\\\""); break;
        case '\\': err = cbat_string_append(out, "\\\\"); break;
        case '\b': err = cbat_string_append(out, "\\b"); break;
        case '\f': err = cbat_string_append(out, "\\f"); break;
        case '\n': err = cbat_string_append(out, "\\n"); break;
        case '\r': err = cbat_string_append(out, "\\r"); break;
        case '\t': err = cbat_string_append(out, "\\t"); break;
        default:
            if (c < 0x20) {
                char escape[7] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 0x0F], '\0'};
                err = cbat_string_append(out, escape);
            } else {
                err = cbat_string_append_char(out, (char)c);
            }
        }
    }
    if (err == CBAT_OK) {
        err = cbat_string_append_char(out, '"');
    }
    return err;
}

static CBatError cbat__json_write_number(CBatString *out, const CBatJson *value) {
    char buffer[21];
    double d = value->as.number.value;
    if (value->is_integer) {
        size_t length = cbat_format_int64(value->as.number.integer, buffer);
        return cbat_string_append_length(out, buffer, length);
    }
    if (d != d || d == HUGE_VAL || d == -HUGE_VAL) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    return cbat_string_append_double(out, d);
}

static CBatError cbat__json_newline(CBatString *out, int indent, int depth) {
    CBatError err;
    int i;
    if (indent <= 0) {
        return CBAT_OK;
    }
    err = cbat_string_append_char(out, '\n');
    for (i = 0; i < indent * depth && err == CBAT_OK; i++) {
        err = cbat_string_append_char(out, ' ');
    }
    return err;
}

static CBatError cbat__json_write(CBatString *out, const CBatJson *value, int indent, int depth) {
    CBatError err = CBAT_OK;
    size_t i;
    switch (value->type) {
    case CBAT_JSON_NULL:
        return cbat_string_append(out, "null");
    case CBAT_JSON_BOOL:
        return cbat_string_append(out, value->as.boolean ? "true" : "false");
    case CBAT_JSON_NUMBER:
        return cbat__json_write_number(out, value);
    case CBAT_JSON_STRING:
        return cbat__json_write_string(out, value->as.string->data, value->as.string->length);
    case CBAT_JSON_ARRAY:
        if (value->as.array->length == 0) {
            return cbat_string_append(out, "[]");
        }
        err = cbat_string_append_char(out, '[');
        for (i = 0; i < value->as.array->length && err == CBAT_OK; i++) {
            if (i > 0) {
                err = cbat_string_append_char(out, ',');
            }
            if (err == CBAT_OK) {
                err = cbat__json_newline(out, indent, depth + 1);
            }
            if (err == CBAT_OK) {
                err = cbat__json_write(out, cbat_json_array_get(value, i), indent, depth + 1);
            }
        }
        if (err == CBAT_OK) {
            err = cbat__json_newline(out, indent, depth);
        }
        if (err == CBAT_OK) {
            err = cbat_string_append_char(out, ']');
        }
        return err;
    case CBAT_JSON_OBJECT: {
        size_t it = 0;
        const char *key;
        CBatJson *member;
        bool first = true;
        if (cbat_dict_length(value->as.object) == 0) {
            return cbat_string_append(out, "{}");
        }
        err = cbat_string_append_char(out, '{');
        while (err == CBAT_OK && cbat_json_object_next(value, &it, &key, &member)) {
            if (!first) {
                err = cbat_string_append_char(out, ',');
            }
            first = false;
            if (err == CBAT_OK) {
                err = cbat__json_newline(out, indent, depth + 1);
            }
            if (err == CBAT_OK) {
                err = cbat__json_write_string(out, key, strlen(key));
            }
            if (err == CBAT_OK) {
                err = cbat_string_append(out, indent > 0 ? ": " : ":");
            }
            if (err == CBAT_OK) {
                err = cbat__json_write(out, member, indent, depth + 1);
            }
        }
        if (err == CBAT_OK) {
            err = cbat__json_newline(out, indent, depth);
        }
        if (err == CBAT_OK) {
            err = cbat_string_append_char(out, '}');
        }
        return err;
    }
    default:
        return CBAT_ERR_INVALID_ARGUMENT;
    }
}

CBatError cbat_json_to_string(const CBatJson *value, int indent, CBatString **out) {
    CBatString *text;
    CBatError err;
    if (value == NULL || out == NULL || indent < 0) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    text = cbat_string_new();
    if (text == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat__json_write(text, value, indent, 0);
    if (err != CBAT_OK) {
        cbat_string_free(text);
        return err;
    }
    *out = text;
    return CBAT_OK;
}

CBatError cbat_json_write_file(const char *path, const CBatJson *value, int indent) {
    CBatString *text = NULL;
    CBatError err = cbat_json_to_string(value, indent, &text);
    if (err != CBAT_OK) {
        return err;
    }
    if (indent > 0) {
        err = cbat_string_append_char(text, '\n');
    }
    if (err == CBAT_OK) {
        err = cbat_file_write(path, text->data, text->length);
    }
    cbat_string_free(text);
    return err;
}
