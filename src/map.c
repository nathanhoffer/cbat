/* Fixed-size-key compact ordered hash map. Part of cbat. MIT License. */
#include "cbat/map.h"
#include "cbat/hash.h"
#include <string.h>

/* C99 has no max_align_t. This union covers the fundamental C99 types. */
typedef union cbat__map_alignment {
    long double number;
    long long integer;
    void *pointer;
    void (*function)(void);
} cbat__map_alignment;

struct CBatMap {
    const CBatAllocator *allocator;
    size_t key_size, value_size, key_stride, value_stride;
    CBatHashFunction hash;
    CBatEqualFunction equal;
    CBatDestructor key_destructor, value_destructor;
    unsigned char *keys, *values, *alive;
    uint64_t *hashes;
    size_t count, length, capacity;
    size_t *index;
    size_t index_capacity, index_used;
};

static void *cbat__map_key(const CBatMap *map, size_t entry) {
    return map->keys + entry * map->key_stride;
}

static void *cbat__map_value(const CBatMap *map, size_t entry) {
    return map->values + entry * map->value_stride;
}

static uint64_t cbat__map_hash(const CBatMap *map, const void *key) {
    return map->hash != NULL ? map->hash(key) : cbat_fnv1a64(key, map->key_size);
}

static size_t cbat__map_find(const CBatMap *map, const void *key, uint64_t hash) {
    size_t i, probes;
    if (map->index_capacity == 0) return CBAT_NPOS;
    i = (size_t)hash & (map->index_capacity - 1);
    for (probes = 0; probes < map->index_capacity; probes++) {
        size_t slot = map->index[i];
        if (slot == 0) return CBAT_NPOS;
        if (slot != SIZE_MAX && map->hashes[slot - 1] == hash) {
            const void *stored = cbat__map_key(map, slot - 1);
            bool same = map->equal != NULL ? map->equal(stored, key) :
                memcmp(stored, key, map->key_size) == 0;
            if (same) return i;
        }
        i = (i + 1) & (map->index_capacity - 1);
    }
    return CBAT_NPOS;
}

static size_t cbat__map_place(size_t *index, size_t capacity, uint64_t hash, size_t entry) {
    size_t i = (size_t)hash & (capacity - 1);
    while (index[i] != 0 && index[i] != SIZE_MAX) i = (i + 1) & (capacity - 1);
    {
        size_t was_empty = index[i] == 0 ? 1 : 0;
        index[i] = entry + 1;
        return was_empty;
    }
}

static CBatError cbat__map_rebuild(CBatMap *map, size_t capacity) {
    size_t bytes, read, write = 0;
    size_t *fresh;
    if (!cbat_size_mul(capacity, sizeof(size_t), &bytes)) return CBAT_ERR_OVERFLOW;
    fresh = (size_t *)cbat_alloc_zeroed(map->allocator, bytes);
    if (fresh == NULL) return CBAT_ERR_NO_MEMORY;
    for (read = 0; read < map->count; read++) {
        if (!map->alive[read]) continue;
        if (write != read) {
            memmove(cbat__map_key(map, write), cbat__map_key(map, read), map->key_size);
            memmove(cbat__map_value(map, write), cbat__map_value(map, read), map->value_size);
            map->hashes[write] = map->hashes[read];
        }
        map->alive[write] = 1;
        cbat__map_place(fresh, capacity, map->hashes[write], write);
        write++;
    }
    cbat_release(map->allocator, map->index);
    map->index = fresh;
    map->index_capacity = capacity;
    map->index_used = write;
    map->count = write;
    return CBAT_OK;
}

