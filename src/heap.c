/* Binary min-heap. Part of cbat. MIT License. */
#include "cbat/heap.h"
#include "cbat/sort.h"
#include <string.h>

struct CBatHeap {
    CBatList *items;
    CBatCompare compare;
    bool reverse;
    void *scratch;
    const CBatAllocator *allocator;
};

static void *cbat__heap_slot(const CBatHeap *heap, size_t i) {
    return (unsigned char *)heap->items->data + i * heap->items->element_size;
}

static int cbat__heap_compare(const CBatHeap *heap, const void *a, const void *b) {
    int order = heap->compare(a, b);
    return heap->reverse ? (order < 0) - (order > 0) : order;
}

static void cbat__heap_down(CBatHeap *heap, size_t i) {
    size_t length = heap->items->length;
    while (i < length / 2) {
        size_t child = i * 2 + 1;
        if (child + 1 < length && cbat__heap_compare(heap, cbat__heap_slot(heap, child + 1),
                                                   cbat__heap_slot(heap, child)) < 0) child++;
        if (cbat__heap_compare(heap, cbat__heap_slot(heap, i), cbat__heap_slot(heap, child)) <= 0) break;
        cbat_swap(cbat__heap_slot(heap, i), cbat__heap_slot(heap, child), heap->items->element_size);
        i = child;
    }
}

static bool cbat__heap_internal_output(const CBatHeap *heap, const void *out) {
    uintptr_t address = (uintptr_t)out, base = (uintptr_t)heap->items->data;
    return out != NULL && heap->items->data != NULL && address >= base &&
        address - base < heap->items->capacity * heap->items->element_size;
}

CBatHeap *cbat_heap_new_with(size_t element_size, CBatCompare compare, const CBatAllocator *allocator) {
    CBatHeap *heap;
    if (element_size == 0 || compare == NULL) return NULL;
    if (allocator == NULL) allocator = cbat_default_allocator();
    heap = (CBatHeap *)cbat_alloc_zeroed(allocator, sizeof(CBatHeap));
    if (heap == NULL) return NULL;
    heap->allocator = allocator;
    heap->compare = compare;
    heap->items = cbat_list_new_with(element_size, allocator);
    heap->scratch = cbat_alloc(allocator, element_size);
    if (heap->items == NULL || heap->scratch == NULL) {
        cbat_heap_free(heap);
        return NULL;
    }
    return heap;
}

CBatHeap *cbat_heap_new(size_t element_size, CBatCompare compare) {
    return cbat_heap_new_with(element_size, compare, NULL);
}

void cbat_heap_free(CBatHeap *heap) {
    if (heap == NULL) return;
    cbat_list_free(heap->items);
    cbat_release(heap->allocator, heap->scratch);
    cbat_release(heap->allocator, heap);
}

void cbat_heap_set_destructor(CBatHeap *heap, CBatDestructor destructor) {
    if (heap != NULL) cbat_list_set_destructor(heap->items, destructor);
}

size_t cbat_heap_length(const CBatHeap *heap) { return heap == NULL ? 0 : heap->items->length; }
bool cbat_heap_is_empty(const CBatHeap *heap) { return cbat_heap_length(heap) == 0; }

