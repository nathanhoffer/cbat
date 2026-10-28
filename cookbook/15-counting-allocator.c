/* Recipe 15: Counting allocator wrapping the default allocator.
 * PROBLEM: Measure live and peak requested bytes for a group of containers.
 * SOLUTION: Prepend an aligned size header, forwarding allocations to the
 * original allocator. Resize changes counters only after success.
 * DISCUSSION: Counts payload bytes, excluding metadata/allocator overhead.
 * The union preserves fundamental C99 alignment. The allocator and context
 * must outlive every object using them; this example is single-threaded.
 */
#include "cbat/cbat.h"
#include <stdio.h>

typedef union AllocationHeader {
    size_t size;
    long double align_number;
    void *align_pointer;
    void (*align_function)(void);
    uint64_t align_integer;
} AllocationHeader;
typedef struct CountingAllocator {
    const CBatAllocator *backing;
    size_t live, peak;
} CountingAllocator;

static void *count_alloc(void *context, size_t size) {
    CountingAllocator *count = (CountingAllocator *)context;
    AllocationHeader *header;
    size_t total, live;
    if (!cbat_size_add(sizeof(AllocationHeader), size, &total) ||
        !cbat_size_add(count->live, size, &live)) return NULL;
    header = (AllocationHeader *)cbat_alloc(count->backing, total);
    if (header == NULL) return NULL;
    header->size = size; count->live = live;
    if (live > count->peak) count->peak = live;
    return header + 1;
}
static void count_release(void *context, void *pointer) {
    CountingAllocator *count = (CountingAllocator *)context;
    AllocationHeader *header;
    if (pointer == NULL) return;
    header = (AllocationHeader *)pointer - 1;
    count->live -= header->size; cbat_release(count->backing, header);
}
static void *count_resize(void *context, void *pointer, size_t old_size, size_t size) {
    CountingAllocator *count = (CountingAllocator *)context;
    AllocationHeader *header, *resized;
    size_t total, live;
    (void)old_size;
    if (pointer == NULL) return count_alloc(context, size);
    header = (AllocationHeader *)pointer - 1;
    if (!cbat_size_add(sizeof(AllocationHeader), size, &total) ||
        !cbat_size_add(count->live - header->size, size, &live)) return NULL;
    resized = (AllocationHeader *)cbat_resize(count->backing, header,
        sizeof(AllocationHeader) + header->size, total);
    if (resized == NULL) return NULL;
    resized->size = size; count->live = live;
    if (live > count->peak) count->peak = live;
    return resized + 1;
}

int main(void) {
    CountingAllocator count = {NULL, 0, 0};
    CBatAllocator allocator = {count_alloc, count_resize, count_release, &count};
    CBatList *values;
    CBatError err = CBAT_OK;
    int i;
    count.backing = cbat_default_allocator();
    values = cbat_list_new_with(sizeof(int), &allocator);
    if (values == NULL) return 1;
    for (i = 0; i < 1000; i++) {
        err = cbat_list_push(values, &i);
        if (err != CBAT_OK) break;
    }
    printf("Live: %lu bytes; peak: %lu bytes\n", (unsigned long)count.live, (unsigned long)count.peak);
    cbat_list_free(values);
    printf("After free: %lu bytes\n", (unsigned long)count.live);
    if (err != CBAT_OK) fprintf(stderr, "%s\n", cbat_error_str(err));
    return err == CBAT_OK && count.live == 0 ? 0 : 1;
}
