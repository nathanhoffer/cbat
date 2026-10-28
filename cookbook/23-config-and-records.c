/* Recipe 23: Named CSV records and JSON configuration updates.
 * Parse CSV by column name, apply an RFC 7396 merge patch, and print stable
 * sorted JSON. Null patch members delete keys; arrays replace whole arrays.
 * Extension parsing is opt-in: normal cbat_json_parse remains strict JSON.
 */
#include "cbat/cbat.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *csv = "name,language\nAda,Analytical Engine\nGrace,COBOL\n";
    const char *source = "{/* configuration */\"port\":8080,\"debug\":true,}";
    const char *change = "{\"port\":9090,\"debug\":null}";
    CBatJsonParseOptions options = {true, true, 32};
    CBatList *records = NULL;
    CBatJson *config = NULL, *patch = NULL;
    CBatString *output = NULL;
    CBatError err = cbat_csv_parse_records(csv, strlen(csv), NULL, &records, NULL);
    size_t i;
    if (err != CBAT_OK) goto cleanup;
    for (i = 0; i < records->length; i++) {
        CBatDict *record = *(CBatDict **)cbat_list_get(records, i);
        CBatString *name = *(CBatString **)cbat_dict_get(record, "name");
        printf("Name: %s\n", name->data);
    }
    err = cbat_json_parse_with(source, strlen(source), &options, &config, NULL);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_json_parse(change, strlen(change), &patch, NULL);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_json_merge_patch(config, patch);
    if (err != CBAT_OK) goto cleanup;
    printf("Port: %lld\n", (long long)cbat_json_pointer_int(config, "/port", 80));
    err = cbat_json_to_string_sorted(config, 2, &output);
    if (err == CBAT_OK) puts(output->data);
cleanup:
    if (err != CBAT_OK) fprintf(stderr, "%s\n", cbat_error_str(err));
    cbat_string_free(output); cbat_json_free(config); cbat_json_free(patch);
    cbat_list_free(records);
    return err == CBAT_OK ? 0 : 1;
}
