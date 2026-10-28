/*
 * Recipe 1: Counting word frequencies
 *
 * PROBLEM
 *   You have some text and want the most common words, like Python's
 *   collections.Counter(text.split()).most_common(3).
 *
 * SOLUTION
 *   Split on whitespace, normalize each word, then use an int64_t dict with
 *   cbat_dict_increment() and cbat_dict_most_common().
 *
 * DISCUSSION
 *   Counter arithmetic reports overflow. Ranking is stable, so tied words
 *   keep their first-occurrence order. The returned key list borrows its
 *   strings from the dict: finish using it before modifying/freeing counts.
 */
#include <stdio.h>

#include "cbat/cbat.h"

int main(void) {
    const char *text =
        "It was the best of times, it was the worst of times, "
        "it was the age of wisdom, it was the age of foolishness";
    CBatList *words = NULL;
    CBatDict *counts = cbat_dict_new(sizeof(int64_t));
    CBatList *top = NULL;
    CBatError err = CBAT_ERR_NO_MEMORY;
    size_t i;

    if (counts == NULL) goto cleanup;
    err = cbat_string_split_whitespace(text, &words);
    if (err != CBAT_OK) goto cleanup;

    CBAT_LIST_FOREACH(CBatString *, entry, words) {
        CBatString *w = *entry;
        cbat_string_to_lower(w);
        err = cbat_string_replace(w, ",", "", NULL);
        if (err != CBAT_OK) goto cleanup;
        err = cbat_dict_increment(counts, cbat_string_cstr(w), 1, NULL);
        if (err != CBAT_OK) goto cleanup;
    }

    err = cbat_dict_most_common(counts, 3, &top);
    if (err != CBAT_OK) goto cleanup;

    printf("Top 3 words:\n");
    for (i = 0; i < cbat_list_length(top); i++) {
        const char *word = CBAT_LIST_GET(top, const char *, i);
        const int64_t *count = (const int64_t *)cbat_dict_get(counts, word);
        printf("  %-6s %lld\n", word, (long long)*count);
    }

cleanup:
    if (err != CBAT_OK) fprintf(stderr, "word frequency: %s\n", cbat_error_str(err));
    cbat_list_free(top);
    cbat_dict_free(counts);
    cbat_list_free(words);
    return err == CBAT_OK ? 0 : 1;
}
