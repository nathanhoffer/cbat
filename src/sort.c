/*
 * cbat/sort.c - stable merge sort and binary search.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/sort.h"

#include <string.h>

#define CBAT__SORT_INSERTION_LIMIT 16

void cbat_swap(void *a, void *b, size_t element_size) {
    unsigned char *pa = (unsigned char *)a;
    unsigned char *pb = (unsigned char *)b;
    size_t i;
    if (a == b) {
        return;
    }
    for (i = 0; i < element_size; i++) {
        unsigned char tmp = pa[i];
        pa[i] = pb[i];
        pb[i] = tmp;
    }
}

void cbat_reverse(void *base, size_t count, size_t element_size) {
    unsigned char *bytes = (unsigned char *)base;
    size_t i;
    if (base == NULL || count < 2) {
        return;
    }
    for (i = 0; i < count / 2; i++) {
        cbat_swap(bytes + i * element_size, bytes + (count - 1 - i) * element_size, element_size);
    }
}

/* Stable insertion sort by adjacent swaps; fine for short runs. */
static void cbat__sort_insertion(unsigned char *base, size_t count, size_t size,
                                 CBatCompare compare) {
    size_t i;
    for (i = 1; i < count; i++) {
        size_t j = i;
        while (j > 0 && compare(base + (j - 1) * size, base + j * size) > 0) {
            cbat_swap(base + (j - 1) * size, base + j * size, size);
            j--;
        }
    }
}

/* Bottom-up merge sort of `a` using `b` as scratch (its initial contents
   are irrelevant: every pass writes all `count` elements). Returns the
   buffer that holds the sorted result. */
static unsigned char *cbat__sort_merge(unsigned char *a, unsigned char *b, size_t count,
                                       size_t size, CBatCompare compare) {
    size_t width;
    size_t start;
    unsigned char *src = a;
    unsigned char *dst = b;
    /* Sort runs of the insertion limit first. */
    for (start = 0; start < count; start += CBAT__SORT_INSERTION_LIMIT) {
        size_t run = CBAT_MIN(CBAT__SORT_INSERTION_LIMIT, count - start);
        cbat__sort_insertion(src + start * size, run, size, compare);
    }
    for (width = CBAT__SORT_INSERTION_LIMIT; width < count; width *= 2) {
        for (start = 0; start < count; start += 2 * width) {
            size_t mid = CBAT_MIN(start + width, count);
            size_t end = CBAT_MIN(start + 2 * width, count);
            size_t i = start;
            size_t j = mid;
            size_t k = start;
            while (i < mid && j < end) {
                /* `<= 0` takes from the left run on ties: that is stability. */
                if (compare(src + i * size, src + j * size) <= 0) {
                    memcpy(dst + k * size, src + i * size, size);
                    i++;
                } else {
                    memcpy(dst + k * size, src + j * size, size);
                    j++;
                }
                k++;
            }
            if (i < mid) {
                memcpy(dst + k * size, src + i * size, (mid - i) * size);
            }
            if (j < end) {
                memcpy(dst + k * size, src + j * size, (end - j) * size);
            }
        }
        {
            unsigned char *tmp = src;
            src = dst;
            dst = tmp;
        }
    }
    return src;
}

CBatError cbat_sort(void *base, size_t count, size_t element_size, CBatCompare compare) {
    unsigned char *scratch;
    unsigned char *result;
    size_t total;
    if ((base == NULL && count > 0) || element_size == 0 || compare == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (count < 2) {
        return CBAT_OK;
    }
    if (count <= CBAT__SORT_INSERTION_LIMIT) {
        cbat__sort_insertion((unsigned char *)base, count, element_size, compare);
        return CBAT_OK;
    }
    if (!cbat_size_mul(count, element_size, &total)) {
        return CBAT_ERR_OVERFLOW;
    }
    scratch = (unsigned char *)cbat_alloc(NULL, total);
    if (scratch == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    result = cbat__sort_merge((unsigned char *)base, scratch, count, element_size, compare);
    if (result != base) {
        memcpy(base, result, total);
    }
    cbat_free(scratch);
    return CBAT_OK;
}

bool cbat_is_sorted(const void *base, size_t count, size_t element_size, CBatCompare compare) {
    const unsigned char *bytes = (const unsigned char *)base;
    size_t i;
    if (base == NULL || compare == NULL) {
        return count < 2;
    }
    for (i = 1; i < count; i++) {
        if (compare(bytes + (i - 1) * element_size, bytes + i * element_size) > 0) {
            return false;
        }
    }
    return true;
}

size_t cbat_lower_bound(const void *base, size_t count, size_t element_size,
                        const void *key, CBatCompare compare) {
    const unsigned char *bytes = (const unsigned char *)base;
    size_t low = 0;
    size_t high = count;
    if (base == NULL || compare == NULL) {
        return count;
    }
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (compare(bytes + mid * element_size, key) < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

size_t cbat_upper_bound(const void *base, size_t count, size_t element_size,
                        const void *key, CBatCompare compare) {
    const unsigned char *bytes = (const unsigned char *)base;
    size_t low = 0;
    size_t high = count;
    if (base == NULL || compare == NULL) {
        return count;
    }
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (compare(bytes + mid * element_size, key) <= 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

size_t cbat_binary_search(const void *base, size_t count, size_t element_size,
                          const void *key, CBatCompare compare) {
    size_t index = cbat_lower_bound(base, count, element_size, key, compare);
    if (index < count &&
        compare((const unsigned char *)base + index * element_size, key) == 0) {
        return index;
    }
    return CBAT_NPOS;
}

int cbat_compare_int(const void *a, const void *b) {
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

int cbat_compare_int64(const void *a, const void *b) {
    int64_t x = *(const int64_t *)a;
    int64_t y = *(const int64_t *)b;
    return (x > y) - (x < y);
}

int cbat_compare_uint64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

int cbat_compare_size(const void *a, const void *b) {
    size_t x = *(const size_t *)a;
    size_t y = *(const size_t *)b;
    return (x > y) - (x < y);
}

int cbat_compare_double(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;
    int x_nan = x != x;
    int y_nan = y != y;
    if (x_nan || y_nan) {
        return x_nan - y_nan;
    }
    return (x > y) - (x < y);
}

int cbat_compare_cstring(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    if (x == NULL || y == NULL) {
        return (x != NULL) - (y != NULL);
    }
    return strcmp(x, y);
}