CBatError cbat_heap_push(CBatHeap *heap, const void *element) {
    size_t i;
    CBatError err;
    if (heap == NULL || element == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat_list_push(heap->items, element);
    if (err != CBAT_OK) return err;
    i = heap->items->length - 1;
    while (i != 0) {
        size_t parent = (i - 1) / 2;
        if (cbat__heap_compare(heap, cbat__heap_slot(heap, parent), cbat__heap_slot(heap, i)) <= 0) break;
        cbat_swap(cbat__heap_slot(heap, parent), cbat__heap_slot(heap, i), heap->items->element_size);
        i = parent;
    }
    return CBAT_OK;
}

void *cbat_heap_peek(const CBatHeap *heap) {
    return cbat_heap_is_empty(heap) ? NULL : heap->items->data;
}

static void cbat__heap_deliver(CBatHeap *heap, void *element, void *out) {
    if (out != NULL) memmove(out, element, heap->items->element_size);
    else if (heap->items->destructor != NULL) heap->items->destructor(element);
}

CBatError cbat_heap_pop(CBatHeap *heap, void *out) {
    if (heap == NULL || cbat__heap_internal_output(heap, out)) return CBAT_ERR_INVALID_ARGUMENT;
    if (heap->items->length == 0) return CBAT_ERR_EMPTY;
    cbat__heap_deliver(heap, heap->items->data, out);
    heap->items->length--;
    if (heap->items->length != 0) {
        memcpy(heap->items->data, cbat__heap_slot(heap, heap->items->length), heap->items->element_size);
        cbat__heap_down(heap, 0);
    }
    return CBAT_OK;
}

CBatError cbat_heap_push_pop(CBatHeap *heap, const void *element, void *out) {
    if (heap == NULL || element == NULL || cbat__heap_internal_output(heap, out)) return CBAT_ERR_INVALID_ARGUMENT;
    memcpy(heap->scratch, element, heap->items->element_size);
    if (heap->items->length == 0 || cbat__heap_compare(heap, heap->scratch, heap->items->data) <= 0) {
        cbat__heap_deliver(heap, heap->scratch, out);
    } else {
        cbat__heap_deliver(heap, heap->items->data, out);
        memcpy(heap->items->data, heap->scratch, heap->items->element_size);
        cbat__heap_down(heap, 0);
    }
    return CBAT_OK;
}

static CBatError cbat__heap_from_list(const CBatList *list, CBatCompare compare, bool reverse, CBatHeap **out) {
    CBatHeap *heap;
    size_t i;
    CBatError err;
    if (list == NULL || compare == NULL || out == NULL || list->element_size == 0) return CBAT_ERR_INVALID_ARGUMENT;
    heap = cbat_heap_new_with(list->element_size, compare, list->allocator);
    if (heap == NULL) return CBAT_ERR_NO_MEMORY;
    heap->reverse = reverse;
    err = cbat_list_extend(heap->items, list->data, list->length);
    if (err != CBAT_OK) { cbat_heap_free(heap); return err; }
    i = heap->items->length / 2;
    while (i != 0) cbat__heap_down(heap, --i);
    *out = heap;
    return CBAT_OK;
}

CBatError cbat_heap_from_list(const CBatList *list, CBatCompare compare, CBatHeap **out) {
    return cbat__heap_from_list(list, compare, false, out);
}

static CBatError cbat__heap_select(const CBatList *list, size_t count, CBatCompare compare, bool largest, CBatList **out) {
    CBatHeap *heap = NULL;
    CBatList *result;
    CBatError err;
    size_t i;
    if (list == NULL || compare == NULL || out == NULL || list->element_size == 0) return CBAT_ERR_INVALID_ARGUMENT;
    result = cbat_list_new_with(list->element_size, list->allocator);
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    count = CBAT_MIN(count, list->length);
    err = cbat_list_resize(result, count);
    if (err == CBAT_OK && count != 0) err = cbat__heap_from_list(list, compare, largest, &heap);
    for (i = 0; i < count && err == CBAT_OK; i++) err = cbat_heap_pop(heap, cbat_list_get(result, i));
    cbat_heap_free(heap);
    if (err != CBAT_OK) { cbat_list_free(result); return err; }
    *out = result;
    return CBAT_OK;
}

CBatError cbat_list_smallest(const CBatList *list, size_t count, CBatCompare compare, CBatList **out) {
    return cbat__heap_select(list, count, compare, false, out);
}

CBatError cbat_list_largest(const CBatList *list, size_t count, CBatCompare compare, CBatList **out) {
    return cbat__heap_select(list, count, compare, true, out);
}
