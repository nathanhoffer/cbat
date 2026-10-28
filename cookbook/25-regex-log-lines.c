/* Recipe 25: Extracting fields from a log line with a regex.
 * Captures are byte spans borrowing the original text. This ordered Pike VM
 * avoids exponential backtracking, including nested repetition. ASCII classes
 * and case folding are deliberate; it does not implement Unicode properties.
 * Build with CBAT_WITH_REGEX=ON and include the optional header explicitly.
 */
#include "cbat/regex.h"
#include <stdio.h>

int main(void) {
    const char *line = "2026-10-27 INFO request=123 status=200";
    CBatRegex *regex = NULL;
    CBatRegexMatch match = {0};
    CBatError err = cbat_regex_compile("^(\\d{4}-\\d{2}-\\d{2}) (\\w+) request=(\\d+) status=(\\d+)$", 0, &regex, NULL);
    size_t i;
    if (err == CBAT_OK) err = cbat_regex_search(regex, line, 0, &match);
    if (err == CBAT_OK) for (i = 1; i < match.group_count; i++) {
        CBatSpan span = match.groups[i];
        printf("Field %lu: %.*s\n", (unsigned long)i, (int)(span.end - span.start), line + span.start);
    }
    if (err != CBAT_OK) fprintf(stderr, "%s\n", cbat_error_str(err));
    cbat_regex_free(regex);
    return err == CBAT_OK ? 0 : 1;
}
