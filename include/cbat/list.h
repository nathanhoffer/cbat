/*
 * cbat/list.h - CBatList, a growable array of fixed-size elements
 * (cbat's answer to Python's list).
 *
 *     CBatList *scores = cbat_list_new(sizeof(int));
 *     CBAT_LIST_PUSH(scores, int, 90);
 *     CBAT_LIST_PUSH(scores, int, 75);
 *     cbat_list_sort(scores, cbat_compare_int);
 *     CBAT_LIST_FOREACH(int, score, scores) {
 *         printf("%d\n", *score);
 *     }
 *     cbat_list_free(scores);
 *
 * Elements are copied in by value. A list can own what its elements point to
 * by setting a destructor (see cbat_list_set_destructor).
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_LIST_H
#define CBAT_LIST_H

#include "cbat/base.h"

CBAT_BEGIN_DECLS

/** Fields may be read freely; change them only through the functions below. */
typedef struct CBatList {
    void *data;            /* `length` contiguous elements */
    size_t length;
    size_t capacity;       /* elements allocated */
    size_t element_size;
    CBatDestructor destructor; /* NULL: elements own nothing */
    const CBatAllocator *allocator;
} CBatList;

/** New empty list. NULL if element_size is 0 or memory runs out. */
CBAT_API CBatList *cbat_list_new(size_t element_size);

/** Same, using `allocator` (NULL = default) for the list and its storage. */
CBAT_API CBatList *cbat_list_new_with(size_t element_size, const CBatAllocator *allocator);

/** Runs the destructor on every element, then frees the list. NULL is ignored. */
CBAT_API void cbat_list_free(CBatList *list);

/** A CBatDestructor for slots holding `CBatList *` (lists of lists). */
CBAT_API void cbat_list_destructor(void *element);

/** Sets the function run on elements that are removed, overwritten or freed. */
CBAT_API void cbat_list_set_destructor(CBatList *list, CBatDestructor destructor);

CBAT_API size_t cbat_list_length(const CBatList *list);
CBAT_API bool cbat_list_is_empty(const CBatList *list);

/** Pointer to element `index`, or NULL when out of range. */
CBAT_API void *cbat_list_get(const CBatList *list, size_t index);

/** Pointer to the first / last element, or NULL when empty. */
CBAT_API void *cbat_list_first(const CBatList *list);
CBAT_API void *cbat_list_last(const CBatList *list);

/** Overwrites element `index` (the old value is destroyed). */
CBAT_API CBatError cbat_list_set(CBatList *list, size_t index, const void *element);

/** Appends a copy of *element. */
CBAT_API CBatError cbat_list_push(CBatList *list, const void *element);

/**
 * Removes the last element. If `out` is non-NULL the element is copied there
 * and ownership passes to the caller; otherwise it is destroyed.
 * CBAT_ERR_EMPTY when the list is empty.
 */
CBAT_API CBatError cbat_list_pop(CBatList *list, void *out);

/** Inserts before `index` (index == length appends). */
CBAT_API CBatError cbat_list_insert(CBatList *list, size_t index, const void *element);

/** Removes element `index`, shifting later ones down. `out` as for pop. */
CBAT_API CBatError cbat_list_remove(CBatList *list, size_t index, void *out);

/** Appends `count` elements copied from a plain array. */
CBAT_API CBatError cbat_list_extend(CBatList *list, const void *elements, size_t count);

/** Destroys every element; keeps the allocated capacity. */
CBAT_API void cbat_list_clear(CBatList *list);

/** Ensures room for `additional` more elements without reallocating. */
CBAT_API CBatError cbat_list_reserve(CBatList *list, size_t additional);

/**
 * Changes the length. New elements are zero-filled; removed ones are
 * destroyed.
 */
CBAT_API CBatError cbat_list_resize(CBatList *list, size_t new_length);

/** Shrinks capacity to the current length. */
CBAT_API CBatError cbat_list_shrink_to_fit(CBatList *list);

/**
 * Shallow copy with the same element size and allocator. The copy has NO
 * destructor, so it never frees what the original's elements point to.
 */
CBAT_API CBatList *cbat_list_copy(const CBatList *list);

/** Index of the first element equal to *element under `compare`, or CBAT_NPOS. */
CBAT_API size_t cbat_list_index_of(const CBatList *list, const void *element, CBatCompare compare);

CBAT_API bool cbat_list_contains(const CBatList *list, const void *element, CBatCompare compare);

/** Stable sort. Fails only with CBAT_ERR_NO_MEMORY on long lists. */
CBAT_API CBatError cbat_list_sort(CBatList *list, CBatCompare compare);

CBAT_API void cbat_list_reverse(CBatList *list);

/** Swaps elements i and j. */
CBAT_API CBatError cbat_list_swap(CBatList *list, size_t i, size_t j);

/* ---- Typed convenience macros (scalar and pointer types) ---------------- */
/* For struct elements, call the functions directly with a pointer. */

/** CBAT_LIST_PUSH(list, int, 42) -> CBatError */
#define CBAT_LIST_PUSH(list, type, value) cbat_list_push((list), &(type){ (value) })

/** CBAT_LIST_INSERT(list, int, 0, 42) -> CBatError */
#define CBAT_LIST_INSERT(list, type, index, value) \
    cbat_list_insert((list), (index), &(type){ (value) })

/** CBAT_LIST_SET(list, int, 0, 42) -> CBatError */
#define CBAT_LIST_SET(list, type, index, value) \
    cbat_list_set((list), (index), &(type){ (value) })

/**
 * CBAT_LIST_GET(list, int, i) -> int. Like `array[i]`, this is UNCHECKED:
 * the index must be < length. Use cbat_list_get() for a checked pointer.
 */
#define CBAT_LIST_GET(list, type, index) (*(type *)cbat_list_get((list), (index)))

/** for (type *var = first; var != end; var++) over the list. */
#define CBAT_LIST_FOREACH(type, var, list)                                  \
    for (type *var = (type *)(list)->data;                                \
         var != NULL && var < (type *)(list)->data + (list)->length; var++)

CBAT_END_DECLS

#endif /* CBAT_LIST_H */
