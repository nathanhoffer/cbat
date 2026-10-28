/*
 * cbat/set.c - CBatSet, a thin layer over a value-less CBatDict.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/set.h"

#include "cbat/dict.h"

struct CBatSet {
    CBatDict *dict;
    const CBatAllocator *allocator;
};

CBatSet *cbat_set_new_with(const CBatAllocator *allocator) {
    CBatSet *set;
    if (allocator == NULL) {
        allocator = cbat_default_allocator();
    }
    set = (CBatSet *)cbat_alloc(allocator, sizeof(CBatSet));
    if (set == NULL) {
        return NULL;
    }
    set->dict = cbat_dict_new_with(0, allocator);
    if (set->dict == NULL) {
        cbat_release(allocator, set);
        return NULL;
    }
    set->allocator = allocator;
    return set;
}

CBatSet *cbat_set_new(void) {
    return cbat_set_new_with(NULL);
}

void cbat_set_free(CBatSet *set) {
    if (set == NULL) {
        return;
    }
    cbat_dict_free(set->dict);
    cbat_release(set->allocator, set);
}

size_t cbat_set_length(const CBatSet *set) {
    return set == NULL ? 0 : cbat_dict_length(set->dict);
}

CBatError cbat_set_add(CBatSet *set, const char *item) {
    if (set == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat_dict_set(set->dict, item, NULL);
}

CBatError cbat_set_remove(CBatSet *set, const char *item) {
    if (set == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat_dict_remove(set->dict, item, NULL);
}

void cbat_set_discard(CBatSet *set, const char *item) {
    if (set != NULL) {
        (void)cbat_dict_remove(set->dict, item, NULL);
    }
}

bool cbat_set_contains(const CBatSet *set, const char *item) {
    return set != NULL && cbat_dict_contains(set->dict, item);
}

void cbat_set_clear(CBatSet *set) {
    if (set != NULL) {
        cbat_dict_clear(set->dict);
    }
}

bool cbat_set_next(const CBatSet *set, size_t *iterator, const char **item) {
    if (set == NULL) {
        return false;
    }
    return cbat_dict_next(set->dict, iterator, item, NULL);
}

/* Adds to `out` every item of `from` for which contains(other) == want. */
static CBatError cbat__set_add_filtered(CBatSet *out, const CBatSet *from, const CBatSet *other,
                                        bool filter, bool want) {
    size_t it = 0;
    const char *item;
    while (cbat_set_next(from, &it, &item)) {
        if (!filter || cbat_set_contains(other, item) == want) {
            CBatError err = cbat_set_add(out, item);
            if (err != CBAT_OK) {
                return err;
            }
        }
    }
    return CBAT_OK;
}

static CBatError cbat__set_build(const CBatSet *a, const CBatSet *b, CBatSet **out,
                                 int operation) {
    CBatSet *result;
    CBatError err;
    if (a == NULL || b == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    result = cbat_set_new();
    if (result == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    switch (operation) {
    case 0: /* union */
        err = cbat__set_add_filtered(result, a, NULL, false, false);
        if (err == CBAT_OK) {
            err = cbat__set_add_filtered(result, b, NULL, false, false);
        }
        break;
    case 1: /* intersection */
        err = cbat__set_add_filtered(result, a, b, true, true);
        break;
    case 2: /* difference */
        err = cbat__set_add_filtered(result, a, b, true, false);
        break;
    default: /* symmetric difference */
        err = cbat__set_add_filtered(result, a, b, true, false);
        if (err == CBAT_OK) err = cbat__set_add_filtered(result, b, a, true, false);
        break;
    }
    if (err != CBAT_OK) {
        cbat_set_free(result);
        return err;
    }
    *out = result;
    return CBAT_OK;
}

CBatError cbat_set_union(const CBatSet *a, const CBatSet *b, CBatSet **out) {
    return cbat__set_build(a, b, out, 0);
}

CBatError cbat_set_intersection(const CBatSet *a, const CBatSet *b, CBatSet **out) {
    return cbat__set_build(a, b, out, 1);
}

CBatError cbat_set_difference(const CBatSet *a, const CBatSet *b, CBatSet **out) {
    return cbat__set_build(a, b, out, 2);
}

CBatError cbat_set_copy(const CBatSet *set, CBatSet **out) {
    CBatSet *copy;
    CBatError err;
    if (set == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    copy = cbat_set_new_with(set->allocator);
    if (copy == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat__set_add_filtered(copy, set, NULL, false, false);
    if (err != CBAT_OK) { cbat_set_free(copy); return err; }
    *out = copy;
    return CBAT_OK;
}

CBatError cbat_set_symmetric_difference(const CBatSet *a, const CBatSet *b, CBatSet **out) {
    return cbat__set_build(a, b, out, 3);
}

bool cbat_set_is_subset(const CBatSet *a, const CBatSet *b) {
    size_t it = 0;
    const char *item;
    if (a == NULL || b == NULL) {
        return false;
    }
    while (cbat_set_next(a, &it, &item)) {
        if (!cbat_set_contains(b, item)) {
            return false;
        }
    }
    return true;
}

bool cbat_set_equals(const CBatSet *a, const CBatSet *b) {
    return cbat_set_length(a) == cbat_set_length(b) && cbat_set_is_subset(a, b);
}

CBatError cbat_set_items(const CBatSet *set, CBatList **out) {
    if (set == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat_dict_keys(set->dict, out);
}
