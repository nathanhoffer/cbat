/* Contiguous row-major arrays and trailing-axis broadcasting. MIT License. */
#include "cbat/ndarray.h"
#include <math.h>
#include <string.h>

struct CBatArray {
    size_t dimensions, shape[4], strides[4], size;
    double *data;
    const CBatAllocator *allocator;
};
static CBatError cbat__array_shape(size_t dimensions, const size_t *shape, size_t *count, size_t strides[4]) {
    size_t i, total = 1;
    bool empty = false;
    if (dimensions > 4 || (dimensions != 0 && shape == NULL)) return CBAT_ERR_INVALID_ARGUMENT;
    for (i = 0; i < dimensions; i++) if (shape[i] == 0) empty = true;
    if (empty) { memset(strides, 0, 4*sizeof(size_t)); *count = 0; return CBAT_OK; }
    for (i = dimensions; i != 0; i--) {
        strides[i-1] = total;
        if (!cbat_size_mul(total, shape[i-1], &total)) return CBAT_ERR_OVERFLOW;
    }
    if (total > SIZE_MAX/sizeof(double)) return CBAT_ERR_OVERFLOW;
    *count = total; return CBAT_OK;
}
CBatError cbat_array_new_with(size_t dimensions, const size_t *shape, const CBatAllocator *allocator, CBatArray **out) {
    size_t count, strides[4] = {0};
    CBatArray *array;
    CBatError err;
    if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat__array_shape(dimensions, shape, &count, strides);
    if (err != CBAT_OK) return err;
    if (allocator == NULL) allocator = cbat_default_allocator();
    array = (CBatArray *)cbat_alloc_zeroed(allocator, sizeof(*array));
    if (array == NULL) return CBAT_ERR_NO_MEMORY;
    array->allocator = allocator; array->dimensions = dimensions; array->size = count;
    if (dimensions != 0) memcpy(array->shape, shape, dimensions*sizeof(size_t));
    memcpy(array->strides, strides, sizeof(strides));
    if (count != 0) {
        array->data = (double *)cbat_alloc_zeroed(allocator, count*sizeof(double));
        if (array->data == NULL) { cbat_array_free(array); return CBAT_ERR_NO_MEMORY; }
    }
    *out = array; return CBAT_OK;
}
CBatError cbat_array_new(size_t dimensions, const size_t *shape, CBatArray **out) { return cbat_array_new_with(dimensions, shape, NULL, out); }
CBatError cbat_array_from_values_with(size_t dimensions, const size_t *shape, const double *values, const CBatAllocator *allocator, CBatArray **out) {
    CBatArray *array = NULL;
    size_t count, strides[4];
    CBatError err;
    if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat__array_shape(dimensions, shape, &count, strides);
    if (err != CBAT_OK) return err;
    if (count != 0 && values == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat_array_new_with(dimensions, shape, allocator, &array);
    if (err != CBAT_OK) return err;
    if (array->size != 0) memcpy(array->data, values, array->size*sizeof(double));
    *out = array; return CBAT_OK;
}
CBatError cbat_array_from_values(size_t dimensions, const size_t *shape, const double *values, CBatArray **out) {
    return cbat_array_from_values_with(dimensions, shape, values, NULL, out);
}
void cbat_array_free(CBatArray *array) {
    if (array != NULL) { cbat_release(array->allocator, array->data); cbat_release(array->allocator, array); }
}
size_t cbat_array_dimensions(const CBatArray *array) { return array == NULL ? 0 : array->dimensions; }
size_t cbat_array_shape(const CBatArray *array, size_t axis) { return array == NULL || axis >= array->dimensions ? 0 : array->shape[axis]; }
size_t cbat_array_size(const CBatArray *array) { return array == NULL ? 0 : array->size; }
double *cbat_array_data(CBatArray *array) { return array == NULL ? NULL : array->data; }
const double *cbat_array_const_data(const CBatArray *array) { return array == NULL ? NULL : array->data; }

static CBatError cbat__array_offset(const CBatArray *array, const size_t *index, size_t *offset) {
    size_t i, result = 0;
    if (array == NULL || (array->dimensions != 0 && index == NULL)) return CBAT_ERR_INVALID_ARGUMENT;
    for (i = 0; i < array->dimensions; i++) {
        if (index[i] >= array->shape[i]) return CBAT_ERR_OUT_OF_RANGE;
        result += index[i]*array->strides[i];
    }
    *offset = result; return CBAT_OK;
}
CBatError cbat_array_get(const CBatArray *array, const size_t *index, double *out) {
    size_t offset; CBatError err;
    if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat__array_offset(array, index, &offset);
    if (err == CBAT_OK) *out = array->data[offset];
    return err;
}
CBatError cbat_array_set(CBatArray *array, const size_t *index, double value) {
    size_t offset; CBatError err = cbat__array_offset(array, index, &offset);
    if (err == CBAT_OK) array->data[offset] = value;
    return err;
}
CBatError cbat_array_reshape(CBatArray *array, size_t dimensions, const size_t *shape) {
    size_t count, strides[4] = {0}, new_shape[4] = {0};
    CBatError err;
    if (array == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat__array_shape(dimensions, shape, &count, strides);
    if (err != CBAT_OK) return err;
    if (count != array->size) return CBAT_ERR_INVALID_ARGUMENT;
    if (dimensions != 0) memcpy(new_shape, shape, dimensions*sizeof(size_t));
    array->dimensions = dimensions; memcpy(array->shape, new_shape, sizeof(new_shape)); memcpy(array->strides, strides, sizeof(strides));
    return CBAT_OK;
}
CBatError cbat_array_arange(double start, double stop, double step, CBatArray **out) {
    CBatArray *array = NULL;
    long double quotient;
    size_t count, i;
    CBatError err;
    if (out == NULL || !isfinite(start) || !isfinite(stop) || !isfinite(step) || step == 0) return CBAT_ERR_INVALID_ARGUMENT;
    quotient = ((long double)stop-(long double)start)/(long double)step;
    if (!isfinite(quotient)) quotient = (long double)(stop/step)-(long double)(start/step);
    if ((step > 0 && start >= stop) || (step < 0 && start <= stop)) quotient = 0;
    quotient = ceill(quotient);
    if (!isfinite(quotient) || quotient > (long double)(SIZE_MAX/sizeof(double))) return CBAT_ERR_OVERFLOW;
    count = quotient <= 0 ? 0 : (size_t)quotient;
    if (count != 0) {
        double last = fma((double)(count-1), step, start);
        if ((step > 0 && last >= stop) || (step < 0 && last <= stop)) count--;
    }
    err = cbat_array_new(1, &count, &array);
    if (err != CBAT_OK) return err;
    for (i = 0; i < count; i++) array->data[i] = fma((double)i, step, start);
    *out = array; return CBAT_OK;
}
CBatError cbat_array_linspace(double start, double stop, size_t count, CBatArray **out) {
    CBatArray *array = NULL; size_t i;
    CBatError err;
    if (out == NULL || !isfinite(start) || !isfinite(stop)) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat_array_new(1, &count, &array);
    if (err != CBAT_OK) return err;
    for (i = 0; i < count; i++) {
        double t = count < 2 ? 0 : (double)i/(double)(count-1);
        array->data[i] = i == 0 ? start : i == count-1 ? stop : (1-t)*start+t*stop;
    }
    *out = array; return CBAT_OK;
}
static CBatError cbat__array_binary(const CBatArray *a, const CBatArray *b, int op, CBatArray **out) {
    size_t shape[4] = {0}, dimensions, i, j;
    CBatArray *result = NULL;
    CBatError err;
    if (a == NULL || b == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    dimensions = a->dimensions > b->dimensions ? a->dimensions : b->dimensions;
    for (i = 0; i < dimensions; i++) {
        size_t x = i < dimensions-a->dimensions ? 1 : a->shape[i-(dimensions-a->dimensions)];
        size_t y = i < dimensions-b->dimensions ? 1 : b->shape[i-(dimensions-b->dimensions)];
        if (x == y) shape[i] = x;
        else if (x == 1) shape[i] = y;
        else if (y == 1) shape[i] = x;
        else return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat_array_new_with(dimensions, shape, a->allocator, &result);
    if (err != CBAT_OK) return err;
    for (i = 0; i < result->size; i++) {
        size_t x = 0, y = 0;
        double av, bv;
        for (j = 0; j < dimensions; j++) {
            size_t index = (i/result->strides[j])%shape[j];
            if (j >= dimensions-a->dimensions) { size_t axis = j-(dimensions-a->dimensions); if (a->shape[axis] != 1) x += index*a->strides[axis]; }
            if (j >= dimensions-b->dimensions) { size_t axis = j-(dimensions-b->dimensions); if (b->shape[axis] != 1) y += index*b->strides[axis]; }
        }
        av = a->data[x]; bv = b->data[y];
        result->data[i] = op == 0 ? av+bv : op == 1 ? av-bv : op == 2 ? av*bv : av/bv;
    }
    *out = result; return CBAT_OK;
}
CBatError cbat_array_add(const CBatArray *a, const CBatArray *b, CBatArray **out) { return cbat__array_binary(a,b,0,out); }
CBatError cbat_array_subtract(const CBatArray *a, const CBatArray *b, CBatArray **out) { return cbat__array_binary(a,b,1,out); }
CBatError cbat_array_multiply(const CBatArray *a, const CBatArray *b, CBatArray **out) { return cbat__array_binary(a,b,2,out); }
CBatError cbat_array_divide(const CBatArray *a, const CBatArray *b, CBatArray **out) { return cbat__array_binary(a,b,3,out); }
void cbat_array_scale(CBatArray *array, double factor) { size_t i; if (array != NULL) for (i = 0; i < array->size; i++) array->data[i] *= factor; }

static CBatError cbat__array_reduce(const CBatArray *array, size_t axis, bool mean, CBatArray **out) {
    size_t shape[4] = {0}, i, j, k, dimensions;
    CBatArray *result = NULL;
    CBatError err;
    if (array == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    if (axis >= array->dimensions) return CBAT_ERR_OUT_OF_RANGE;
    dimensions = array->dimensions-1;
    for (i = 0, j = 0; i < array->dimensions; i++) if (i != axis) shape[j++] = array->shape[i];
    err = cbat_array_new_with(dimensions, shape, array->allocator, &result);
    if (err != CBAT_OK) return err;
    for (i = 0; i < result->size; i++) {
        size_t base = 0;
        long double sum = 0;
        for (j = 0, k = 0; j < array->dimensions; j++) if (j != axis) {
            base += ((i/result->strides[k])%shape[k])*array->strides[j]; k++;
        }
        for (j = 0; j < array->shape[axis]; j++) sum += array->data[base+j*array->strides[axis]];
        result->data[i] = mean ? (array->shape[axis] == 0 ? NAN : (double)(sum/(long double)array->shape[axis])) : (double)sum;
    }
    *out = result; return CBAT_OK;
}
CBatError cbat_array_sum_axis(const CBatArray *array, size_t axis, CBatArray **out) { return cbat__array_reduce(array,axis,false,out); }
CBatError cbat_array_mean_axis(const CBatArray *array, size_t axis, CBatArray **out) { return cbat__array_reduce(array,axis,true,out); }

static CBatError cbat__array_print(const CBatArray *array, size_t axis, size_t offset, CBatString *text) {
    size_t i; CBatError err;
    if (axis == array->dimensions) return cbat_string_append_double(text, array->data[offset]);
    err = cbat_string_append_char(text, '[');
    for (i = 0; i < array->shape[axis] && err == CBAT_OK; i++) {
        if (i != 0) {
            size_t spaces;
            err = cbat_string_append(text, axis+1 == array->dimensions ? " " : "\n");
            if (axis+1 != array->dimensions) for (spaces = 0; spaces <= axis && err == CBAT_OK; spaces++) err = cbat_string_append_char(text, ' ');
        }
        if (err == CBAT_OK) err = cbat__array_print(array, axis+1, offset+i*array->strides[axis], text);
    }
    if (err == CBAT_OK) err = cbat_string_append_char(text, ']');
    return err;
}
CBatError cbat_array_to_string(const CBatArray *array, CBatString **out) {
    CBatString *text; CBatError err;
    if (array == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    text = cbat_string_new_with(array->allocator);
    if (text == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat__array_print(array, 0, 0, text);
    if (err != CBAT_OK) { cbat_string_free(text); return err; }
    *out = text; return CBAT_OK;
}
