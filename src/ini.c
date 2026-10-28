/* Ordered INI parsing and serialization. Part of cbat. MIT License. */
#include "cbat/ini.h"
#include "cbat/dict.h"
#include "cbat/file.h"
#include <string.h>

struct CBatIni { CBatDict *sections; const CBatAllocator *allocator; };
static bool cbat__ini_space(char c) { return c == ' ' || c == '\t'; }
static bool cbat__ini_comment(char c) { return c == ';' || c == '#'; }

static void cbat__ini_destroy_section(void *slot) { cbat_dict_free(*(CBatDict **)slot); }

CBatIni *cbat_ini_new_with(const CBatAllocator *allocator) {
    CBatIni *ini;
    if (allocator == NULL) allocator = cbat_default_allocator();
    ini = (CBatIni *)cbat_alloc(allocator, sizeof(CBatIni));
    if (ini == NULL) return NULL;
    ini->allocator = allocator;
    ini->sections = cbat_dict_new_with(sizeof(CBatDict *), allocator);
    if (ini->sections == NULL) { cbat_release(allocator, ini); return NULL; }
    cbat_dict_set_destructor(ini->sections, cbat__ini_destroy_section);
    return ini;
}

CBatIni *cbat_ini_new(void) { return cbat_ini_new_with(NULL); }

void cbat_ini_free(CBatIni *ini) {
    if (ini == NULL) return;
    cbat_dict_free(ini->sections);
    cbat_release(ini->allocator, ini);
}

static CBatDict *cbat__ini_section(const CBatIni *ini, const char *section) {
    CBatDict **slot;
    if (ini == NULL || section == NULL) return NULL;
    slot = (CBatDict **)cbat_dict_get(ini->sections, section);
    return slot == NULL ? NULL : *slot;
}

static bool cbat__ini_valid_name(const char *name, bool section) {
    size_t i, length;
    if (name == NULL) return false;
    length = strlen(name);
    if (length == 0) return section;
    if (cbat__ini_space(name[0]) || cbat__ini_space(name[length - 1])) return false;
    if (!section && cbat__ini_comment(name[0])) return false;
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if ((c < 32 && c != '\t') || c == 127 || c == '[' || c == ']' ||
            (!section && (c == '=' || c == ':'))) return false;
    }
    return true;
}

static bool cbat__ini_valid_value(const char *value) {
    const unsigned char *p = (const unsigned char *)value;
    if (value == NULL) return false;
    for (; *p != '\0'; p++) if ((*p < 32 && *p != '\t') || *p == 127) return false;
    return true;
}

static CBatDict *cbat__ini_new_section(CBatIni *ini) {
    CBatDict *section = cbat_dict_new_with(sizeof(CBatString *), ini->allocator);
    if (section != NULL) cbat_dict_set_destructor(section, cbat_string_destructor);
    return section;
}

