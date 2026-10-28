/* URI-reference parsing and percent coding. Part of cbat. MIT License. */
#include "cbat/url.h"
#include <string.h>

static bool cbat__url_alpha(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool cbat__url_digit(unsigned char c) { return c >= '0' && c <= '9'; }
static bool cbat__url_unreserved(unsigned char c) {
    return cbat__url_alpha(c) || cbat__url_digit(c) || c == '-' || c == '.' || c == '_' || c == '~';
}
static int cbat__url_hex(unsigned char c) {
    if (cbat__url_digit(c)) return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static CBatError cbat__url_append_encoded(CBatString *out, const char *text, size_t length, bool form) {
    static const char hex[] = "0123456789ABCDEF";
    size_t needed, i;
    CBatError err;
    if (!cbat_size_mul(length, 3, &needed)) return CBAT_ERR_OVERFLOW;
    err = cbat_string_reserve(out, needed);
    if (err != CBAT_OK) return err;
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (cbat__url_unreserved(c)) out->data[out->length++] = (char)c;
        else if (form && c == ' ') out->data[out->length++] = '+';
        else {
            out->data[out->length++] = '%';
            out->data[out->length++] = hex[c >> 4];
            out->data[out->length++] = hex[c & 15];
        }
    }
    out->data[out->length] = '\0';
    return CBAT_OK;
}

static CBatError cbat__url_encode(const char *text, bool form, CBatString **out) {
    CBatString *result;
    CBatError err;
    if (text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_string_new();
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat__url_append_encoded(result, text, strlen(text), form);
    if (err != CBAT_OK) { cbat_string_free(result); return err; }
    *out = result;
    return CBAT_OK;
}

CBatError cbat_url_encode(const char *text, CBatString **out) { return cbat__url_encode(text, false, out); }
CBatError cbat_url_encode_form(const char *text, CBatString **out) { return cbat__url_encode(text, true, out); }

static CBatError cbat__url_decode(const char *text, size_t length, bool form, CBatString **out) {
    CBatString *result = cbat_string_new();
    CBatError err;
    size_t i;
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat_string_reserve(result, length);
    if (err != CBAT_OK) { cbat_string_free(result); return err; }
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '%') {
            int high, low;
            if (length - i < 3 || (high = cbat__url_hex((unsigned char)text[i + 1])) < 0 ||
                (low = cbat__url_hex((unsigned char)text[i + 2])) < 0) {
                cbat_string_free(result);
                return CBAT_ERR_PARSE;
            }
            c = (unsigned char)(high * 16 + low);
            i += 2;
        } else if (form && c == '+') c = ' ';
        result->data[result->length++] = (char)c;
    }
    result->data[result->length] = '\0';
    *out = result;
    return CBAT_OK;
}

CBatError cbat_url_decode(const char *text, CBatString **out) {
    if (text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    return cbat__url_decode(text, strlen(text), false, out);
}

static bool cbat__url_subdelimiter(unsigned char c) { return c != 0 && strchr("!$&'()*+,;=", c) != NULL; }

static const char *cbat__url_bad_component(const char *start, const char *end, const char *extra) {
    const char *p;
    for (p = start; p < end; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '%') {
            if (end - p < 3 || cbat__url_hex((unsigned char)p[1]) < 0 || cbat__url_hex((unsigned char)p[2]) < 0) return p;
            p += 2;
        } else if (!cbat__url_unreserved(c) && !cbat__url_subdelimiter(c) &&
                   (c == 0 || strchr(extra, c) == NULL)) return p;
    }
    return NULL;
}

static bool cbat__url_ipv4(const char *p, const char *end) {
    int groups;
    for (groups = 0; groups < 4; groups++) {
        const char *start = p;
        unsigned value = 0;
        while (p < end && cbat__url_digit((unsigned char)*p)) {
            value = value * 10 + (unsigned)(*p++ - '0');
            if (p - start > 3 || value > 255) return false;
        }
        if (p == start || (p - start > 1 && *start == '0')) return false;
        if (groups == 3) return p == end;
        if (p == end || *p++ != '.') return false;
    }
    return false;
}

