/*
 * cbat/heap.h - min-priority queues, in the style of Python's heapq.
 * Entries are copied bytes and ordered by a CBatCompare callback. Callbacks
 * must not modify the heap. Borrowed peek pointers are read-only and valid
 * until the next modification. No internal locking. Part of cbat. MIT License.
 */
#ifndef CBAT_HEAP_H
#define CBAT_HEAP_H
#include "cbat/base.h"
#include "cbat/list.h"

CBAT_BEGIN_DECLS
typedef struct CBatHeap CBatHeap;

/** New min-heap; NULL for zero element_size, NULL compare, or OOM. */
CBAT_API CBatHeap *cbat_heap_new(size_t element_size, CBatCompare compare);
/** Same with a custom allocator (NULL = default), which must outlive the heap. */
CBAT_API CBatHeap *cbat_heap_new_with(size_t element_size, CBatCompare compare, const CBatAllocator *allocator);
/** Destroys remaining elements and storage. NULL is ignored. */
CBAT_API void cbat_heap_free(CBatHeap *heap);
/** Ownership callback for elements popped without an output or freed. */
CBAT_API void cbat_heap_set_destructor(CBatHeap *heap, CBatDestructor destructor);
/** Number of entries (0 for NULL). */
CBAT_API size_t cbat_heap_length(const CBatHeap *heap);
/** Whether there are no entries (true for NULL). */
CBAT_API bool cbat_heap_is_empty(const CBatHeap *heap);
/**
 * Copies an element, taking ownership of pointed-to resources on success
 * when a destructor is configured. O(log n); failure leaves the heap unchanged.
 * Copying bytes does not clone resources; do not insert duplicate ownership.
 */
CBAT_API CBatError cbat_heap_push(CBatHeap *heap, const void *element);
/**
 * Removes the minimum in O(log n). Non-NULL out receives ownership;
 * otherwise the destructor runs. EMPTY leaves outputs unchanged. Output must
 * be external storage (a pointer into heap storage gives INVALID_ARGUMENT).
 */
CBAT_API CBatError cbat_heap_pop(CBatHeap *heap, void *out);
/** Borrowed read-only minimum, or NULL for empty/NULL heap. */
CBAT_API void *cbat_heap_peek(const CBatHeap *heap);
/**
 * Pushes then pops, without growing storage. If the input <= minimum (or
 * heap is empty), returns/destroys the input and leaves the heap unchanged.
 * Otherwise returns/destroys the old minimum and keeps the input.
 * Same ownership/output rules as push/pop; no allocation after construction.
 */
CBAT_API CBatError cbat_heap_push_pop(CBatHeap *heap, const void *element, void *out);
/**
 * O(n) heap construction from copied list elements, using the list allocator.
 * The new heap has no destructor: pointed-to resources remain borrowed.
 * Output unchanged on error. The list is not modified.
 */
CBAT_API CBatError cbat_heap_from_list(const CBatList *list, CBatCompare compare, CBatHeap **out);
/** Up to count smallest items, ascending. New shallow list, no destructor; source unchanged. */
CBAT_API CBatError cbat_list_smallest(const CBatList *list, size_t count, CBatCompare compare, CBatList **out);
/** Up to count largest items, descending. New shallow list, no destructor; source unchanged. */
CBAT_API CBatError cbat_list_largest(const CBatList *list, size_t count, CBatCompare compare, CBatList **out);

/** Typed scalar/pointer push. For structs, pass the address of a local to cbat_heap_push. */
#define CBAT_HEAP_PUSH(heap, type, value) cbat_heap_push((heap), &(type){ (value) })
CBAT_END_DECLS
#endif /* CBAT_HEAP_H */
