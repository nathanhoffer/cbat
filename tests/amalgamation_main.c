/*
 * Compiles the generated single header with the implementation and runs a
 * smoke test through a few modules. Built only when Python is available.
 */
#define CBAT_IMPLEMENTATION
#include "cbat.h"

#include <stdio.h>

int main(void) {
    CBatArena *arena = cbat_arena_new(0);
    CBatMap *map;
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
    map = cbat_map_new(sizeof(int), sizeof(int));
    if (map == NULL) return 1;
    if (CBAT_MAP_SET(map, int, 42, int, 7) != CBAT_OK ||
        CBAT_MAP_GET(map, int, 42, int, 0) != 7) {
        cbat_map_free(map);
        return 1;
    }
    cbat_map_free(map);
    return 0;
}
