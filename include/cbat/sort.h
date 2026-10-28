/*
 * cbat/sort.h - stable sorting, binary search and ready-made comparators.
 *
 *     int values[] = {5, 2, 9};
 *     cbat_sort(values, 3, sizeof(int), cbat_compare_int);
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_SORT_H
#define CBAT_SORT_H

#include "cbat/base.h"

CBAT_BEGIN_DECLS

/**
 * Stable sort (equal elements keep their order, like Python's sorted()).
 * Uses a temporary buffer for arrays longer than 16 elements, so it can fail
 * with CBAT_ERR_NO_MEMORY; the array is unchanged in that case.
 */
CBAT_API CBatError cbat_sort(void *base, size_t count, size_t element_size, CBatCompare compare);

/** True when the array is in non-decreasing order. */
CBAT_API bool cbat_is_sorted(const void *base, size_t count, size_t element_size,
                             CBatCompare compare);

/** Reverses an array in place. */
CBAT_API void cbat_reverse(void *base, size_t count, size_t element_size);

/** Swaps two elements of `element_size` bytes. */
CBAT_API void cbat_swap(void *a, void *b, size_t element_size);

/** Index of the first element >= key in a sorted array (count if none). */
CBAT_API size_t cbat_lower_bound(const void *base, size_t count, size_t element_size,
                                 const void *key, CBatCompare compare);

/** Index of the first element > key in a sorted array (count if none). */
CBAT_API size_t cbat_upper_bound(const void *base, size_t count, size_t element_size,
                                 const void *key, CBatCompare compare);

/** Index of an element equal to key in a sorted array, or CBAT_NPOS. */
CBAT_API size_t cbat_binary_search(const void *base, size_t count, size_t element_size,
                                   const void *key, CBatCompare compare);

/* Comparators. Each receives pointers to elements of the named type. */
CBAT_API int cbat_compare_int(const void *a, const void *b);
CBAT_API int cbat_compare_int64(const void *a, const void *b);
CBAT_API int cbat_compare_uint64(const void *a, const void *b);
CBAT_API int cbat_compare_size(const void *a, const void *b);
/** Doubles; NaN sorts after every number. */
CBAT_API int cbat_compare_double(const void *a, const void *b);
/** Elements are `const char *` (NULL sorts first). Byte-wise, like strcmp. */
CBAT_API int cbat_compare_cstring(const void *a, const void *b);

CBAT_END_DECLS

#endif /* CBAT_SORT_H */
