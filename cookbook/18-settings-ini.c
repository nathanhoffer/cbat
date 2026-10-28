/*
 * Recipe 18: Reading INI settings
 *
 * PROBLEM
 *   Load named settings, check their types, and save an updated configuration.
 * SOLUTION
 *   Parse INI text, use typed getters, then serialize the owned settings object.
 * DISCUSSION
 *   A native program can replace parse with cbat_ini_read_file. This example
 *   keeps its input in memory so it also runs in the app VM. Section/key names
 *   are case-sensitive. Quoted values preserve spaces and comment characters.
 */
#include <stdio.h>
#include <string.h>
#include "cbat/cbat.h"

int main(void) {
    const char *source = "[server]\nhost = localhost\nport = 8080\ntls = yes\n"
                         "[display]\nscale = 1.25\ntitle = \"C # batteries\"\n";
    CBatIni *settings = NULL;
    CBatString *saved = NULL;
    CBatParseInfo info;
    int64_t port = 0;
    double scale = 0;
    bool tls = false;
    CBatError err = cbat_ini_parse(source, strlen(source), &settings, &info);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_ini_get_int(settings, "server", "port", &port);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_ini_get_bool(settings, "server", "tls", &tls);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_ini_get_double(settings, "display", "scale", &scale);
    if (err != CBAT_OK) goto cleanup;
    printf("Server: %s:%lld, TLS: %s, scale: %.2f\n",
           cbat_ini_get(settings, "server", "host"), (long long)port,
           tls ? "yes" : "no", scale);
    err = cbat_ini_set(settings, "server", "port", "9090");
    if (err != CBAT_OK) goto cleanup;
    err = cbat_ini_to_string(settings, &saved);
    if (err == CBAT_OK) printf("Updated settings:\n%s", saved->data);
cleanup:
    if (err != CBAT_OK) fprintf(stderr, "Settings: %s\n", cbat_error_str(err));
    cbat_string_free(saved);
    cbat_ini_free(settings);
    return err == CBAT_OK ? 0 : 1;
}