static CBatError cbat__ini_ensure_section(CBatIni *ini, const char *name) {
    CBatDict *section;
    CBatError err;
    if (cbat__ini_section(ini, name) != NULL) return CBAT_OK;
    section = cbat__ini_new_section(ini);
    if (section == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat_dict_set(ini->sections, name, &section);
    if (err != CBAT_OK) cbat_dict_free(section);
    return err;
}

CBatError cbat_ini_set(CBatIni *ini, const char *section, const char *key, const char *value) {
    CBatDict *target;
    CBatString *copy;
    CBatError err;
    bool fresh;
    if (ini == NULL || !cbat__ini_valid_name(section, true) ||
        !cbat__ini_valid_name(key, false) || !cbat__ini_valid_value(value)) return CBAT_ERR_INVALID_ARGUMENT;
    copy = cbat_string_new_with(ini->allocator);
    if (copy == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat_string_append(copy, value);
    if (err != CBAT_OK) { cbat_string_free(copy); return err; }
    target = cbat__ini_section(ini, section);
    fresh = target == NULL;
    if (fresh) target = cbat__ini_new_section(ini);
    if (target == NULL) { cbat_string_free(copy); return CBAT_ERR_NO_MEMORY; }
    err = cbat_dict_set(target, key, &copy);
    if (err != CBAT_OK) {
        cbat_string_free(copy);
        if (fresh) cbat_dict_free(target);
        return err;
    }
    if (fresh) {
        err = cbat_dict_set(ini->sections, section, &target);
        if (err != CBAT_OK) cbat_dict_free(target);
    }
    return err;
}

const char *cbat_ini_get(const CBatIni *ini, const char *section, const char *key) {
    CBatDict *target = cbat__ini_section(ini, section);
    CBatString **value = target == NULL ? NULL : (CBatString **)cbat_dict_get(target, key);
    return value == NULL ? NULL : (*value)->data;
}

CBatError cbat_ini_get_int(const CBatIni *ini, const char *section, const char *key, int64_t *out) {
    const char *value;
    if (ini == NULL || section == NULL || key == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    value = cbat_ini_get(ini, section, key);
    return value == NULL ? CBAT_ERR_NOT_FOUND : cbat_string_to_int64(value, out);
}

CBatError cbat_ini_get_double(const CBatIni *ini, const char *section, const char *key, double *out) {
    const char *value;
    if (ini == NULL || section == NULL || key == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    value = cbat_ini_get(ini, section, key);
    return value == NULL ? CBAT_ERR_NOT_FOUND : cbat_string_to_double(value, out);
}

static bool cbat__ini_word(const char *start, const char *end, const char *word) {
    while (start < end && *word != '\0') {
        char c = *start++;
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        if (c != *word++) return false;
    }
    return start == end && *word == '\0';
}

CBatError cbat_ini_get_bool(const CBatIni *ini, const char *section, const char *key, bool *out) {
    const char *value, *end;
    if (ini == NULL || section == NULL || key == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    value = cbat_ini_get(ini, section, key);
    if (value == NULL) return CBAT_ERR_NOT_FOUND;
    while (cbat__ini_space(*value)) value++;
    end = value + strlen(value);
    while (end > value && cbat__ini_space(end[-1])) end--;
    if (cbat__ini_word(value, end, "true") || cbat__ini_word(value, end, "yes") ||
        cbat__ini_word(value, end, "on") || cbat__ini_word(value, end, "1")) { *out = true; return CBAT_OK; }
    if (cbat__ini_word(value, end, "false") || cbat__ini_word(value, end, "no") ||
        cbat__ini_word(value, end, "off") || cbat__ini_word(value, end, "0")) { *out = false; return CBAT_OK; }
    return CBAT_ERR_PARSE;
}

static CBatError cbat__ini_fail(const char *text, size_t offset, CBatParseInfo *info, const char *message) {
    if (info != NULL) {
        size_t i, line = 1, column = 1;
        for (i = 0; i < offset; i++) {
            if (text[i] == '\r') { line++; column = 1; }
            else if (text[i] == '\n') { if (i == 0 || text[i - 1] != '\r') line++; column = 1; }
            else column++;
        }
        info->line = line; info->column = column; info->offset = offset; info->message = message;
    }
    return CBAT_ERR_PARSE;
}

static CBatError cbat__ini_read_value(const char *text, size_t start, size_t end, CBatString *value, size_t *bad) {
    size_t p = start;
    if (p < end && (text[p] == '"' || text[p] == '\'')) {
        char quote = text[p++];
        bool closed = false;
        while (p < end) {
            char c = text[p++];
            CBatError err;
            if (c == quote) { closed = true; break; }
            if (c == '\\' && p < end && (text[p] == '\\' || text[p] == quote)) c = text[p++];
            err = cbat_string_append_char(value, c);
            if (err != CBAT_OK) return err;
        }
        if (!closed) { *bad = start; return CBAT_ERR_PARSE; }
        while (p < end && cbat__ini_space(text[p])) p++;
        if (p < end && !cbat__ini_comment(text[p])) { *bad = p; return CBAT_ERR_PARSE; }
        return CBAT_OK;
    }
    for (; p < end; p++) {
        if (cbat__ini_comment(text[p]) && (p == start || cbat__ini_space(text[p - 1]))) { end = p; break; }
    }
    while (end > start && cbat__ini_space(text[end - 1])) end--;
    return cbat_string_append_length(value, text + start, end - start);
}

CBatError cbat_ini_parse(const char *text, size_t length, CBatIni **out, CBatParseInfo *info) {
    CBatIni *ini;
    CBatString *section, *key, *value;
    size_t position = 0;
    CBatError err = CBAT_OK;
    if ((text == NULL && length != 0) || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (text == NULL) text = "";
    {
        const char *nul = (const char *)memchr(text, 0, length);
        if (nul != NULL) return cbat__ini_fail(text, (size_t)(nul - text), info, "NUL byte in INI text");
    }
    ini = cbat_ini_new();
    section = cbat_string_new(); key = cbat_string_new(); value = cbat_string_new();
    if (ini == NULL || section == NULL || key == NULL || value == NULL) { err = CBAT_ERR_NO_MEMORY; goto done; }
    if (length >= 3 && memcmp(text, "\xef\xbb\xbf", 3) == 0) position = 3;
    while (position < length && err == CBAT_OK) {
        size_t start = position, end, next, p, bad = start;
        while (position < length && text[position] != '\r' && text[position] != '\n') position++;
        end = position; next = position;
        if (next < length && text[next] == '\r') next++;
        if (next < length && text[next] == '\n') next++;
        position = next;
        while (start < end && cbat__ini_space(text[start])) start++;
        while (end > start && cbat__ini_space(text[end - 1])) end--;
        if (start == end || cbat__ini_comment(text[start])) continue;
        if (text[start] == '[') {
            size_t close = start + 1, name_start = close, name_end;
            while (close < end && text[close] != ']') close++;
            if (close == end) { err = cbat__ini_fail(text, start, info, "missing section closing bracket"); break; }
            name_end = close;
            while (name_start < name_end && cbat__ini_space(text[name_start])) name_start++;
            while (name_end > name_start && cbat__ini_space(text[name_end - 1])) name_end--;
            p = close + 1;
            while (p < end && cbat__ini_space(text[p])) p++;
            if (name_start == name_end || (p < end && !cbat__ini_comment(text[p]))) {
                err = cbat__ini_fail(text, start, info, "invalid section header"); break;
            }
            cbat_string_clear(section);
            err = cbat_string_append_length(section, text + name_start, name_end - name_start);
            if (err != CBAT_OK) break;
            if (!cbat__ini_valid_name(section->data, true)) { err = cbat__ini_fail(text, name_start, info, "invalid section name"); break; }
            err = cbat__ini_ensure_section(ini, section->data);
        } else {
            size_t key_end, value_start;
            p = start;
            while (p < end && text[p] != '=' && text[p] != ':') p++;
            if (p == end) { err = cbat__ini_fail(text, start, info, "expected '=' or ':'"); break; }
            key_end = p; value_start = p + 1;
            while (key_end > start && cbat__ini_space(text[key_end - 1])) key_end--;
            while (value_start < end && cbat__ini_space(text[value_start])) value_start++;
            cbat_string_clear(key); cbat_string_clear(value);
            err = cbat_string_append_length(key, text + start, key_end - start);
            if (err == CBAT_OK) err = cbat__ini_read_value(text, value_start, end, value, &bad);
            if (err == CBAT_ERR_PARSE) { err = cbat__ini_fail(text, bad, info, "invalid quoted value"); break; }
            if (err == CBAT_OK) err = cbat_ini_set(ini, section->data, key->data, value->data);
            if (err == CBAT_ERR_INVALID_ARGUMENT) err = cbat__ini_fail(text, start, info, "invalid key or value");
        }
    }
done:
    cbat_string_free(section); cbat_string_free(key); cbat_string_free(value);
    if (err != CBAT_OK) { cbat_ini_free(ini); return err; }
    *out = ini;
    return CBAT_OK;
}

CBatError cbat_ini_read_file(const char *path, CBatIni **out, CBatParseInfo *info) {
    CBatString *text = NULL;
    CBatError err;
    if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat_file_read(path, &text);
    if (err == CBAT_OK) err = cbat_ini_parse(text->data, text->length, out, info);
    cbat_string_free(text);
    return err;
}

bool cbat_ini_next_section(const CBatIni *ini, size_t *iterator, const char **name) {
    return ini != NULL && cbat_dict_next(ini->sections, iterator, name, NULL);
}

bool cbat_ini_next_key(const CBatIni *ini, const char *section, size_t *iterator, const char **key, const char **value) {
    CBatDict *target = cbat__ini_section(ini, section);
    void *slot;
    if (target == NULL || !cbat_dict_next(target, iterator, key, &slot)) return false;
    if (value != NULL) *value = (*(CBatString **)slot)->data;
    return true;
}

static CBatError cbat__ini_write_section(const CBatIni *ini, const char *name, CBatString *out) {
    const char *key, *value;
    size_t it = 0;
    CBatError err = CBAT_OK;
    if (*name != '\0') {
        if (out->length != 0) err = cbat_string_append_char(out, '\n');
        if (err == CBAT_OK) err = cbat_string_append_format(out, "[%s]\n", name);
    }
    while (err == CBAT_OK && cbat_ini_next_key(ini, name, &it, &key, &value)) {
        size_t i;
        err = cbat_string_append_format(out, "%s = \"", key);
        for (i = 0; value[i] != '\0' && err == CBAT_OK; i++) {
            if (value[i] == '\\' || value[i] == '"') err = cbat_string_append_char(out, '\\');
            if (err == CBAT_OK) err = cbat_string_append_char(out, value[i]);
        }
        if (err == CBAT_OK) err = cbat_string_append(out, "\"\n");
    }
    return err;
}

CBatError cbat_ini_to_string(const CBatIni *ini, CBatString **out) {
    CBatString *result;
    size_t it = 0;
    const char *name;
    CBatError err;
    if (ini == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_string_new_with(ini->allocator);
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat__ini_write_section(ini, "", result);
    while (err == CBAT_OK && cbat_ini_next_section(ini, &it, &name)) {
        if (*name != '\0') err = cbat__ini_write_section(ini, name, result);
    }
    if (err != CBAT_OK) { cbat_string_free(result); return err; }
    *out = result;
    return CBAT_OK;
}
