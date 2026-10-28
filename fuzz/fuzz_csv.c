#include "fuzz_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    CBatList *rows = NULL;
    if (cbat_csv_parse((const char *)data, size, NULL, &rows, NULL) == CBAT_OK) {
        CBatString *text = cbat_string_new();
        CBatList *copy = NULL;
        size_t i;
        CBatError err = text == NULL ? CBAT_ERR_NO_MEMORY : CBAT_OK;
        for (i = 0; i < cbat_list_length(rows) && err == CBAT_OK; i++) {
            err = cbat_csv_append_row_list(text, CBAT_LIST_GET(rows, CBatList *, i), NULL);
        }
        if (err == CBAT_OK) {
            CBAT_FUZZ_CHECK(cbat_csv_parse(text->data, text->length, NULL, &copy, NULL) == CBAT_OK);
            CBAT_FUZZ_CHECK(cbat_list_length(rows) == cbat_list_length(copy));
            for (i = 0; i < cbat_list_length(rows); i++) {
                CBatList *a = CBAT_LIST_GET(rows, CBatList *, i);
                CBatList *b = CBAT_LIST_GET(copy, CBatList *, i);
                size_t j;
                CBAT_FUZZ_CHECK(cbat_list_length(a) == cbat_list_length(b));
                for (j = 0; j < cbat_list_length(a); j++) {
                    CBAT_FUZZ_CHECK(cbat_string_compare(CBAT_LIST_GET(a, CBatString *, j),
                                                       CBAT_LIST_GET(b, CBatString *, j)) == 0);
                }
            }
        }
        cbat_list_free(copy);
        cbat_string_free(text);
        cbat_list_free(rows);
    }
    return 0;
}
