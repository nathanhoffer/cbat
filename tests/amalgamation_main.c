/*
 * Compiles the generated single header with the implementation and runs a
 * smoke test through a few modules. Built only when Python is available.
 */
#define CBAT_IMPLEMENTATION
#include "cbat.h"

#include <stdio.h>

int main(void) {
    CBatArena *arena = cbat_arena_new(0);
    char *copy;
    if (arena == NULL) {
        return 1;
    }
    copy = cbat_arena_strdup(arena, "single header works");
    if (copy == NULL) {
        cbat_arena_free(arena);
        return 1;
    }
    printf("%s (cbat %s)\n", copy, CBAT_VERSION_STRING);
    cbat_arena_free(arena);
    return 0;
}