static CBatError cbat__map_grow(CBatMap *map) {
    size_t capacity = map->capacity == 0 ? 4 : map->capacity * 2;
    size_t key_bytes, value_bytes, hash_bytes;
    unsigned char *keys, *values, *alive;
    uint64_t *hashes;
    if (capacity < map->capacity ||
        !cbat_size_mul(capacity, map->key_stride, &key_bytes) ||
        !cbat_size_mul(capacity, map->value_stride, &value_bytes) ||
        !cbat_size_mul(capacity, sizeof(uint64_t), &hash_bytes)) return CBAT_ERR_OVERFLOW;
    keys = (unsigned char *)cbat_alloc(map->allocator, key_bytes);
    values = (unsigned char *)cbat_alloc(map->allocator, value_bytes);
    hashes = (uint64_t *)cbat_alloc(map->allocator, hash_bytes);
    alive = (unsigned char *)cbat_alloc(map->allocator, capacity);
    if (keys == NULL || values == NULL || hashes == NULL || alive == NULL) {
        cbat_release(map->allocator, keys);
        cbat_release(map->allocator, values);
        cbat_release(map->allocator, hashes);
        cbat_release(map->allocator, alive);
        return CBAT_ERR_NO_MEMORY;
    }
    if (map->count > 0) {
        memcpy(keys, map->keys, map->count * map->key_stride);
        memcpy(values, map->values, map->count * map->value_stride);
        memcpy(hashes, map->hashes, map->count * sizeof(uint64_t));
        memcpy(alive, map->alive, map->count);
    }
    cbat_release(map->allocator, map->keys);
    cbat_release(map->allocator, map->values);
    cbat_release(map->allocator, map->hashes);
    cbat_release(map->allocator, map->alive);
    map->keys = keys; map->values = values; map->hashes = hashes; map->alive = alive;
    map->capacity = capacity;
    return CBAT_OK;
}

CBatMap *cbat_map_new_with(size_t key_size, size_t value_size, const CBatAllocator *allocator) {
    CBatMap *map;
    size_t alignment = sizeof(cbat__map_alignment);
    size_t key_stride, value_stride;
    if (key_size == 0 || !cbat_size_add(key_size, alignment - 1, &key_stride) ||
        !cbat_size_add(value_size == 0 ? 1 : value_size, alignment - 1, &value_stride)) return NULL;
    if (allocator == NULL) allocator = cbat_default_allocator();
    map = (CBatMap *)cbat_alloc_zeroed(allocator, sizeof(CBatMap));
    if (map == NULL) return NULL;
    map->allocator = allocator;
    map->key_size = key_size;
    map->value_size = value_size;
    map->key_stride = key_stride / alignment * alignment;
    map->value_stride = value_stride / alignment * alignment;
    return map;
}

CBatMap *cbat_map_new(size_t key_size, size_t value_size) {
    return cbat_map_new_with(key_size, value_size, NULL);
}

void cbat_map_clear(CBatMap *map) {
    size_t i;
    if (map == NULL) return;
    for (i = 0; i < map->count; i++) {
        if (!map->alive[i]) continue;
        if (map->key_destructor != NULL) map->key_destructor(cbat__map_key(map, i));
        if (map->value_destructor != NULL && map->value_size != 0)
            map->value_destructor(cbat__map_value(map, i));
    }
    map->count = map->length = map->index_used = 0;
    if (map->index != NULL) memset(map->index, 0, map->index_capacity * sizeof(size_t));
}

void cbat_map_free(CBatMap *map) {
    if (map == NULL) return;
    cbat_map_clear(map);
    cbat_release(map->allocator, map->keys);
    cbat_release(map->allocator, map->values);
    cbat_release(map->allocator, map->hashes);
    cbat_release(map->allocator, map->alive);
    cbat_release(map->allocator, map->index);
    cbat_release(map->allocator, map);
}

CBatError cbat_map_set_hash(CBatMap *map, CBatHashFunction hash, CBatEqualFunction equal) {
    if (map == NULL || map->length != 0 || (hash == NULL) != (equal == NULL))
        return CBAT_ERR_INVALID_ARGUMENT;
    cbat_map_clear(map);
    map->hash = hash; map->equal = equal;
    return CBAT_OK;
}

void cbat_map_set_key_destructor(CBatMap *map, CBatDestructor destructor) {
    if (map != NULL) map->key_destructor = destructor;
}

void cbat_map_set_value_destructor(CBatMap *map, CBatDestructor destructor) {
    if (map != NULL) map->value_destructor = destructor;
}

size_t cbat_map_length(const CBatMap *map) { return map == NULL ? 0 : map->length; }