static bool cbat__url_ip_literal(const char *p, const char *end) {
    int groups = 0;
    bool compressed = false;
    if (p == end) return false;
    if (*p == 'v' || *p == 'V') {
        const char *version = ++p;
        while (p < end && cbat__url_hex((unsigned char)*p) >= 0) p++;
        if (p == version || p == end || *p++ != '.' || p == end) return false;
        return cbat__url_bad_component(p, end, ":") == NULL && memchr(p, '%', (size_t)(end - p)) == NULL;
    }
    if (*p == ':') {
        if (end - p < 2 || p[1] != ':') return false;
        p += 2; compressed = true;
    }
    while (p < end) {
        const char *start = p;
        while (p < end && cbat__url_hex((unsigned char)*p) >= 0) p++;
        if (p < end && *p == '.') {
            if (!cbat__url_ipv4(start, end)) return false;
            groups += 2; p = end; break;
        }
        if (p == start || p - start > 4) return false;
        groups++;
        if (groups > 8) return false;
        if (p == end) break;
        if (*p++ != ':') return false;
        if (p < end && *p == ':') {
            if (compressed) return false;
            compressed = true; p++;
        } else if (p == end) return false;
    }
    return compressed ? groups < 8 : groups == 8;
}

static CBatError cbat__url_fail(const char *text, const char *at, CBatParseInfo *info, const char *message) {
    if (info != NULL) {
        info->offset = (size_t)(at - text); info->line = 1;
        info->column = info->offset + 1; info->message = message;
    }
    return CBAT_ERR_PARSE;
}

void cbat_url_free(CBatUrl *url) {
    if (url == NULL) return;
    cbat_string_free(url->scheme); cbat_string_free(url->user); cbat_string_free(url->host);
    cbat_string_free(url->path); cbat_string_free(url->query); cbat_string_free(url->fragment);
    cbat_free(url);
}

