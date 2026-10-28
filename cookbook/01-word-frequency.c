/*
 * Recipe 1: Counting word frequencies
 *
 * PROBLEM
 *   You have some text and want the most common words, like Python's
 *   collections.Counter(text.split()).most_common(3).
 *
 * SOLUTION
 *   Split on whitespace, lower-case each word, count with a CBatDict of
 *   ints, then sort (word, count) pairs by count.
 *
 * DISCUSSION
 *   CBatDict keeps insertion order, so words with equal counts stay in the
 *   order they first appeared - cbat_sort is stable, just like sorted().
 *   CBAT_DICT_GET with a fallback of 0 is the C spelling of
 *   counts.get(word, 0).
 */
#include <stdio.h>

#include "cbat/cbat.h"

typedef struct WordCount {
    const char *word; /* borrowed from the dict */
    int count;
} WordCount;

static int by_count_descending(const void *a, const void *b) {
    const WordCount *x = (const WordCount *)a;
    const WordCount *y = (const WordCount *)b;
    return (y->count > x->count) - (y->count < x->count);
}

int main(void) {
    const char *text =
        "It was the best of times, it was the worst of times, "
        "it was the age of wisdom, it was the age of foolishness";
    CBatList *words = NULL;
    CBatDict *counts = cbat_dict_new(sizeof(int));
    CBatList *pairs = cbat_list_new(sizeof(WordCount));
    size_t it = 0;
    const char *word;
    void *value;
    size_t i;

    if (counts == NULL || pairs == NULL ||
        cbat_string_split_whitespace(text, &words) != CBAT_OK) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    CBAT_LIST_FOREACH(CBatString *, entry, words) {
        CBatString *w = *entry;
        cbat_string_to_lower(w);
        cbat_string_replace(w, ",", "", NULL); /* strip punctuation */
        CBAT_DICT_SET(counts, cbat_string_cstr(w), int,
                      CBAT_DICT_GET(counts, cbat_string_cstr(w), int, 0) + 1);
    }

    while (cbat_dict_next(counts, &it, &word, &value)) {
        WordCount pair;
        pair.word = word;
        pair.count = *(int *)value;
        cbat_list_push(pairs, &pair);
    }
    cbat_list_sort(pairs, by_count_descending);

    printf("Top 3 words:\n");
    for (i = 0; i < 3 && i < cbat_list_length(pairs); i++) {
        WordCount *pair = (WordCount *)cbat_list_get(pairs, i);
        printf("  %-6s %d\n", pair->word, pair->count);
    }

    cbat_list_free(pairs);
    cbat_dict_free(counts);
    cbat_list_free(words);
    return 0;
}