CBatError cbat_map_set(CBatMap *map, const void *key, const void *value) {
    uint64_t hash;
    size_t slot, entry, bytes;
    unsigned char small[256];
    unsigned char *copy = small;
    CBatError err = CBAT_OK;
    if (map == NULL || key == NULL || (value == NULL && map->value_size != 0))
        return CBAT_ERR_INVALID_ARGUMENT;
    hash = cbat__map_hash(map, key);
    slot = cbat__map_find(map, key, hash);
    if (slot != CBAT_NPOS) {
        void *stored = cbat__map_value(map, map->index[slot] - 1);
        if (map->value_size != 0 && stored != value) {
            if (map->value_destructor != NULL) map->value_destructor(stored);
            memmove(stored, value, map->value_size);
        }
        return CBAT_OK;
    }
    /* Stage before growth/compaction so callers may use borrowed inputs. */
    if (!cbat_size_add(map->key_size, map->value_size, &bytes)) return CBAT_ERR_OVERFLOW;
    if (bytes > sizeof(small)) {
        copy = (unsigned char *)cbat_alloc(map->allocator, bytes);
        if (copy == NULL) return CBAT_ERR_NO_MEMORY;
    }
    memcpy(copy, key, map->key_size);
    if (map->value_size != 0) memcpy(copy + map->key_size, value, map->value_size);
    if (map->index_used + 1 > map->index_capacity - map->index_capacity / 4) {
        size_t capacity = map->index_capacity == 0 ? 8 : map->index_capacity;
        while (map->length + 1 > capacity - capacity / 4) {
            if (capacity > SIZE_MAX / 2) { err = CBAT_ERR_OVERFLOW; break; }
            capacity *= 2;
        }
        if (err == CBAT_OK) err = cbat__map_rebuild(map, capacity);
    }
    if (err == CBAT_OK && map->count == map->capacity) {
        err = map->count > map->length ? cbat__map_rebuild(map, map->index_capacity) :
            cbat__map_grow(map);
    }
    if (err == CBAT_OK) {
        entry = map->count;
        memcpy(cbat__map_key(map, entry), copy, map->key_size);
        if (map->value_size != 0) memcpy(cbat__map_value(map, entry), copy + map->key_size, map->value_size);
        map->hashes[entry] = hash;
        map->alive[entry] = 1;
        map->index_used += cbat__map_place(map->index, map->index_capacity, hash, entry);
        map->count++; map->length++;
    }
    if (copy != small) cbat_release(map->allocator, copy);
    return err;
}

void *cbat_map_get(const CBatMap *map, const void *key) {
    size_t slot;
    if (map == NULL || key == NULL) return NULL;
    slot = cbat__map_find(map, key, cbat__map_hash(map, key));
    return slot == CBAT_NPOS ? NULL : cbat__map_value(map, map->index[slot] - 1);
}

void *cbat_map_get_or(const CBatMap *map, const void *key, void *fallback) {
    void *value = cbat_map_get(map, key);
    return value == NULL ? fallback : value;
}

bool cbat_map_contains(const CBatMap *map, const void *key) { return cbat_map_get(map, key) != NULL; }

CBatError cbat_map_remove(CBatMap *map, const void *key, void *key_out, void *value_out) {
    size_t slot, entry;
    if (map == NULL || key == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    slot = cbat__map_find(map, key, cbat__map_hash(map, key));
    if (slot == CBAT_NPOS) return CBAT_ERR_NOT_FOUND;
    entry = map->index[slot] - 1;
    if (key_out != NULL) memcpy(key_out, cbat__map_key(map, entry), map->key_size);
    else if (map->key_destructor != NULL) map->key_destructor(cbat__map_key(map, entry));
    if (map->value_size != 0) {
        if (value_out != NULL) memcpy(value_out, cbat__map_value(map, entry), map->value_size);
        else if (map->value_destructor != NULL) map->value_destructor(cbat__map_value(map, entry));
    }
    map->alive[entry] = 0;
    map->index[slot] = SIZE_MAX;
    map->length--;
    while (map->count > 0 && !map->alive[map->count - 1]) map->count--;
    return CBAT_OK;
}

bool cbat_map_next(const CBatMap *map, size_t *iterator, const void **key, void **value) {
    size_t i;
    if (map == NULL || iterator == NULL) return false;
    for (i = *iterator; i < map->count; i++) {
        if (!map->alive[i]) continue;
        if (key != NULL) *key = cbat__map_key(map, i);
        if (value != NULL) *value = cbat__map_value(map, i);
        *iterator = i + 1;
        return true;
    }
    *iterator = map->count;
    return false;
}