CBatError cbat_url_parse(const char *text, CBatUrl **out, CBatParseInfo *info) {
    const char *end, *hash, *question, *p, *bad;
    const char *scheme_end, *path_start, *path_end, *user_start, *user_end, *host_start, *host_end;
    const char *query_start, *query_end, *fragment_start, *port_start = NULL, *authority_end = NULL;
    int port = -1;
    CBatUrl *url;
    if (text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    end = text + strlen(text);
    scheme_end = user_start = user_end = host_start = host_end = text;
    hash = strchr(text, '#');
    fragment_start = hash == NULL ? end : hash + 1;
    query_end = hash == NULL ? end : hash;
    question = (const char *)memchr(text, '?', (size_t)(query_end - text));
    query_start = question == NULL ? query_end : question + 1;
    path_end = question == NULL ? query_end : question;
    path_start = text;
    for (p = text; p < path_end && *p != '/' && *p != ':'; p++) { }
    if (p < path_end && *p == ':') {
        const char *s;
        if (p == text || !cbat__url_alpha((unsigned char)*text)) return cbat__url_fail(text, text, info, "invalid scheme");
        for (s = text + 1; s < p; s++) {
            if (!cbat__url_alpha((unsigned char)*s) && !cbat__url_digit((unsigned char)*s) &&
                strchr("+.-", *s) == NULL) return cbat__url_fail(text, s, info, "invalid scheme");
        }
        scheme_end = p; path_start = p + 1;
    }
    if (path_end - path_start >= 2 && path_start[0] == '/' && path_start[1] == '/') {
        const char *at, *colon;
        host_start = path_start + 2;
        authority_end = host_start;
        while (authority_end < path_end && *authority_end != '/') authority_end++;
        path_start = authority_end;
        at = (const char *)memchr(host_start, '@', (size_t)(authority_end - host_start));
        if (at != NULL) {
            user_start = host_start; user_end = at; host_start = at + 1;
            bad = cbat__url_bad_component(user_start, user_end, ":");
            if (bad != NULL) return cbat__url_fail(text, bad, info, "invalid user information");
        }
        if (host_start < authority_end && *host_start == '[') {
            const char *close = (const char *)memchr(host_start, ']', (size_t)(authority_end - host_start));
            if (close == NULL || !cbat__url_ip_literal(host_start + 1, close))
                return cbat__url_fail(text, host_start, info, "invalid IP literal");
            host_start++; host_end = close;
            if (close + 1 < authority_end) {
                if (close[1] != ':') return cbat__url_fail(text, close + 1, info, "unexpected text after host");
                port_start = close + 2;
            }
        } else {
            colon = (const char *)memchr(host_start, ':', (size_t)(authority_end - host_start));
            host_end = colon == NULL ? authority_end : colon;
            if (colon != NULL) port_start = colon + 1;
            bad = cbat__url_bad_component(host_start, host_end, "");
            if (bad != NULL) return cbat__url_fail(text, bad, info, "invalid host");
        }
        if (port_start != NULL && port_start < authority_end) {
            port = 0;
            for (p = port_start; p < authority_end; p++) {
                if (!cbat__url_digit((unsigned char)*p) || port > (65535 - (*p - '0')) / 10)
                    return cbat__url_fail(text, p, info, "invalid port");
                port = port * 10 + (*p - '0');
            }
        }
    }
    bad = cbat__url_bad_component(path_start, path_end, "/:@");
    if (bad != NULL) return cbat__url_fail(text, bad, info, "invalid path");
    bad = cbat__url_bad_component(query_start, query_end, "/?:@");
    if (bad != NULL) return cbat__url_fail(text, bad, info, "invalid query");
    bad = cbat__url_bad_component(fragment_start, end, "/?:@");
    if (bad != NULL) return cbat__url_fail(text, bad, info, "invalid fragment");
    url = (CBatUrl *)cbat_alloc_zeroed(NULL, sizeof(CBatUrl));
    if (url == NULL) return CBAT_ERR_NO_MEMORY;
    url->port = port;
    url->scheme = cbat_string_from_length(text, (size_t)(scheme_end - text));
    url->user = cbat_string_from_length(user_start, (size_t)(user_end - user_start));
    url->host = cbat_string_from_length(host_start, (size_t)(host_end - host_start));
    url->path = cbat_string_from_length(path_start, (size_t)(path_end - path_start));
    url->query = cbat_string_from_length(query_start, (size_t)(query_end - query_start));
    url->fragment = cbat_string_from_length(fragment_start, (size_t)(end - fragment_start));
    if (url->scheme == NULL || url->user == NULL || url->host == NULL || url->path == NULL || url->query == NULL || url->fragment == NULL) {
        cbat_url_free(url); return CBAT_ERR_NO_MEMORY;
    }
    *out = url;
    return CBAT_OK;
}

CBatError cbat_url_parse_query(const char *query, CBatDict **out) {
    CBatDict *result;
    const char *p;
    CBatError err = CBAT_OK;
    if (query == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_dict_new(sizeof(CBatString *));
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    cbat_dict_set_destructor(result, cbat_string_destructor);
    p = query;
    while (*p != '\0') {
        const char *end = strchr(p, '&'), *equals;
        CBatString *key = NULL, *value = NULL;
        if (end == NULL) end = p + strlen(p);
        if (end == p) { p++; continue; }
        equals = (const char *)memchr(p, '=', (size_t)(end - p));
        err = cbat__url_decode(p, (size_t)((equals == NULL ? end : equals) - p), true, &key);
        if (err == CBAT_OK && memchr(key->data, 0, key->length) != NULL) err = CBAT_ERR_PARSE;
        if (err == CBAT_OK) err = cbat__url_decode(equals == NULL ? end : equals + 1,
            equals == NULL ? 0 : (size_t)(end - equals - 1), true, &value);
        if (err == CBAT_OK) err = cbat_dict_set(result, key->data, &value);
        cbat_string_free(key);
        if (err != CBAT_OK) { cbat_string_free(value); break; }
        p = *end == '&' ? end + 1 : end;
    }
    if (err != CBAT_OK) { cbat_dict_free(result); return err; }
    *out = result;
    return CBAT_OK;
}

CBatError cbat_url_build_query(const CBatDict *params, CBatString **out) {
    CBatString *result;
    size_t it = 0;
    const char *key;
    void *slot;
    CBatError err = CBAT_OK;
    bool first = true;
    if (params == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (cbat_dict_value_size(params) != sizeof(CBatString *)) return CBAT_ERR_TYPE;
    result = cbat_string_new();
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    while (cbat_dict_next(params, &it, &key, &slot)) {
        const CBatString *value = *(CBatString **)slot;
        if (!first) err = cbat_string_append_char(result, '&');
        if (err == CBAT_OK) err = cbat__url_append_encoded(result, key, strlen(key), true);
        if (err == CBAT_OK) err = cbat_string_append_char(result, '=');
        if (err == CBAT_OK) err = cbat__url_append_encoded(result, value == NULL ? "" : value->data,
            value == NULL ? 0 : value->length, true);
        if (err != CBAT_OK) break;
        first = false;
    }
    if (err != CBAT_OK) { cbat_string_free(result); return err; }
    *out = result;
    return CBAT_OK;
}
