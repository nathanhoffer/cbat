#include "fuzz_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    size_t count = 0;
    bool valid = cbat_utf8_is_valid((const char *)data, size);
    CBatError err = cbat_utf8_count((const char *)data, size, &count);
    CBAT_FUZZ_CHECK(valid == (err == CBAT_OK));
    if (valid) {
        size_t position = 0;
        size_t actual_count = 0;
        while (position < size) {
            uint32_t codepoint;
            char bytes[4];
            size_t used = cbat_utf8_decode((const char *)data + position, size - position, &codepoint);
            CBAT_FUZZ_CHECK(used > 0 && used <= size - position);
            CBAT_FUZZ_CHECK(cbat_utf8_encode(codepoint, bytes) == used);
            CBAT_FUZZ_CHECK(memcmp(bytes, data + position, used) == 0);
            position += used;
            actual_count++;
        }
        CBAT_FUZZ_CHECK(count == actual_count);
        if (memchr(data, 0, size) == NULL && size != SIZE_MAX) {
            char *text = (char *)malloc(size + 1);
            if (text != NULL) {
                CBatString *reversed = NULL, *twice = NULL, *slice = NULL;
                memcpy(text, data, size); text[size] = '\0';
                CBAT_FUZZ_CHECK(cbat_utf8_reverse(text, &reversed) == CBAT_OK);
                CBAT_FUZZ_CHECK(cbat_utf8_reverse(reversed->data, &twice) == CBAT_OK);
                CBAT_FUZZ_CHECK(twice->length == size && memcmp(twice->data, data, size) == 0);
                CBAT_FUZZ_CHECK(cbat_utf8_substring(text, 0, SIZE_MAX, &slice) == CBAT_OK);
                CBAT_FUZZ_CHECK(slice->length == size && memcmp(slice->data, data, size) == 0);
                cbat_string_free(slice); cbat_string_free(twice); cbat_string_free(reversed); free(text);
            }
        }
    }
    return 0;
}
