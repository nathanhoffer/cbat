/*
 * cbat/list.c - CBatList.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/list.h"

#include <string.h>

#include "cbat/sort.h"

#define CBAT__LIST_MIN_CAPACITY 4

static unsigned char *cbat__list_slot(const CBatList *list, size_t index) {
    return (unsigned char *)list->data + index * list->element_size;
}

static void cbat__list_destroy_range(CBatList *list, size_t start, size_t end) {
    size_t i;
    if (list->destructor == NULL) {
        return;
    }
    for (i = start; i < end; i++) {
        list->destructor(cbat__list_slot(list, i));
    }
}

/* Grows capacity to at least `needed` elements (amortized doubling). */
static CBatError cbat__list_grow(CBatList *list, size_t needed) {
    size_t new_capacity;
    size_t old_bytes;
    size_t new_bytes;
    void *fresh;
    if (needed <= list->capacity) {
        return CBAT_OK;
    }
    new_capacity = list->capacity < CBAT__LIST_MIN_CAPACITY ? CBAT__LIST_MIN_CAPACITY
                                                            : list->capacity;
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = needed;
            break;
        }
        new_capacity *= 2;
    }
    if (!cbat_size_mul(new_capacity, list->element_size, &new_bytes)) {
        return CBAT_ERR_OVERFLOW;
    }
    old_bytes = list->capacity * list->element_size;
    fresh = cbat_resize(list->allocator, list->data, old_bytes, new_bytes);
    if (fresh == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    list->data = fresh;
    list->capacity = new_capacity;
    return CBAT_OK;
}

CBatList *cbat_list_new_with(size_t element_size, const CBatAllocator *allocator) {
    CBatList *list;
    if (element_size == 0) {
        return NULL;
    }
    if (allocator == NULL) {
        allocator = cbat_default_allocator();
    }
    list = (CBatList *)cbat_alloc(allocator, sizeof(CBatList));
    if (list == NULL) {
        return NULL;
    }
    list->data = NULL;
    list->length = 0;
    list->capacity = 0;
    list->element_size = element_size;
    list->destructor = NULL;
    list->allocator = allocator;
    return list;
}

CBatList *cbat_list_new(size_t element_size) {
    return cbat_list_new_with(element_size, NULL);
}

void cbat_list_free(CBatList *list) {
    if (list == NULL) {
        return;
    }
    cbat__list_destroy_range(list, 0, list->length);
    cbat_release(list->allocator, list->data);
    cbat_release(list->allocator, list);
}

void cbat_list_destructor(void *element) {
    if (element != NULL) {
        cbat_list_free(*(CBatList **)element);
    }
}

void cbat_list_set_destructor(CBatList *list, CBatDestructor destructor) {
    if (list != NULL) {
        list->destructor = destructor;
    }
}

size_t cbat_list_length(const CBatList *list) {
    return list == NULL ? 0 : list->length;
}

bool cbat_list_is_empty(const CBatList *list) {
    return cbat_list_length(list) == 0;
}

void *cbat_list_get(const CBatList *list, size_t index) {
    if (list == NULL || index >= list->length) {
        return NULL;
    }
    return cbat__list_slot(list, index);
}

void *cbat_list_first(const CBatList *list) {
    return cbat_list_get(list, 0);
}

void *cbat_list_last(const CBatList *list) {
    if (list == NULL || list->length == 0) {
        return NULL;
    }
    return cbat__list_slot(list, list->length - 1);
}

