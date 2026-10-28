/*
 * Recipe 2: Reading a JSON configuration with defaults
 *
 * PROBLEM
 *   Load settings from JSON, fall back to defaults for anything missing,
 *   report syntax errors with a line number, and save changes back.
 *
 * SOLUTION
 *   cbat_json_parse() fills a CBatParseInfo on failure. JSON Pointer paths
 *   ("/server/port") plus the forgiving cbat_json_as_* getters make
 *   "value or default" a one-liner.
 *
 * DISCUSSION
 *   The as_* getters never fail: a missing key, a null, or a value of the
 *   wrong type all produce the fallback. When you need to distinguish those
 *   cases, use the strict cbat_json_get_* functions, which return
 *   CBAT_ERR_TYPE for a wrong type.
 */
#include <stdio.h>
#include <string.h>

#include "cbat/cbat.h"

int main(void) {
    const char *config_text =
        "{\n"
        "  \"server\": { \"host\": \"localhost\", \"port\": 8080 },\n"
        "  \"features\": [\"search\", \"export\"],\n"
        "  \"debug\": true\n"
        "}\n";
    const char *broken = "{\n  \"name\": \"demo\",\n  \"size\": 12,\n}";
    CBatJson *config = NULL;
    CBatParseInfo info;
    CBatString *out = NULL;
    size_t i;

    if (cbat_json_parse(broken, strlen(broken), &config, &info) != CBAT_OK) {
        printf("broken.json:%lu:%lu: %s\n", (unsigned long)info.line,
               (unsigned long)info.column, info.message);
    }

    if (cbat_json_parse(config_text, strlen(config_text), &config, &info) != CBAT_OK) {
        printf("config error at line %lu: %s\n", (unsigned long)info.line, info.message);
        return 1;
    }

    printf("host    = %s\n", cbat_json_as_string(cbat_json_pointer(config, "/server/host"), "0.0.0.0"));
    printf("port    = %lld\n", (long long)cbat_json_as_int64(cbat_json_pointer(config, "/server/port"), 80));
    printf("timeout = %g (default)\n", cbat_json_as_number(cbat_json_pointer(config, "/server/timeout"), 30.0));
    printf("debug   = %s\n", cbat_json_as_bool(cbat_json_object_get(config, "debug"), false) ? "on" : "off");

    {
        CBatJson *features = cbat_json_object_get(config, "features");
        for (i = 0; i < cbat_json_length(features); i++) {
            printf("feature: %s\n", cbat_json_as_string(cbat_json_array_get(features, i), "?"));
        }
        cbat_json_array_append_string(features, "share");
    }

    cbat_json_object_set_int(cbat_json_object_get(config, "server"), "port", 9090);
    if (cbat_json_to_string(config, 2, &out) == CBAT_OK) {
        printf("updated config:\n%s\n", cbat_string_cstr(out));
        cbat_string_free(out);
    }
    cbat_json_free(config);
    return 0;
}
