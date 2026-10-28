#include "fuzz_common.h"
#include "cbat/regex.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char pattern[65], text[129];
    CBatRegex *regex = NULL;
    size_t split, length, i;
    if (size == 0) return 0;
    split = CBAT_MIN((size_t)(data[0] % 65), size - 1);
    memcpy(pattern, data + 1, split); pattern[split] = 0;
    length = CBAT_MIN(size - 1 - split, sizeof(text) - 1);
    memcpy(text, data + 1 + split, length); text[length] = 0;
    length = strlen(text);
    if (cbat_regex_compile(pattern, data[0] & 7, &regex, NULL) == CBAT_OK) {
        CBatRegexMatch match;
        CBatList *matches = NULL;
        CBatError err = cbat_regex_search(regex, text, 0, &match);
        CBAT_FUZZ_CHECK(err == CBAT_OK || err == CBAT_ERR_NOT_FOUND || err == CBAT_ERR_NO_MEMORY);
        if (err == CBAT_OK) {
            CBAT_FUZZ_CHECK(match.groups[0].start <= match.groups[0].end && match.groups[0].end <= length);
            for (i = 1; i < match.group_count; i++) if (match.groups[i].start != CBAT_NPOS)
                CBAT_FUZZ_CHECK(match.groups[i].start <= match.groups[i].end && match.groups[i].end <= length);
        }
        if (cbat_regex_find_all(regex, text, &matches) == CBAT_OK) {
            size_t previous = 0;
            CBAT_FUZZ_CHECK(matches->length <= length + 1);
            for (i = 0; i < matches->length; i++) {
                CBatRegexMatch *m = (CBatRegexMatch *)cbat_list_get(matches, i);
                CBAT_FUZZ_CHECK(m->groups[0].start >= previous && m->groups[0].end <= length);
                previous = m->groups[0].end;
            }
            cbat_list_free(matches);
        }
        cbat_regex_free(regex);
    }
    return 0;
}
