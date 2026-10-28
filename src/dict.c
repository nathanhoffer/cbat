/*
 * cbat/dict.c - insertion-ordered hash map ("compact dict" layout).
 *
 * Entries live in parallel arrays in insertion order; a separate
 * open-addressing index (linear probing) maps hashes to entry positions.
 * Deleted entries leave a NULL key until the next rebuild compacts them.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/dict.h"

#include <string.h>

#include "cbat/hash.h"
#include "cbat/sort.h"

#define CBAT__DICT_EMPTY 0u
#define CBAT__DICT_TOMBSTONE SIZE_MAX
#define CBAT__DICT_MIN_INDEX 8u

struct CBatDict {
    const CBatAllocator *allocator;
    size_t value_size;
    CBatDestructor destructor;
    /* Entries, in insertion order. keys[e] == NULL marks a deleted entry. */
    char **keys;
    uint64_t *hashes;
    unsigned char *values;
    size_t entry_count;    /* used entry slots, live or deleted */
    size_t entry_capacity;
    size_t length;         /* live entries */
    /* Index: 0 = empty, SIZE_MAX = tombstone, otherwise entry + 1. */
    size_t *index;
    size_t index_capacity; /* 0 or a power of two */
    size_t index_used;     /* non-empty slots (live + tombstones) */
};

/* Address handed out for present keys when value_size is 0. */
static unsigned char cbat__dict_no_value;

static void *cbat__dict_value(const CBatDict *dict, size_t entry) {
    if (dict->value_size == 0) {
        return &cbat__dict_no_value;
    }
    return dict->values + entry * dict->value_size;
}

/* Returns the index slot holding `key`, or CBAT_NPOS. */
static size_t cbat__dict_find_slot(const CBatDict *dict, const char *key, uint64_t hash) {
    size_t mask;
    size_t i;
    size_t probes;
    if (dict->index_capacity == 0) {
        return CBAT_NPOS;
    }
    mask = dict->index_capacity - 1;
    i = (size_t)hash & mask;
    for (probes = 0; probes < dict->index_capacity; probes++) {
        size_t slot = dict->index[i];
        if (slot == CBAT__DICT_EMPTY) {
            return CBAT_NPOS;
        }
        if (slot != CBAT__DICT_TOMBSTONE) {
            size_t entry = slot - 1;
            if (dict->hashes[entry] == hash && strcmp(dict->keys[entry], key) == 0) {
                return i;
            }
        }
        i = (i + 1) & mask;
    }
    return CBAT_NPOS;
}

/* Puts `entry` into the first free slot of `index` (no duplicate check). */
static void cbat__dict_place(size_t *index, size_t capacity, uint64_t hash, size_t entry) {
    size_t mask = capacity - 1;
    size_t i = (size_t)hash & mask;
    while (index[i] != CBAT__DICT_EMPTY && index[i] != CBAT__DICT_TOMBSTONE) {
        i = (i + 1) & mask;
    }
    index[i] = entry + 1;
}

