/*
 * cbat/map.h - insertion-ordered maps with fixed-size keys and values.
 *
 *     CBatMap *counts = cbat_map_new(sizeof(int), sizeof(int));
 *     CBAT_MAP_SET(counts, int, 42, int, 3);
 *     int count = CBAT_MAP_GET(counts, int, 42, int, 0);
 *     cbat_map_free(counts);
 *
 * Keys and values are copied as bytes. By default all key_size bytes are
 * hashed/compared, including padding: zero-initialize struct keys. Floating
 * keys use representation equality (-0.0 differs from +0.0; NaN payloads
 * may differ). Custom hash/equality callbacks must agree and remain stable.
 * Pointers returned by get/next are borrowed until the next modification.
 * Mutation, including removal, invalidates an active iterator. Objects have
 * no support for over-aligned compiler-extension types. A set operation may
 * compact storage even if a later allocation fails: refresh borrowed pointers
 * and restart iteration after any set/remove/clear call. Callbacks must not
 * mutate the map or change key representations.
 * There is no internal locking. Part of cbat. MIT License.
 */
#ifndef CBAT_MAP_H
#define CBAT_MAP_H

#include "cbat/base.h"

CBAT_BEGIN_DECLS

typedef struct CBatMap CBatMap;

/** Receives a pointer to the stored key representation. */
typedef uint64_t (*CBatHashFunction)(const void *key);
/** Must return true only for keys with the same hash. */
typedef bool (*CBatEqualFunction)(const void *a, const void *b);

/** New empty map; key_size must be nonzero (otherwise NULL). Values may be 0 bytes. */
CBAT_API CBatMap *cbat_map_new(size_t key_size, size_t value_size);
/** Same with an allocator (NULL selects the default). Allocator must outlive the map. */
CBAT_API CBatMap *cbat_map_new_with(size_t key_size, size_t value_size, const CBatAllocator *allocator);
/** Destroys owned entries and storage. NULL is ignored. */
CBAT_API void cbat_map_free(CBatMap *map);
/** Only on an empty map. Both callbacks NULL restores bytewise keys; one NULL is invalid. */
CBAT_API CBatError cbat_map_set_hash(CBatMap *map, CBatHashFunction hash, CBatEqualFunction equal);
/** Called on keys removed without transfer, cleared, or freed; not on replacement. */
CBAT_API void cbat_map_set_key_destructor(CBatMap *map, CBatDestructor destructor);
/** Called on values replaced, removed without transfer, cleared, or freed. */
CBAT_API void cbat_map_set_value_destructor(CBatMap *map, CBatDestructor destructor);
/** Live entry count; 0 for NULL. */
CBAT_API size_t cbat_map_length(const CBatMap *map);
/**
 * Inserts/replaces copied data. An existing key keeps its original key and
 * position. Ownership of pointed-to resources transfers only on success;
 * a replacement transfers only the new value, not the candidate key.
 * value may be NULL only for zero-sized values. Returns INVALID_ARGUMENT,
 * NO_MEMORY or OVERFLOW on failure, with entries unchanged. Input may point
 * into this map. Do not mutate keys in place or change hash/equality state.
 */
CBAT_API CBatError cbat_map_set(CBatMap *map, const void *key, const void *value);
/** Borrowed value pointer, or NULL for a missing/invalid key. */
CBAT_API void *cbat_map_get(const CBatMap *map, const void *key);
/** Stored value or fallback. */
CBAT_API void *cbat_map_get_or(const CBatMap *map, const void *key, void *fallback);
/** Whether a key is present (false for NULL arguments). */
CBAT_API bool cbat_map_contains(const CBatMap *map, const void *key);
/**
 * Removes a key, or returns NOT_FOUND/INVALID_ARGUMENT. Non-NULL outputs
 * receive copies and ownership (no destructor); otherwise that part is
 * destroyed. Outputs must be external storage, unchanged on failure.
 */
CBAT_API CBatError cbat_map_remove(CBatMap *map, const void *key, void *key_out, void *value_out);
/** Destroys all entries, retaining storage and callback settings. NULL is ignored. */
CBAT_API void cbat_map_clear(CBatMap *map);
/** Insertion order. Start *iterator at 0; outputs may be NULL; false at end/invalid input. */
CBAT_API bool cbat_map_next(const CBatMap *map, size_t *iterator, const void **key, void **value);

/** Typed scalar/pointer convenience; use addressable variables for struct keys/values. */
#define CBAT_MAP_SET(map, key_type, key, value_type, value) \
    cbat_map_set((map), &(key_type){ (key) }, &(value_type){ (value) })
/** Typed lookup with a fallback value. */
#define CBAT_MAP_GET(map, key_type, key, value_type, fallback) \
    (*(value_type *)cbat_map_get_or((map), &(key_type){ (key) }, &(value_type){ (fallback) }))

CBAT_END_DECLS
#endif /* CBAT_MAP_H */
