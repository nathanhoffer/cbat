/*
 * Recipe 12: Comparing collections with sets, reporting with logging
 *
 * PROBLEM
 *   Two teams list the tools they use. Who uses what in common, what is
 *   unique to each - and how do you report progress without printf noise
 *   in the final output?
 *
 * SOLUTION
 *   CBatSet gives union / intersection / difference. CBAT_LOG_* macros
 *   write leveled diagnostics to stderr (or a handler), separate from the
 *   program's real output on stdout.
 *
 * DISCUSSION
 *   Raise the threshold with cbat_log_set_level(CBAT_LOG_WARNING) to
 *   silence chatter; lower it to CBAT_LOG_DEBUG when investigating. The
 *   app routes these messages into its console panel via a handler.
 */
#include <stdio.h>

#include "cbat/cbat.h"

static CBatSet *set_of(const char *const *items, size_t count) {
    CBatSet *set = cbat_set_new();
    size_t i;
    for (i = 0; set != NULL && i < count; i++) {
        cbat_set_add(set, items[i]);
    }
    return set;
}

static void print_set(const char *label, const CBatSet *set) {
    size_t it = 0;
    const char *item;
    printf("%-10s:", label);
    while (cbat_set_next(set, &it, &item)) {
        printf(" %s", item);
    }
    putchar('\n');
}

int main(void) {
    const char *team_a[] = {"git", "cmake", "clang", "vim", "gdb"};
    const char *team_b[] = {"git", "meson", "gcc", "vim", "lldb", "git"};
    CBatSet *a = set_of(team_a, CBAT_ARRAY_LEN(team_a));
    CBatSet *b = set_of(team_b, CBAT_ARRAY_LEN(team_b));
    CBatSet *both = NULL;
    CBatSet *only_a = NULL;
    CBatSet *all = NULL;

    cbat_log_set_level(CBAT_LOG_DEBUG);
    CBAT_LOG_DEBUG("team A: %lu tools, team B: %lu tools (duplicates removed)",
                   (unsigned long)cbat_set_length(a), (unsigned long)cbat_set_length(b));

    if (cbat_set_intersection(a, b, &both) != CBAT_OK ||
        cbat_set_difference(a, b, &only_a) != CBAT_OK || cbat_set_union(a, b, &all) != CBAT_OK) {
        CBAT_LOG_ERROR("out of memory");
        return 1;
    }
    print_set("shared", both);
    print_set("only A", only_a);
    print_set("everyone", all);
    if (!cbat_set_is_subset(both, a)) {
        CBAT_LOG_WARNING("this should never happen");
    }
    CBAT_LOG_INFO("done");

    cbat_set_free(a);
    cbat_set_free(b);
    cbat_set_free(both);
    cbat_set_free(only_a);
    cbat_set_free(all);
    return 0;
}
