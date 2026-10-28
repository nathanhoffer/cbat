/*
 * Recipe 22: Building a search URL
 *
 * PROBLEM
 *   Read URL components and build query parameters containing spaces,
 *   ampersands and literal plus signs without confusing them with syntax.
 *
 * SOLUTION
 *   Parse the URI reference with cbat_url_parse, then parse/build its query
 *   as an owned dictionary of strings. Percent-encode a standalone path
 *   component separately from a form query.
 *
 * DISCUSSION
 *   URL parsing preserves raw component escapes. Query helpers interpret
 *   '+' as space, while cbat_url_decode keeps a literal '+'. Repeated query
 *   keys use the last value. No network request is made by this module.
 */
#include <stdio.h>
#include "cbat/cbat.h"

int main(void) {
    CBatUrl *url = NULL;
    CBatDict *params = NULL;
    CBatString *query = NULL, *component = NULL;
    CBatParseInfo info;
    CBatError err = cbat_url_parse("https://example.com:8443/search?q=C%2B%2B&author=Ada+Lovelace", &url, &info);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_url_parse_query(url->query->data, &params);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_url_build_query(params, &query);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_url_encode("C & C++", &component);
    if (err != CBAT_OK) goto cleanup;
    printf("Host: %s, port: %d\n", url->host->data, url->port);
    printf("Query: %s\nPath component: %s\n", query->data, component->data);
cleanup:
    if (err != CBAT_OK) fprintf(stderr, "URL: %s\n", cbat_error_str(err));
    cbat_string_free(component); cbat_string_free(query);
    cbat_dict_free(params); cbat_url_free(url);
    return err == CBAT_OK ? 0 : 1;
}
