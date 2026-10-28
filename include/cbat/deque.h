/*
 * cbat/deque.h - CBatDeque, a double-ended queue on a ring buffer
 * (cbat's answer to Python's collections.deque).
 *
 *     CBatDeque *queue = cbat_deque_new(sizeof(int));
 *     CBAT_DEQUE_PUSH_BACK(queue, int, 1);
 *     CBAT_DEQUE_PUSH_FRONT(queue, int, 0);
 *     int first;
 *     cbat_deque_pop_front(queue, &first);   // 0
 *     cbat_deque_free(queue);
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_DEQUE_H
#define CBAT_DEQUE_H

#include "cbat/base.h"

CBAT_BEGIN_DECLS

typedef struct CBatDeque CBatDeque;

/** NULL if element_size is 0 or memory runs out. */
CBAT_API CBatDeque *cbat_deque_new(size_t element_size);
CBAT_API CBatDeque *cbat_deque_new_with(size_t element_size, const CBatAllocator *allocator);
CBAT_API void cbat_deque_free(CBatDeque *deque);
CBAT_API void cbat_deque_set_destructor(CBatDeque *deque, CBatDestructor destructor);

CBAT_API size_t cbat_deque_length(const CBatDeque *deque);
CBAT_API bool cbat_deque_is_empty(const CBatDeque *deque);

CBAT_API CBatError cbat_deque_push_back(CBatDeque *deque, const void *element);
CBAT_API CBatError cbat_deque_push_front(CBatDeque *deque, const void *element);

/** Removes from an end; `out` (may be NULL) receives ownership. CBAT_ERR_EMPTY if empty. */
CBAT_API CBatError cbat_deque_pop_back(CBatDeque *deque, void *out);
CBAT_API CBatError cbat_deque_pop_front(CBatDeque *deque, void *out);

/** Pointer to the front/back element, or NULL when empty. */
CBAT_API void *cbat_deque_front(const CBatDeque *deque);
CBAT_API void *cbat_deque_back(const CBatDeque *deque);

/** Pointer to element `index` counted from the front, or NULL. */
CBAT_API void *cbat_deque_get(const CBatDeque *deque, size_t index);

CBAT_API void cbat_deque_clear(CBatDeque *deque);
CBAT_API CBatError cbat_deque_reserve(CBatDeque *deque, size_t additional);

#define CBAT_DEQUE_PUSH_BACK(deque, type, value) cbat_deque_push_back((deque), &(type){ (value) })
#define CBAT_DEQUE_PUSH_FRONT(deque, type, value) cbat_deque_push_front((deque), &(type){ (value) })

CBAT_END_DECLS

#endif /* CBAT_DEQUE_H */
