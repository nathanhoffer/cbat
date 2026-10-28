/* cbat/ndarray.h - owned contiguous row-major double arrays, 0..4 axes.
 * Zero axes represent a scalar (one value); zero-length axes are allowed.
 * Optional tier 2: include explicitly; CBAT_WITH_NDARRAY defaults ON.
 * Shape/data pointers are borrowed; array operations copy values, never views.
 * Objects require external synchronization for concurrent mutation.
 * MIT License.
 */
#ifndef CBAT_NDARRAY_H
#define CBAT_NDARRAY_H
#include "cbat/base.h"
#include "cbat/string.h"
CBAT_BEGIN_DECLS
typedef struct CBatArray CBatArray;
/** New zero-filled array; shape has dimensions entries (NULL for scalar).
 * INVALID_ARGUMENT for >4 axes; OVERFLOW for element/byte count overflow.
 * All fallible APIs leave output unchanged on failure. */
CBAT_API CBatError cbat_array_new(size_t dimensions, const size_t *shape, CBatArray **out);
/** Same with retained allocator (NULL = default), which must outlive array. */
CBAT_API CBatError cbat_array_new_with(size_t dimensions, const size_t *shape, const CBatAllocator *allocator, CBatArray **out);
/** Copies row-major values; NULL values allowed only for zero elements. */
CBAT_API CBatError cbat_array_from_values(size_t dimensions, const size_t *shape, const double *values, CBatArray **out);
/** Same with retained allocator. */
CBAT_API CBatError cbat_array_from_values_with(size_t dimensions, const size_t *shape, const double *values,
    const CBatAllocator *allocator, CBatArray **out);
/** [start,stop) with nonzero finite step; wrong direction returns empty vector.
 * Floating rounding can repeat values. Nonfinite arguments are INVALID_ARGUMENT. */
CBAT_API CBatError cbat_array_arange(double start, double stop, double step, CBatArray **out);
/** count evenly spaced values including endpoints; count 1 yields start, 0 empty. Finite endpoints required. */
CBAT_API CBatError cbat_array_linspace(double start, double stop, size_t count, CBatArray **out);
/** Frees array and data; NULL ignored. */
CBAT_API void cbat_array_free(CBatArray *array);
/** Rank, 0 for scalar or NULL. */
CBAT_API size_t cbat_array_dimensions(const CBatArray *array);
/** Extent of axis, 0 for invalid array/axis. */
CBAT_API size_t cbat_array_shape(const CBatArray *array, size_t axis);
/** Element count (scalar = 1), 0 for NULL. */
CBAT_API size_t cbat_array_size(const CBatArray *array);
/** Borrowed mutable contiguous data; NULL for empty/NULL array. */
CBAT_API double *cbat_array_data(CBatArray *array);
/** Borrowed read-only data; same lifetime as data. */
CBAT_API const double *cbat_array_const_data(const CBatArray *array);
/** Reads indexed value. index has rank entries (NULL allowed for scalar). */
CBAT_API CBatError cbat_array_get(const CBatArray *array, const size_t *index, double *out);
/** Writes indexed value, allowing all double values including NaN/infinity. */
CBAT_API CBatError cbat_array_set(CBatArray *array, const size_t *index, double value);
/** Changes shape without moving data; same element count required. Atomic on error. */
CBAT_API CBatError cbat_array_reshape(CBatArray *array, size_t dimensions, const size_t *shape);
/** Elementwise addition with NumPy trailing-axis broadcasting. New result uses a's allocator.
 * Incompatible shapes give INVALID_ARGUMENT. IEEE NaN/infinity propagation. */
CBAT_API CBatError cbat_array_add(const CBatArray *a, const CBatArray *b, CBatArray **out);
/** Broadcast subtraction, same rules as add. */
CBAT_API CBatError cbat_array_subtract(const CBatArray *a, const CBatArray *b, CBatArray **out);
/** Broadcast multiplication, same rules as add. */
CBAT_API CBatError cbat_array_multiply(const CBatArray *a, const CBatArray *b, CBatArray **out);
/** Broadcast division; IEEE division by zero yields infinity/NaN. */
CBAT_API CBatError cbat_array_divide(const CBatArray *a, const CBatArray *b, CBatArray **out);
/** Multiplies all values in place; NULL ignored. */
CBAT_API void cbat_array_scale(CBatArray *array, double factor);
/** Reduces axis, removing it (vector -> scalar). Empty-axis sum is zero.
 * Result uses input allocator; OUT_OF_RANGE for invalid axis. */
CBAT_API CBatError cbat_array_sum_axis(const CBatArray *array, size_t axis, CBatArray **out);
/** Same, dividing by axis extent. Empty-axis mean is NaN. */
CBAT_API CBatError cbat_array_mean_axis(const CBatArray *array, size_t axis, CBatArray **out);
/** New owned nested-bracket text, spaces between numbers and newlines between
 * subarrays; scalar has no brackets. No truncation. Uses array allocator. */
CBAT_API CBatError cbat_array_to_string(const CBatArray *array, CBatString **out);
CBAT_END_DECLS
#endif
