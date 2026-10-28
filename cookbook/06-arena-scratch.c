/*
 * Recipe 6: Throwaway objects with an arena
 *
 * PROBLEM
 *   A loop builds lots of temporary strings and lists per iteration.
 *   Freeing each one individually is tedious and easy to get wrong.
 *
 * SOLUTION
 *   Create the temporaries with *_new_with(cbat_arena_allocator(arena))
 *   and call cbat_arena_reset() at the end of each iteration. One reset
 *   reclaims everything allocated during that iteration.
 *
 * DISCUSSION
 *   Calling cbat_string_free() on an arena-backed string is allowed but
 *   unnecessary. Never keep pointers to arena objects across a reset.
 *   Arenas are also faster: allocation is a pointer bump.
 */
#include <stdio.h>

#include "cbat/cbat.h"

int main(void) {
    const char *records[] = {"ada:36:london", "grace:85:new york", "linus:56:helsinki"};
    CBatArena *scratch = cbat_arena_new(4096);
    size_t i;
    if (scratch == NULL) {
        return 1;
    }
    for (i = 0; i < CBAT_ARRAY_LEN(records); i++) {
        const CBatAllocator *a = cbat_arena_allocator(scratch);
        CBatString *line = cbat_string_new_with(a);
        CBatList *parts = NULL;
        /* cbat_string_split allocates from the default allocator, so free it. */
        if (line == NULL || cbat_string_split(records[i], ":", &parts) != CBAT_OK) {
            cbat_arena_free(scratch);
            return 1;
        }
        cbat_string_append_format(line, "%s (age %s) lives in ",
                                  cbat_string_cstr(CBAT_LIST_GET(parts, CBatString *, 0)),
                                  cbat_string_cstr(CBAT_LIST_GET(parts, CBatString *, 1)));
        cbat_string_append(line, cbat_string_cstr(CBAT_LIST_GET(parts, CBatString *, 2)));
        cbat_string_to_upper(line);
        printf("%s  [arena: %lu bytes]\n", cbat_string_cstr(line),
               (unsigned long)cbat_arena_bytes_used(scratch));
        cbat_list_free(parts);
        cbat_arena_reset(scratch); /* `line` is gone now */
    }
    cbat_arena_free(scratch);
    return 0;
}
