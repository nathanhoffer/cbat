#include "fuzz_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    CBatString *encoded = NULL;
    CBatString *decoded = NULL;
    CBatString *parsed = NULL;
    if (cbat_base64_encode(data, size, &encoded) == CBAT_OK) {
        CBAT_FUZZ_CHECK(cbat_base64_decode(encoded->data, encoded->length, &decoded) == CBAT_OK);
        CBAT_FUZZ_CHECK(decoded->length == size && memcmp(decoded->data, data, size) == 0);
        cbat_string_free(decoded);
        cbat_string_free(encoded);
    }
    if (cbat_base64_decode((const char *)data, size, &parsed) == CBAT_OK) {
        encoded = NULL;
        decoded = NULL;
        if (cbat_base64url_encode(parsed->data, parsed->length, &encoded) == CBAT_OK) {
            CBAT_FUZZ_CHECK(cbat_base64_decode(encoded->data, encoded->length, &decoded) == CBAT_OK);
            CBAT_FUZZ_CHECK(cbat_string_compare(parsed, decoded) == 0);
            cbat_string_free(decoded);
            cbat_string_free(encoded);
        }
        cbat_string_free(parsed);
    }
    return 0;
}
