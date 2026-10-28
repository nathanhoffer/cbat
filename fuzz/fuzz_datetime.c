#include "fuzz_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *input;
    CBatDateTime value;
    if (size == SIZE_MAX) return 0;
    input = (char *)malloc(size + 1);
    if (input == NULL) return 0;
    memcpy(input, data, size);
    input[size] = '\0';
    if (cbat_datetime_parse_iso(input, &value, NULL) == CBAT_OK) {
        CBatString *text = NULL;
        CBAT_FUZZ_CHECK(cbat_datetime_is_valid(&value));
        if (cbat_datetime_format_iso(&value, &text) == CBAT_OK) {
            CBatDateTime copy;
            CBAT_FUZZ_CHECK(cbat_datetime_parse_iso(text->data, &copy, NULL) == CBAT_OK);
            CBAT_FUZZ_CHECK(value.year == copy.year && value.month == copy.month &&
                value.day == copy.day && value.hour == copy.hour && value.minute == copy.minute &&
                value.second == copy.second && value.nanosecond == copy.nanosecond &&
                value.utc_offset_minutes == copy.utc_offset_minutes);
            cbat_string_free(text);
        }
    }
    free(input);
    return 0;
}
