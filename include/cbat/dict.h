/*
 * cbat/dict.h - CBatDict, a hash map from strings to fixed-size values that
 * remembers insertion order (cbat's answer to Python's dict).
 *
 *     CBatDict *ages = cbat_dict_new(sizeof(int));
 *     CBAT_DICT_SET(ages, "ada", int, 36);
 *     int *age = cbat_dict_get(ages, "ada");      // NULL if missing
 *
 *     size_t it = 0;
 *     const char *name;
 *     void *value;
 *     while (cbat_dict_next(ages, &it, &name, &value)) {
 *         printf("%s is %d\n", name, *(int *)value);
 *     }
 *     cbat_dict_free(ages);
 *
 * Keys are copied (the dict owns its copies). Values are copied by value; set
 * a destructor if values own memory. Pointers returned by get/next stay valid
 * until the dict is next modified.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_DICT_H
#define CBAT_DICT_H

#include "cbat/base.h"
#include "cbat/list.h"

CBAT_BEGIN_DECLS

typedef struct CBatDict CBatDict;

/** New empty dict whose values are `value_size` bytes (0 is allowed: a set). */
CBAT_API CBatDict *cbat_dict_new(size_t value_size);
CBAT_API CBatDict *cbat_dict_new_with(size_t value_size, const CBatAllocator *allocator);

/** Destroys every value and key, then the dict. NULL is ignored. */
CBAT_API void cbat_dict_free(CBatDict *dict);

/** Function run on values that are replaced, removed or freed. */
CBAT_API void cbat_dict_set_destructor(CBatDict *dict, CBatDestructor destructor);

CBAT_API size_t cbat_dict_length(const CBatDict *dict);
CBAT_API size_t cbat_dict_value_size(const CBatDict *dict);

/**
 * Inserts or replaces. `value` points to value_size bytes (may be NULL when
 * value_size is 0). Replacing destroys the old value but keeps the key's
 * original position in the iteration order.
 */
CBAT_API CBatError cbat_dict_set(CBatDict *dict, const char *key, const void *value);

/** Pointer to the stored value, or NULL when the key is missing. */
CBAT_API void *cbat_dict_get(const CBatDict *dict, const char *key);

/** Pointer to the stored value, or `fallback` when the key is missing. */
CBAT_API void *cbat_dict_get_or(const CBatDict *dict, const char *key, void *fallback);

CBAT_API bool cbat_dict_contains(const CBatDict *dict, const char *key);

/**
 * Removes a key. With non-NULL `out` the value is copied there and the caller
 * takes ownership; otherwise it is destroyed. CBAT_ERR_NOT_FOUND if missing.
 */
CBAT_API CBatError cbat_dict_remove(CBatDict *dict, const char *key, void *out);

/** Removes everything (keeps allocated tables). */
CBAT_API void cbat_dict_clear(CBatDict *dict);

/**
 * Iterates in insertion order. Start with *iterator = 0; returns false when
 * done. `key` and `value` may be NULL if not needed.
 */
CBAT_API bool cbat_dict_next(const CBatDict *dict, size_t *iterator, const char **key,
                             void **value);

/**
 * New list of the keys (as `const char *`, in insertion order). The pointers
 * are borrowed from the dict and valid until it is modified.
 */
CBAT_API CBatError cbat_dict_keys(const CBatDict *dict, CBatList **out);

/**
 * Returns a borrowed existing value, or inserts a copy of value first.
 * out must be non-NULL. Ownership transfers only if a new value is inserted.
 * Outputs unchanged on error; value may be NULL for zero-sized values.
 */
CBAT_API CBatError cbat_dict_get_or_insert(CBatDict *dict, const char *key, const void *value, void **out);

/**
 * Adds delta to an int64_t counter, starting missing keys at zero.
 * Requires sizeof(int64_t) values and no destructor (otherwise ERR_TYPE).
 * ERR_OVERFLOW leaves the value unchanged. new_value may be NULL.
 */
CBAT_API CBatError cbat_dict_increment(CBatDict *dict, const char *key, int64_t delta, int64_t *new_value);

/**
 * Up to limit keys sorted by int64_t value, descending (stable on ties).
 * ERR_TYPE unless value_size is sizeof(int64_t). limit 0 gives an empty list.
 * The new list owns its storage, but keys are borrowed until dict mutation.
 */
CBAT_API CBatError cbat_dict_most_common(const CBatDict *dict, size_t limit, CBatList **out);

/**
 * Copies keys and value bytes using the source allocator. The new dict has
 * no value destructor: pointed-to resources are borrowed, not cloned.
 * Outputs unchanged on failure; free the new dict with cbat_dict_free.
 */
CBAT_API CBatError cbat_dict_copy(const CBatDict *dict, CBatDict **out);

/**
 * Copies value bytes from other, keeping existing key order then appending
 * new keys. Sizes must match (ERR_TYPE). Self-update is a no-op.
 * To avoid transferring shared ownership, destination must have no destructor
 * (ERR_UNSUPPORTED otherwise). Pointed-to resources remain borrowed.
 * An allocation failure may leave earlier entries applied; both dicts stay valid.
 */
CBAT_API CBatError cbat_dict_update(CBatDict *dict, const CBatDict *other);

/* ---- Typed convenience macros (scalar and pointer types) ---------------- */

/** CBAT_DICT_SET(dict, "k", int, 5) -> CBatError */
#define CBAT_DICT_SET(dict, key, type, value) cbat_dict_set((dict), (key), &(type){ (value) })

/** CBAT_DICT_GET(dict, "k", int, 0) -> stored int, or the fallback 0. */
#define CBAT_DICT_GET(dict, key, type, fallback) \
    (*(type *)cbat_dict_get_or((dict), (key), &(type){ (fallback) }))

CBAT_END_DECLS

#endif /* CBAT_DICT_H */
