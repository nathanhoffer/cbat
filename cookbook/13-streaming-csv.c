/* Recipe 13: Streaming a large CSV with constant memory.
 * PROBLEM: Total a file too large to load into a list of all its rows.
 * SOLUTION: Read one borrowed row at a time with CBatCsvReader.
 * DISCUSSION: Memory depends on the largest row, not the number of rows.
 * Each row expires at the next reader_next call. The sample creates a temporary
 * fixture; a real program can open an existing data file directly.
 */
#include "cbat/cbat.h"
#include <stdio.h>

int main(void) {
    CBatString *directory = NULL, *path = NULL;
    CBatCsvReader *reader = NULL;
    const CBatList *fields = NULL;
    size_t rows = 0, i;
    double total = 0;
    CBatError err = cbat_fs_make_temp_dir("cbat-csv-", &directory);
    if (err != CBAT_OK) goto cleanup;
    path = cbat_path_join(directory->data, "sales.csv");
    if (path == NULL) { err = CBAT_ERR_NO_MEMORY; goto cleanup; }
    err = cbat_file_write_text(path->data, "region,amount\n");
    if (err != CBAT_OK) goto cleanup;
    for (i = 0; i < 2000; i++) {
        err = cbat_file_append(path->data, "north,12.5\n", 11);
        if (err != CBAT_OK) goto cleanup;
    }
    err = cbat_csv_reader_open(path->data, NULL, &reader);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_csv_reader_next(reader, &fields); /* header */
    if (err != CBAT_OK) goto cleanup;
    while ((err = cbat_csv_reader_next(reader, &fields)) == CBAT_OK) {
        double amount;
        CBatString *field;
        if (fields->length != 2) { err = CBAT_ERR_PARSE; goto cleanup; }
        field = *(CBatString **)cbat_list_get(fields, 1);
        err = cbat_string_to_double(field->data, &amount);
        if (err != CBAT_OK) goto cleanup;
        total += amount; rows++;
    }
    if (err == CBAT_ERR_END) err = CBAT_OK;
    if (err == CBAT_OK) printf("%lu rows, total %.2f\n", (unsigned long)rows, total);
cleanup:
    cbat_csv_reader_close(reader);
    if (directory != NULL) {
        CBatError removed = cbat_fs_remove_tree(directory->data);
        if (err == CBAT_OK) err = removed;
    }
    cbat_string_free(path); cbat_string_free(directory);
    if (err != CBAT_OK) fprintf(stderr, "%s\n", cbat_error_str(err));
    return err == CBAT_OK ? 0 : 1;
}
