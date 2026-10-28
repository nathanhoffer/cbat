/* Recipe 24: Calendar scheduling, sampling and message authentication.
 * Month addition clamps the day; calendar operations preserve the fixed UTC
 * offset rather than applying daylight-saving rules. Sampling copies values.
 * HMAC authenticates bytes with a shared secret; PCG UUIDs are for simulations,
 * not secret tokens. Production secrets must come from the host securely.
 */
#include "cbat/cbat.h"
#include <stdio.h>

int main(void) {
    CBatDateTime date;
    CBatRandom rng;
    CBatList *items = NULL, *sample = NULL;
    CBatString *iso = NULL, *duration = NULL;
    unsigned char digest[32];
    char uuid[37];
    CBatError err = cbat_datetime_parse_iso("2024-01-31T09:00:00Z", &date, NULL);
    size_t i;
    if (err != CBAT_OK) goto cleanup;
    err = cbat_datetime_add_months(&date, 1);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_datetime_format_iso(&date, &iso);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_format_duration(3723.5, &duration);
    if (err != CBAT_OK) goto cleanup;
    printf("Next date: %s; elapsed: %s\n", iso->data, duration->data);
    items = cbat_list_new(sizeof(int));
    if (items == NULL) { err = CBAT_ERR_NO_MEMORY; goto cleanup; }
    for (i = 0; i < 10; i++) {
        int value = (int)i;
        err = cbat_list_push(items, &value);
        if (err != CBAT_OK) goto cleanup;
    }
    cbat_random_seed(&rng, 42);
    err = cbat_random_sample(&rng, items, 3, &sample);
    if (err != CBAT_OK) goto cleanup;
    for (i = 0; i < sample->length; i++) printf("Sample: %d\n", *(int *)cbat_list_get(sample, i));
    cbat_random_uuid4(&rng, uuid); printf("Simulation ID: %s\n", uuid);
    cbat_hmac_sha256("example key", 11, "hello", 5, digest);
    printf("HMAC-SHA256: ");
    for (i = 0; i < sizeof(digest); i++) printf("%02x", (unsigned)digest[i]);
    puts("");
cleanup:
    if (err != CBAT_OK) fprintf(stderr, "%s\n", cbat_error_str(err));
    cbat_list_free(sample); cbat_list_free(items);
    cbat_string_free(iso); cbat_string_free(duration);
    return err == CBAT_OK ? 0 : 1;
}
