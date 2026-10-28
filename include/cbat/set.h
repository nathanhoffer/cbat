/*
 * cbat/set.h - CBatSet, an insertion-ordered set of strings
 * (cbat's answer to Python's set).
 *
 *     CBatSet *seen = cbat_set_new();
 *     cbat_set_add(seen, "apple");
 *     if (cbat_set_contains(seen, "apple")) { ... }
 *     cbat_set_free(seen);
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_SET_H
#define CBAT_SET_H

#include "cbat/base.h"
#include "cbat/list.h"

CBAT_BEGIN_DECLS

typedef struct CBatSet CBatSet;

CBAT_API CBatSet *cbat_set_new(void);
CBAT_API CBatSet *cbat_set_new_with(const CBatAllocator *allocator);
CBAT_API void cbat_set_free(CBatSet *set);

CBAT_API size_t cbat_set_length(const CBatSet *set);

/** Adds a copy of `item`. Adding an existing item is a no-op (CBAT_OK). */
CBAT_API CBatError cbat_set_add(CBatSet *set, const char *item);

/** CBAT_ERR_NOT_FOUND if `item` is absent (use cbat_set_discard to ignore). */
CBAT_API CBatError cbat_set_remove(CBatSet *set, const char *item);

/** Removes `item` if present. */
CBAT_API void cbat_set_discard(CBatSet *set, const char *item);

CBAT_API bool cbat_set_contains(const CBatSet *set, const char *item);
CBAT_API void cbat_set_clear(CBatSet *set);

/** Iterates in insertion order, starting from *iterator = 0. */
CBAT_API bool cbat_set_next(const CBatSet *set, size_t *iterator, const char **item);

/** New set with the items of both (a's order, then b's new items). */
CBAT_API CBatError cbat_set_union(const CBatSet *a, const CBatSet *b, CBatSet **out);

/** New set with the items in both. */
CBAT_API CBatError cbat_set_intersection(const CBatSet *a, const CBatSet *b, CBatSet **out);

/** New set with the items of `a` that are not in `b`. */
CBAT_API CBatError cbat_set_difference(const CBatSet *a, const CBatSet *b, CBatSet **out);

/** Independent copy of owned keys, using the source allocator. Outputs unchanged on error. */
CBAT_API CBatError cbat_set_copy(const CBatSet *set, CBatSet **out);

/** Items in exactly one set: a-only items then b-only items, in insertion order. */
CBAT_API CBatError cbat_set_symmetric_difference(const CBatSet *a, const CBatSet *b, CBatSet **out);

/** True when every item of `a` is in `b`. */
CBAT_API bool cbat_set_is_subset(const CBatSet *a, const CBatSet *b);

/** True when both contain exactly the same items. */
CBAT_API bool cbat_set_equals(const CBatSet *a, const CBatSet *b);

/** New list of borrowed `const char *` items in insertion order. */
CBAT_API CBatError cbat_set_items(const CBatSet *set, CBatList **out);

CBAT_END_DECLS

#endif /* CBAT_SET_H */
