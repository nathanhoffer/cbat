/* Dense double matrix algebra and partial-pivoted LU. MIT License. */
#include "cbat/linalg.h"
#include <math.h>
#include <limits.h>
#include <string.h>

static size_t cbat__linalg_product(const CBatArray *a, size_t from, size_t to) {
    size_t product = 1, i;
    for (i = from; i < to; i++) product *= cbat_array_shape(a, i);
    return product;
}
CBatError cbat_linalg_matmul(const CBatArray *a, const CBatArray *b, CBatArray **out) {
    size_t ad, bd, ab, bb, batches, shape[4] = {0}, batch_count = 1, rows, inner, columns, dimensions;
    size_t batch, i, j, k;
    bool av, bv;
    CBatArray *result = NULL;
    const double *ap, *bp;
    double *rp;
    CBatError err;
    if (a == NULL || b == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    ad = cbat_array_dimensions(a); bd = cbat_array_dimensions(b);
    if (ad == 0 || bd == 0) return CBAT_ERR_INVALID_ARGUMENT;
    av = ad == 1; bv = bd == 1; ab = av ? 0 : ad-2; bb = bv ? 0 : bd-2;
    rows = av ? 1 : cbat_array_shape(a, ad-2); inner = cbat_array_shape(a, ad-1);
    columns = bv ? 1 : cbat_array_shape(b, bd-1);
    if (inner != cbat_array_shape(b, bv ? 0 : bd-2)) return CBAT_ERR_INVALID_ARGUMENT;
    batches = ab > bb ? ab : bb;
    for (i = 0; i < batches; i++) {
        size_t x = i < batches-ab ? 1 : cbat_array_shape(a, i-(batches-ab));
        size_t y = i < batches-bb ? 1 : cbat_array_shape(b, i-(batches-bb));
        if (x == y) shape[i] = x;
        else if (x == 1) shape[i] = y;
        else if (y == 1) shape[i] = x;
        else return CBAT_ERR_INVALID_ARGUMENT;
        if (!cbat_size_mul(batch_count, shape[i], &batch_count)) return CBAT_ERR_OVERFLOW;
    }
    dimensions = batches;
    if (!av) shape[dimensions++] = rows;
    if (!bv) shape[dimensions++] = columns;
    err = cbat_array_new(dimensions, shape, &result);
    if (err != CBAT_OK) return err;
    ap = cbat_array_const_data(a); bp = cbat_array_const_data(b); rp = cbat_array_data(result);
    if (cbat_array_size(result) == 0) { *out = result; return CBAT_OK; }
    for (batch = 0; batch < batch_count; batch++) {
        size_t ai = 0, bi = 0, remainder = batch;
        for (i = batches; i != 0; i--) {
            size_t index = remainder%shape[i-1]; remainder /= shape[i-1];
            if (i-1 >= batches-ab) {
                size_t axis = i-1-(batches-ab);
                if (cbat_array_shape(a, axis) != 1) ai += index*cbat__linalg_product(a, axis+1, ad);
            }
            if (i-1 >= batches-bb) {
                size_t axis = i-1-(batches-bb);
                if (cbat_array_shape(b, axis) != 1) bi += index*cbat__linalg_product(b, axis+1, bd);
            }
        }
        for (i = 0; i < rows; i++) for (j = 0; j < columns; j++) {
            double sum = 0;
            for (k = 0; k < inner; k++) sum += ap[ai+i*inner+k]*bp[bi+k*columns+j];
            rp[(batch*rows+i)*columns+j] = sum;
        }
    }
    *out = result; return CBAT_OK;
}
CBatError cbat_linalg_transpose(const CBatArray *a, CBatArray **out) {
    size_t dimensions, shape[4] = {0}, i, j;
    CBatArray *result = NULL;
    CBatError err;
    if (a == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    dimensions = cbat_array_dimensions(a);
    for (i = 0; i < dimensions; i++) shape[i] = cbat_array_shape(a, dimensions-1-i);
    err = cbat_array_new(dimensions, shape, &result);
    if (err != CBAT_OK) return err;
    for (i = 0; i < cbat_array_size(result); i++) {
        size_t remainder = i, offset = 0;
        for (j = 0; j < dimensions; j++) {
            size_t index = remainder%cbat_array_shape(a, j); remainder /= cbat_array_shape(a, j);
            offset += index*cbat__linalg_product(a, j+1, dimensions);
        }
        cbat_array_data(result)[i] = cbat_array_const_data(a)[offset];
    }
    *out = result; return CBAT_OK;
}
CBatError cbat_linalg_identity(size_t n, CBatArray **out) {
    size_t shape[2] = {n,n}, i;
    CBatArray *result = NULL;
    CBatError err;
    if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat_array_new(2, shape, &result);
    if (err != CBAT_OK) return err;
    for (i = 0; i < n; i++) cbat_array_data(result)[i*n+i] = 1;
    *out = result; return CBAT_OK;
}

typedef struct cbat__lu { CBatArray *matrix; size_t *permutation, n; int sign; } cbat__lu;
static void cbat__lu_free(cbat__lu *lu) { cbat_array_free(lu->matrix); cbat_free(lu->permutation); }
static bool cbat__linalg_finite(const CBatArray *a) {
    size_t i;
    for (i = 0; i < cbat_array_size(a); i++) if (!isfinite(cbat_array_const_data(a)[i])) return false;
    return true;
}
static CBatError cbat__lu_factor(const CBatArray *a, cbat__lu *lu) {
    size_t shape[2], i, j, k;
    double *data;
    CBatError err;
    memset(lu, 0, sizeof(*lu)); lu->sign = 1;
    if (a == NULL || cbat_array_dimensions(a) != 2 || cbat_array_shape(a,0) != cbat_array_shape(a,1) || !cbat__linalg_finite(a)) return CBAT_ERR_INVALID_ARGUMENT;
    shape[0] = shape[1] = lu->n = cbat_array_shape(a,0);
    err = cbat_array_from_values(2, shape, cbat_array_const_data(a), &lu->matrix);
    if (err != CBAT_OK) return err;
    if (lu->n != 0) {
        lu->permutation = (size_t *)cbat_alloc(NULL, lu->n*sizeof(size_t));
        if (lu->permutation == NULL) return CBAT_ERR_NO_MEMORY;
    }
    data = cbat_array_data(lu->matrix);
    for (i = 0; i < lu->n; i++) lu->permutation[i] = i;
    for (k = 0; k < lu->n; k++) {
        size_t pivot = k;
        double maximum = fabs(data[k*lu->n+k]);
        for (i = k+1; i < lu->n; i++) if (fabs(data[i*lu->n+k]) > maximum) { pivot = i; maximum = fabs(data[i*lu->n+k]); }
        if (maximum == 0) return CBAT_ERR_OUT_OF_RANGE;
        if (pivot != k) {
            size_t swap = lu->permutation[k]; lu->permutation[k] = lu->permutation[pivot]; lu->permutation[pivot] = swap;
            for (j = 0; j < lu->n; j++) { double t = data[k*lu->n+j]; data[k*lu->n+j] = data[pivot*lu->n+j]; data[pivot*lu->n+j] = t; }
            lu->sign = -lu->sign;
        }
        for (i = k+1; i < lu->n; i++) {
            data[i*lu->n+k] /= data[k*lu->n+k];
            for (j = k+1; j < lu->n; j++) {
                data[i*lu->n+j] -= data[i*lu->n+k]*data[k*lu->n+j];
                if (!isfinite(data[i*lu->n+j])) return CBAT_ERR_OVERFLOW;
            }
        }
    }
    return CBAT_OK;
}
CBatError cbat_linalg_solve(const CBatArray *a, const CBatArray *b, CBatArray **out) {
    cbat__lu lu;
    CBatArray *result = NULL;
    CBatError err;
    size_t bd, shape[2], columns, i, j, k;
    double *x; const double *data;
    if (a == NULL || b == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    bd = cbat_array_dimensions(b);
    if ((bd != 1 && bd != 2) || cbat_array_shape(b,0) != cbat_array_shape(a,0) || !cbat__linalg_finite(b)) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat__lu_factor(a, &lu);
    if (err != CBAT_OK) { cbat__lu_free(&lu); return err; }
    shape[0] = lu.n; columns = bd == 1 ? 1 : cbat_array_shape(b,1); shape[1] = columns;
    err = cbat_array_new(bd, shape, &result);
    if (err != CBAT_OK) { cbat__lu_free(&lu); return err; }
    x = cbat_array_data(result); data = cbat_array_const_data(lu.matrix);
    for (i = 0; i < lu.n; i++) for (j = 0; j < columns; j++) x[i*columns+j] = cbat_array_const_data(b)[lu.permutation[i]*columns+j];
    for (i = 0; i < lu.n; i++) for (j = 0; j < columns; j++) {
        for (k = 0; k < i; k++) x[i*columns+j] -= data[i*lu.n+k]*x[k*columns+j];
        if (!isfinite(x[i*columns+j])) { err = CBAT_ERR_OVERFLOW; goto done; }
    }
    for (i = lu.n; i != 0; i--) for (j = 0; j < columns; j++) {
        for (k = i; k < lu.n; k++) x[(i-1)*columns+j] -= data[(i-1)*lu.n+k]*x[k*columns+j];
        x[(i-1)*columns+j] /= data[(i-1)*lu.n+i-1];
        if (!isfinite(x[(i-1)*columns+j])) { err = CBAT_ERR_OVERFLOW; goto done; }
    }
done:
    cbat__lu_free(&lu);
    if (err != CBAT_OK) { cbat_array_free(result); return err; }
    *out = result; return CBAT_OK;
}
CBatError cbat_linalg_inverse(const CBatArray *a, CBatArray **out) {
    CBatArray *identity = NULL;
    CBatError err;
    if (a == NULL || out == NULL || cbat_array_dimensions(a) != 2 || cbat_array_shape(a,0) != cbat_array_shape(a,1)) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat_linalg_identity(cbat_array_shape(a,0), &identity);
    if (err == CBAT_OK) err = cbat_linalg_solve(a, identity, out);
    cbat_array_free(identity); return err;
}
CBatError cbat_linalg_determinant(const CBatArray *a, double *out) {
    cbat__lu lu;
    CBatError err;
    double mantissa, value;
    int64_t exponent = 0;
    size_t i;
    if (out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    err = cbat__lu_factor(a, &lu);
    if (err == CBAT_ERR_OUT_OF_RANGE) { cbat__lu_free(&lu); *out = 0; return CBAT_OK; }
    if (err != CBAT_OK) { cbat__lu_free(&lu); return err; }
    mantissa = lu.sign;
    for (i = 0; i < lu.n; i++) {
        int e, normalization;
        mantissa *= frexp(cbat_array_const_data(lu.matrix)[i*lu.n+i], &e);
        mantissa = frexp(mantissa, &normalization); exponent += (int64_t)e + normalization;
    }
    cbat__lu_free(&lu);
    if (exponent > INT_MAX) return CBAT_ERR_OVERFLOW;
    value = exponent < INT_MIN ? copysign(0, mantissa) : ldexp(mantissa, (int)exponent);
    if (!isfinite(value)) return CBAT_ERR_OVERFLOW;
    *out = value; return CBAT_OK;
}
