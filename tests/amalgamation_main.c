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
    CBatHeap *heap;
    int minimum;
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
    heap = cbat_heap_new(sizeof(int), cbat_compare_int);
    if (heap == NULL) return 1;
    if (CBAT_HEAP_PUSH(heap, int, 9) != CBAT_OK ||
        CBAT_HEAP_PUSH(heap, int, 2) != CBAT_OK ||
        cbat_heap_pop(heap, &minimum) != CBAT_OK || minimum != 2) {
        cbat_heap_free(heap);
        return 1;
    }
    cbat_heap_free(heap);
#if !defined(CBAT_WITH_REGEX) || CBAT_WITH_REGEX
    {
        CBatRegex *regex = NULL;
        if (cbat_regex_compile("(ab)+", 0, &regex, NULL) != CBAT_OK) return 1;
        if (!cbat_regex_is_match(regex, "abab")) { cbat_regex_free(regex); return 1; }
        cbat_regex_free(regex);
    }
#endif
#if (!defined(CBAT_WITH_NDARRAY) || CBAT_WITH_NDARRAY) && (!defined(CBAT_WITH_LINALG) || CBAT_WITH_LINALG)
    {
        CBatArray *identity = NULL;
        double determinant = 0;
        if (cbat_linalg_identity(2, &identity) != CBAT_OK) return 1;
        if (cbat_linalg_determinant(identity, &determinant) != CBAT_OK || determinant != 1) {
            cbat_array_free(identity); return 1;
        }
        cbat_array_free(identity);
    }
#endif
    return 0;
}
