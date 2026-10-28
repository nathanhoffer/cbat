#include "fuzz_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    CBatJson *value = NULL;
    if (cbat_json_parse((const char *)data, size, &value, NULL) == CBAT_OK) {
        CBatString *text = NULL;
        if (cbat_json_to_string(value, 0, &text) == CBAT_OK) {
            CBatJson *copy = NULL;
            CBAT_FUZZ_CHECK(cbat_json_parse(text->data, text->length, &copy, NULL) == CBAT_OK);
            CBAT_FUZZ_CHECK(cbat_json_equals(value, copy));
            cbat_json_free(copy);
            cbat_string_free(text);
        }
        cbat_json_free(value);
    }
    {
        CBatJsonParseOptions options = {true, true, 64};
        value = NULL;
        if (cbat_json_parse_with((const char *)data, size, &options, &value, NULL) == CBAT_OK) {
            CBatString *text = NULL;
            if (cbat_json_to_string_sorted(value, 0, &text) == CBAT_OK) {
                CBatJson *copy = NULL;
                CBAT_FUZZ_CHECK(cbat_json_parse(text->data, text->length, &copy, NULL) == CBAT_OK);
                CBAT_FUZZ_CHECK(cbat_json_equals(value, copy));
                cbat_json_free(copy); cbat_string_free(text);
            }
            cbat_json_free(value);
        }
    }
    return 0;
}