CBatError cbat_list_set(CBatList *list, size_t index, const void *element) {
    unsigned char *slot;
    if (list == NULL || element == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (index >= list->length) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    slot = cbat__list_slot(list, index);
    if (slot == (const unsigned char *)element) {
        return CBAT_OK;
    }
    if (list->destructor != NULL) {
        list->destructor(slot);
    }
    memcpy(slot, element, list->element_size);
    return CBAT_OK;
}

CBatError cbat_list_push(CBatList *list, const void *element) {
    if (list == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat_list_insert(list, list->length, element);
}

CBatError cbat_list_pop(CBatList *list, void *out) {
    if (list == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (list->length == 0) {
        return CBAT_ERR_EMPTY;
    }
    return cbat_list_remove(list, list->length - 1, out);
}

CBatError cbat_list_insert(CBatList *list, size_t index, const void *element) {
    CBatError err;
    unsigned char *slot;
    unsigned char *temp = NULL;
    if (list == NULL || element == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (index > list->length) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    if (list->length == SIZE_MAX) {
        return CBAT_ERR_OVERFLOW;
    }
    /* `element` may point into our own storage, which growing would move. */
    if (list->data != NULL && (const unsigned char *)element >= (unsigned char *)list->data &&
        (const unsigned char *)element < cbat__list_slot(list, list->capacity)) {
        temp = (unsigned char *)cbat_alloc(list->allocator, list->element_size);
        if (temp == NULL) {
            return CBAT_ERR_NO_MEMORY;
        }
        memcpy(temp, element, list->element_size);
        element = temp;
    }
    err = cbat__list_grow(list, list->length + 1);
    if (err != CBAT_OK) {
        cbat_release(list->allocator, temp);
        return err;
    }
    slot = cbat__list_slot(list, index);
    memmove(slot + list->element_size, slot, (list->length - index) * list->element_size);
    memcpy(slot, element, list->element_size);
    list->length++;
    cbat_release(list->allocator, temp);
    return CBAT_OK;
}

CBatError cbat_list_remove(CBatList *list, size_t index, void *out) {
    unsigned char *slot;
    if (list == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (index >= list->length) {
        return list->length == 0 ? CBAT_ERR_EMPTY : CBAT_ERR_OUT_OF_RANGE;
    }
    slot = cbat__list_slot(list, index);
    if (out != NULL) {
        memcpy(out, slot, list->element_size);
    } else if (list->destructor != NULL) {
        list->destructor(slot);
    }
    memmove(slot, slot + list->element_size, (list->length - index - 1) * list->element_size);
    list->length--;
    return CBAT_OK;
}

CBatError cbat_list_extend(CBatList *list, const void *elements, size_t count) {
    size_t needed;
    size_t bytes;
    CBatError err;
    if (list == NULL || (elements == NULL && count > 0)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (count == 0) {
        return CBAT_OK;
    }
    if (!cbat_size_add(list->length, count, &needed) ||
        !cbat_size_mul(count, list->element_size, &bytes)) {
        return CBAT_ERR_OVERFLOW;
    }
    if (list->data != NULL && (const unsigned char *)elements >= (unsigned char *)list->data &&
        (const unsigned char *)elements < cbat__list_slot(list, list->capacity)) {
        /* Extending from ourselves: compute offset, then re-derive after growth. */
        size_t offset = (size_t)((const unsigned char *)elements - (unsigned char *)list->data);
        err = cbat__list_grow(list, needed);
        if (err != CBAT_OK) {
            return err;
        }
        memmove(cbat__list_slot(list, list->length), (unsigned char *)list->data + offset, bytes);
    } else {
        err = cbat__list_grow(list, needed);
        if (err != CBAT_OK) {
            return err;
        }
        memcpy(cbat__list_slot(list, list->length), elements, bytes);
    }
    list->length = needed;
    return CBAT_OK;
}

void cbat_list_clear(CBatList *list) {
    if (list == NULL) {
        return;
    }
    cbat__list_destroy_range(list, 0, list->length);
    list->length = 0;
}

CBatError cbat_list_reserve(CBatList *list, size_t additional) {
    size_t needed;
    if (list == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (!cbat_size_add(list->length, additional, &needed)) {
        return CBAT_ERR_OVERFLOW;
    }
    return cbat__list_grow(list, needed);
}

CBatError cbat_list_resize(CBatList *list, size_t new_length) {
    CBatError err;
    if (list == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (new_length <= list->length) {
        cbat__list_destroy_range(list, new_length, list->length);
        list->length = new_length;
        return CBAT_OK;
    }
    err = cbat__list_grow(list, new_length);
    if (err != CBAT_OK) {
        return err;
    }
    memset(cbat__list_slot(list, list->length), 0, (new_length - list->length) * list->element_size);
    list->length = new_length;
    return CBAT_OK;
}

CBatError cbat_list_shrink_to_fit(CBatList *list) {
    void *fresh;
    if (list == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (list->capacity == list->length) {
        return CBAT_OK;
    }
    if (list->length == 0) {
        cbat_release(list->allocator, list->data);
        list->data = NULL;
        list->capacity = 0;
        return CBAT_OK;
    }
    fresh = cbat_resize(list->allocator, list->data, list->capacity * list->element_size,
                        list->length * list->element_size);
    if (fresh == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    list->data = fresh;
    list->capacity = list->length;
    return CBAT_OK;
}

CBatList *cbat_list_copy(const CBatList *list) {
    CBatList *copy;
    if (list == NULL) {
        return NULL;
    }
    copy = cbat_list_new_with(list->element_size, list->allocator);
    if (copy == NULL) {
        return NULL;
    }
    if (cbat_list_extend(copy, list->data, list->length) != CBAT_OK) {
        cbat_list_free(copy);
        return NULL;
    }
    return copy;
}

size_t cbat_list_index_of(const CBatList *list, const void *element, CBatCompare compare) {
    size_t i;
    if (list == NULL || element == NULL || compare == NULL) {
        return CBAT_NPOS;
    }
    for (i = 0; i < list->length; i++) {
        if (compare(cbat__list_slot(list, i), element) == 0) {
            return i;
        }
    }
    return CBAT_NPOS;
}

bool cbat_list_contains(const CBatList *list, const void *element, CBatCompare compare) {
    return cbat_list_index_of(list, element, compare) != CBAT_NPOS;
}

CBatError cbat_list_sort(CBatList *list, CBatCompare compare) {
    if (list == NULL || compare == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat_sort(list->data, list->length, list->element_size, compare);
}

void cbat_list_reverse(CBatList *list) {
    if (list != NULL) {
        cbat_reverse(list->data, list->length, list->element_size);
    }
}

CBatError cbat_list_swap(CBatList *list, size_t i, size_t j) {
    if (list == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (i >= list->length || j >= list->length) {
        return CBAT_ERR_OUT_OF_RANGE;
    }
    cbat_swap(cbat__list_slot(list, i), cbat__list_slot(list, j), list->element_size);
    return CBAT_OK;
}