/* Compacts deleted entries and rebuilds the index with `capacity` slots. */
static CBatError cbat__dict_rebuild(CBatDict *dict, size_t capacity) {
    size_t bytes;
    size_t *fresh;
    size_t read;
    size_t write = 0;
    if (!cbat_size_mul(capacity, sizeof(size_t), &bytes)) {
        return CBAT_ERR_OVERFLOW;
    }
    fresh = (size_t *)cbat_alloc_zeroed(dict->allocator, bytes);
    if (fresh == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    for (read = 0; read < dict->entry_count; read++) {
        if (dict->keys[read] == NULL) {
            continue;
        }
        if (write != read) {
            dict->keys[write] = dict->keys[read];
            dict->hashes[write] = dict->hashes[read];
            if (dict->value_size > 0) {
                memcpy(dict->values + write * dict->value_size,
                       dict->values + read * dict->value_size, dict->value_size);
            }
        }
        cbat__dict_place(fresh, capacity, dict->hashes[write], write);
        write++;
    }
    cbat_release(dict->allocator, dict->index);
    dict->index = fresh;
    dict->index_capacity = capacity;
    dict->index_used = write;
    dict->entry_count = write;
    return CBAT_OK;
}

static CBatError cbat__dict_grow_entries(CBatDict *dict) {
    size_t capacity = dict->entry_capacity == 0 ? 4 : dict->entry_capacity * 2;
    size_t key_bytes;
    size_t hash_bytes;
    size_t value_bytes;
    void *fresh;
    if (capacity < dict->entry_capacity ||
        !cbat_size_mul(capacity, sizeof(char *), &key_bytes) ||
        !cbat_size_mul(capacity, sizeof(uint64_t), &hash_bytes) ||
        !cbat_size_mul(capacity, dict->value_size, &value_bytes)) {
        return CBAT_ERR_OVERFLOW;
    }
    /* Each array is resized independently; a failure part-way leaves the
       earlier arrays merely larger than needed, which is harmless. */
    fresh = cbat_resize(dict->allocator, dict->keys, dict->entry_capacity * sizeof(char *), key_bytes);
    if (fresh == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    dict->keys = (char **)fresh;
    fresh = cbat_resize(dict->allocator, dict->hashes, dict->entry_capacity * sizeof(uint64_t),
                        hash_bytes);
    if (fresh == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    dict->hashes = (uint64_t *)fresh;
    if (dict->value_size > 0) {
        fresh = cbat_resize(dict->allocator, dict->values,
                            dict->entry_capacity * dict->value_size, value_bytes);
        if (fresh == NULL) {
            return CBAT_ERR_NO_MEMORY;
        }
        dict->values = (unsigned char *)fresh;
    }
    dict->entry_capacity = capacity;
    return CBAT_OK;
}

CBatDict *cbat_dict_new_with(size_t value_size, const CBatAllocator *allocator) {
    CBatDict *dict;
    if (allocator == NULL) {
        allocator = cbat_default_allocator();
    }
    dict = (CBatDict *)cbat_alloc_zeroed(allocator, sizeof(CBatDict));
    if (dict == NULL) {
        return NULL;
    }
    dict->allocator = allocator;
    dict->value_size = value_size;
    return dict;
}

CBatDict *cbat_dict_new(size_t value_size) {
    return cbat_dict_new_with(value_size, NULL);
}

static void cbat__dict_destroy_entries(CBatDict *dict) {
    size_t e;
    for (e = 0; e < dict->entry_count; e++) {
        if (dict->keys[e] == NULL) {
            continue;
        }
        if (dict->destructor != NULL && dict->value_size > 0) {
            dict->destructor(cbat__dict_value(dict, e));
        }
        cbat_release(dict->allocator, dict->keys[e]);
        dict->keys[e] = NULL;
    }
}

void cbat_dict_free(CBatDict *dict) {
    if (dict == NULL) {
        return;
    }
    cbat__dict_destroy_entries(dict);
    cbat_release(dict->allocator, dict->keys);
    cbat_release(dict->allocator, dict->hashes);
    cbat_release(dict->allocator, dict->values);
    cbat_release(dict->allocator, dict->index);
    cbat_release(dict->allocator, dict);
}

void cbat_dict_set_destructor(CBatDict *dict, CBatDestructor destructor) {
    if (dict != NULL) {
        dict->destructor = destructor;
    }
}

size_t cbat_dict_length(const CBatDict *dict) {
    return dict == NULL ? 0 : dict->length;
}

size_t cbat_dict_value_size(const CBatDict *dict) {
    return dict == NULL ? 0 : dict->value_size;
}

CBatError cbat_dict_set(CBatDict *dict, const char *key, const void *value) {
    uint64_t hash;
    size_t slot;
    size_t entry;
    char *key_copy;
    CBatError err;
    if (dict == NULL || key == NULL || (value == NULL && dict->value_size > 0)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    hash = cbat_hash_cstring(key);
    slot = cbat__dict_find_slot(dict, key, hash);
    if (slot != CBAT_NPOS) {
        void *stored;
        entry = dict->index[slot] - 1;
        stored = cbat__dict_value(dict, entry);
        if (dict->value_size > 0 && stored != value) {
            if (dict->destructor != NULL) {
                dict->destructor(stored);
            }
            memcpy(stored, value, dict->value_size);
        }
        return CBAT_OK;
    }

    key_copy = cbat_strdup(dict->allocator, key);
    if (key_copy == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    /* Keep the index at most 3/4 full (tombstones count as full). */
    if ((dict->index_used + 1) * 4 > dict->index_capacity * 3) {
        size_t capacity = CBAT__DICT_MIN_INDEX;
        while (capacity < (dict->length + 1) * 2) {
            capacity *= 2;
        }
        err = cbat__dict_rebuild(dict, capacity);
        if (err != CBAT_OK) {
            cbat_release(dict->allocator, key_copy);
            return err;
        }
    }
    if (dict->entry_count == dict->entry_capacity) {
        if (dict->entry_count > dict->length) {
            err = cbat__dict_rebuild(dict, dict->index_capacity); /* reclaim deleted */
        } else {
            err = cbat__dict_grow_entries(dict);
        }
        if (err != CBAT_OK) {
            cbat_release(dict->allocator, key_copy);
            return err;
        }
    }
    entry = dict->entry_count;
    dict->keys[entry] = key_copy;
    dict->hashes[entry] = hash;
    if (dict->value_size > 0) {
        memcpy(cbat__dict_value(dict, entry), value, dict->value_size);
    }
    {
        /* Count the slot as newly used only if it was empty, not a tombstone. */
        size_t mask = dict->index_capacity - 1;
        size_t i = (size_t)hash & mask;
        while (dict->index[i] != CBAT__DICT_EMPTY && dict->index[i] != CBAT__DICT_TOMBSTONE) {
            i = (i + 1) & mask;
        }
        if (dict->index[i] == CBAT__DICT_EMPTY) {
            dict->index_used++;
        }
        dict->index[i] = entry + 1;
    }
    dict->entry_count++;
    dict->length++;
    return CBAT_OK;
}

void *cbat_dict_get(const CBatDict *dict, const char *key) {
    size_t slot;
    if (dict == NULL || key == NULL) {
        return NULL;
    }
    slot = cbat__dict_find_slot(dict, key, cbat_hash_cstring(key));
    if (slot == CBAT_NPOS) {
        return NULL;
    }
    return cbat__dict_value(dict, dict->index[slot] - 1);
}

void *cbat_dict_get_or(const CBatDict *dict, const char *key, void *fallback) {
    void *value = cbat_dict_get(dict, key);
    return value != NULL ? value : fallback;
}

bool cbat_dict_contains(const CBatDict *dict, const char *key) {
    return cbat_dict_get(dict, key) != NULL;
}

CBatError cbat_dict_remove(CBatDict *dict, const char *key, void *out) {
    size_t slot;
    size_t entry;
    if (dict == NULL || key == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    slot = cbat__dict_find_slot(dict, key, cbat_hash_cstring(key));
    if (slot == CBAT_NPOS) {
        return CBAT_ERR_NOT_FOUND;
    }
    entry = dict->index[slot] - 1;
    dict->index[slot] = CBAT__DICT_TOMBSTONE;
    if (dict->value_size > 0) {
        if (out != NULL) {
            memcpy(out, cbat__dict_value(dict, entry), dict->value_size);
        } else if (dict->destructor != NULL) {
            dict->destructor(cbat__dict_value(dict, entry));
        }
    }
    cbat_release(dict->allocator, dict->keys[entry]);
    dict->keys[entry] = NULL;
    dict->length--;
    /* Trailing deleted entries can be reused immediately. */
    while (dict->entry_count > 0 && dict->keys[dict->entry_count - 1] == NULL) {
        dict->entry_count--;
    }
    return CBAT_OK;
}

void cbat_dict_clear(CBatDict *dict) {
    if (dict == NULL) {
        return;
    }
    cbat__dict_destroy_entries(dict);
    dict->entry_count = 0;
    dict->length = 0;
    if (dict->index != NULL) {
        memset(dict->index, 0, dict->index_capacity * sizeof(size_t));
    }
    dict->index_used = 0;
}

bool cbat_dict_next(const CBatDict *dict, size_t *iterator, const char **key, void **value) {
    size_t e;
    if (dict == NULL || iterator == NULL) {
        return false;
    }
    for (e = *iterator; e < dict->entry_count; e++) {
        if (dict->keys[e] != NULL) {
            if (key != NULL) {
                *key = dict->keys[e];
            }
            if (value != NULL) {
                *value = cbat__dict_value(dict, e);
            }
            *iterator = e + 1;
            return true;
        }
    }
    *iterator = dict->entry_count;
    return false;
}

CBatError cbat_dict_keys(const CBatDict *dict, CBatList **out) {
    CBatList *keys;
    size_t it = 0;
    const char *key;
    CBatError err;
    if (dict == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    keys = cbat_list_new(sizeof(const char *));
    if (keys == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_list_reserve(keys, dict->length);
    if (err != CBAT_OK) {
        cbat_list_free(keys);
        return err;
    }
    while (cbat_dict_next(dict, &it, &key, NULL)) {
        cbat_list_push(keys, &key); /* cannot fail: reserved */
    }
    *out = keys;
    return CBAT_OK;
}

CBatError cbat_dict_get_or_insert(CBatDict *dict, const char *key, const void *value, void **out) {
    void *stored;
    CBatError err;
    if (dict == NULL || key == NULL || out == NULL ||
        (value == NULL && dict->value_size != 0)) return CBAT_ERR_INVALID_ARGUMENT;
    stored = cbat_dict_get(dict, key);
    if (stored == NULL) {
        err = cbat_dict_set(dict, key, value);
        if (err != CBAT_OK) return err;
        stored = cbat_dict_get(dict, key);
    }
    *out = stored;
    return CBAT_OK;
}

CBatError cbat_dict_increment(CBatDict *dict, const char *key, int64_t delta, int64_t *new_value) {
    int64_t old = 0, next;
    void *stored;
    CBatError err;
    if (dict == NULL || key == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (dict->value_size != sizeof(int64_t) || dict->destructor != NULL) return CBAT_ERR_TYPE;
    stored = cbat_dict_get(dict, key);
    if (stored != NULL) memcpy(&old, stored, sizeof(old));
    if ((delta > 0 && old > INT64_MAX - delta) ||
        (delta < 0 && old < INT64_MIN - delta)) return CBAT_ERR_OVERFLOW;
    next = old + delta;
    err = cbat_dict_set(dict, key, &next);
    if (err == CBAT_OK && new_value != NULL) *new_value = next;
    return err;
}

typedef struct cbat__dict_count {
    const char *key;
    int64_t count;
} cbat__dict_count;

static int cbat__dict_count_descending(const void *a, const void *b) {
    int64_t x = ((const cbat__dict_count *)a)->count;
    int64_t y = ((const cbat__dict_count *)b)->count;
    return (x < y) - (x > y);
}

CBatError cbat_dict_most_common(const CBatDict *dict, size_t limit, CBatList **out) {
    CBatList *pairs = NULL;
    CBatList *keys;
    size_t it = 0, i;
    const char *key;
    void *value;
    CBatError err = CBAT_OK;
    if (dict == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (dict->value_size != sizeof(int64_t)) return CBAT_ERR_TYPE;
    keys = cbat_list_new(sizeof(const char *));
    if (keys == NULL) return CBAT_ERR_NO_MEMORY;
    limit = CBAT_MIN(limit, dict->length);
    if (limit != 0) {
        pairs = cbat_list_new(sizeof(cbat__dict_count));
        if (pairs == NULL) err = CBAT_ERR_NO_MEMORY;
        if (err == CBAT_OK) err = cbat_list_reserve(pairs, dict->length);
        while (err == CBAT_OK && cbat_dict_next(dict, &it, &key, &value)) {
            cbat__dict_count pair;
            pair.key = key;
            memcpy(&pair.count, value, sizeof(pair.count));
            err = cbat_list_push(pairs, &pair);
        }
        if (err == CBAT_OK) err = cbat_list_sort(pairs, cbat__dict_count_descending);
        if (err == CBAT_OK) err = cbat_list_reserve(keys, limit);
        for (i = 0; i < limit && err == CBAT_OK; i++) {
            cbat__dict_count *pair = (cbat__dict_count *)cbat_list_get(pairs, i);
            err = cbat_list_push(keys, &pair->key);
        }
    }
    cbat_list_free(pairs);
    if (err != CBAT_OK) { cbat_list_free(keys); return err; }
    *out = keys;
    return CBAT_OK;
}

CBatError cbat_dict_update(CBatDict *dict, const CBatDict *other) {
    size_t it = 0;
    const char *key;
    void *value;
    if (dict == NULL || other == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (dict == other) return CBAT_OK;
    if (dict->value_size != other->value_size) return CBAT_ERR_TYPE;
    if (dict->destructor != NULL) return CBAT_ERR_UNSUPPORTED;
    while (cbat_dict_next(other, &it, &key, &value)) {
        CBatError err = cbat_dict_set(dict, key, value);
        if (err != CBAT_OK) return err;
    }
    return CBAT_OK;
}

CBatError cbat_dict_copy(const CBatDict *dict, CBatDict **out) {
    CBatDict *copy;
    CBatError err;
    if (dict == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    copy = cbat_dict_new_with(dict->value_size, dict->allocator);
    if (copy == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat_dict_update(copy, dict);
    if (err != CBAT_OK) { cbat_dict_free(copy); return err; }
    *out = copy;
    return CBAT_OK;
}
