/*
 * cbat/deque.c - ring-buffer deque.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/deque.h"

#include <string.h>

struct CBatDeque {
    unsigned char *data;
    size_t head;      /* physical index of the front element */
    size_t length;
    size_t capacity;  /* elements */
    size_t element_size;
    CBatDestructor destructor;
    const CBatAllocator *allocator;
};

static unsigned char *cbat__deque_slot(const CBatDeque *deque, size_t logical) {
    size_t physical = deque->head + logical;
    if (physical >= deque->capacity) {
        physical -= deque->capacity;
    }
    return deque->data + physical * deque->element_size;
}

/* Reallocates into a fresh buffer with the elements unwrapped from 0. */
static CBatError cbat__deque_grow(CBatDeque *deque, size_t needed) {
    size_t capacity = deque->capacity == 0 ? 8 : deque->capacity;
    size_t bytes;
    size_t first_part;
    unsigned char *fresh;
    if (needed <= deque->capacity) {
        return CBAT_OK;
    }
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if (!cbat_size_mul(capacity, deque->element_size, &bytes)) {
        return CBAT_ERR_OVERFLOW;
    }
    fresh = (unsigned char *)cbat_alloc(deque->allocator, bytes);
    if (fresh == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    if (deque->length > 0) {
        first_part = CBAT_MIN(deque->length, deque->capacity - deque->head);
        memcpy(fresh, deque->data + deque->head * deque->element_size,
               first_part * deque->element_size);
        memcpy(fresh + first_part * deque->element_size, deque->data,
               (deque->length - first_part) * deque->element_size);
    }
    cbat_release(deque->allocator, deque->data);
    deque->data = fresh;
    deque->head = 0;
    deque->capacity = capacity;
    return CBAT_OK;
}

CBatDeque *cbat_deque_new_with(size_t element_size, const CBatAllocator *allocator) {
    CBatDeque *deque;
    if (element_size == 0) {
        return NULL;
    }
    if (allocator == NULL) {
        allocator = cbat_default_allocator();
    }
    deque = (CBatDeque *)cbat_alloc_zeroed(allocator, sizeof(CBatDeque));
    if (deque == NULL) {
        return NULL;
    }
    deque->element_size = element_size;
    deque->allocator = allocator;
    return deque;
}

CBatDeque *cbat_deque_new(size_t element_size) {
    return cbat_deque_new_with(element_size, NULL);
}

void cbat_deque_free(CBatDeque *deque) {
    if (deque == NULL) {
        return;
    }
    cbat_deque_clear(deque);
    cbat_release(deque->allocator, deque->data);
    cbat_release(deque->allocator, deque);
}

void cbat_deque_set_destructor(CBatDeque *deque, CBatDestructor destructor) {
    if (deque != NULL) {
        deque->destructor = destructor;
    }
}

size_t cbat_deque_length(const CBatDeque *deque) {
    return deque == NULL ? 0 : deque->length;
}

bool cbat_deque_is_empty(const CBatDeque *deque) {
    return cbat_deque_length(deque) == 0;
}

CBatError cbat_deque_push_back(CBatDeque *deque, const void *element) {
    unsigned char temp[64];
    unsigned char *copy = NULL;
    CBatError err;
    if (deque == NULL || element == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (deque->length == SIZE_MAX) {
        return CBAT_ERR_OVERFLOW;
    }
    /* Growing frees the old buffer; copy `element` first if it lives there. */
    if (deque->length == deque->capacity && deque->data != NULL &&
        (const unsigned char *)element >= deque->data &&
        (const unsigned char *)element < deque->data + deque->capacity * deque->element_size) {
        if (deque->element_size <= sizeof(temp)) {
            copy = temp;
        } else {
            copy = (unsigned char *)cbat_alloc(deque->allocator, deque->element_size);
            if (copy == NULL) {
                return CBAT_ERR_NO_MEMORY;
            }
        }
        memcpy(copy, element, deque->element_size);
        element = copy;
    }
    err = cbat__deque_grow(deque, deque->length + 1);
    if (err == CBAT_OK) {
        memcpy(cbat__deque_slot(deque, deque->length), element, deque->element_size);
        deque->length++;
    }
    if (copy != NULL && copy != temp) {
        cbat_release(deque->allocator, copy);
    }
    return err;
}

CBatError cbat_deque_push_front(CBatDeque *deque, const void *element) {
    CBatError err;
    if (deque == NULL || element == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    /* Reuse push_back's growth (and aliasing care), then rotate it to the front. */
    err = cbat_deque_push_back(deque, element);
    if (err != CBAT_OK) {
        return err;
    }
    if (deque->length > 1) {
        unsigned char *last = cbat__deque_slot(deque, deque->length - 1);
        size_t new_head = deque->head == 0 ? deque->capacity - 1 : deque->head - 1;
        unsigned char *front = deque->data + new_head * deque->element_size;
        /* The slot before head is free: length < capacity before the push. */
        memcpy(front, last, deque->element_size);
        deque->head = new_head;
    }
    return CBAT_OK;
}

CBatError cbat_deque_pop_back(CBatDeque *deque, void *out) {
    unsigned char *slot;
    if (deque == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (deque->length == 0) {
        return CBAT_ERR_EMPTY;
    }
    slot = cbat__deque_slot(deque, deque->length - 1);
    if (out != NULL) {
        memcpy(out, slot, deque->element_size);
    } else if (deque->destructor != NULL) {
        deque->destructor(slot);
    }
    deque->length--;
    return CBAT_OK;
}

CBatError cbat_deque_pop_front(CBatDeque *deque, void *out) {
    unsigned char *slot;
    if (deque == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (deque->length == 0) {
        return CBAT_ERR_EMPTY;
    }
    slot = cbat__deque_slot(deque, 0);
    if (out != NULL) {
        memcpy(out, slot, deque->element_size);
    } else if (deque->destructor != NULL) {
        deque->destructor(slot);
    }
    deque->head++;
    if (deque->head == deque->capacity) {
        deque->head = 0;
    }
    deque->length--;
    return CBAT_OK;
}

void *cbat_deque_get(const CBatDeque *deque, size_t index) {
    if (deque == NULL || index >= deque->length) {
        return NULL;
    }
    return cbat__deque_slot(deque, index);
}

void *cbat_deque_front(const CBatDeque *deque) {
    return cbat_deque_get(deque, 0);
}

void *cbat_deque_back(const CBatDeque *deque) {
    if (deque == NULL || deque->length == 0) {
        return NULL;
    }
    return cbat__deque_slot(deque, deque->length - 1);
}

void cbat_deque_clear(CBatDeque *deque) {
    size_t i;
    if (deque == NULL) {
        return;
    }
    if (deque->destructor != NULL) {
        for (i = 0; i < deque->length; i++) {
            deque->destructor(cbat__deque_slot(deque, i));
        }
    }
    deque->length = 0;
    deque->head = 0;
}

CBatError cbat_deque_reserve(CBatDeque *deque, size_t additional) {
    size_t needed;
    if (deque == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (!cbat_size_add(deque->length, additional, &needed)) {
        return CBAT_ERR_OVERFLOW;
    }
    return cbat__deque_grow(deque, needed);
}
