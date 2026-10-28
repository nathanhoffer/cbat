/* cbat/linalg.h - dense double matrix algebra on CBatArray. MIT License.
 * Optional CBAT_WITH_LINALG (default ON) requires CBAT_WITH_NDARRAY.
 * Results use default allocator and own their data; inputs remain unchanged.
 * Fallible APIs preserve out on error. Dimension mismatches: INVALID_ARGUMENT.
 */
#ifndef CBAT_LINALG_H
#define CBAT_LINALG_H
#include "cbat/ndarray.h"
CBAT_BEGIN_DECLS
/** NumPy-style matmul: rank 1 vectors, rank >=2 matrices, trailing two axes
 * multiplied and preceding axes broadcast. Vector-vector gives scalar.
 * Empty inner axes sum to zero; IEEE arithmetic propagates NaN/infinity. */
CBAT_API CBatError cbat_linalg_matmul(const CBatArray *a, const CBatArray *b, CBatArray **out);
/** New copy with axis order reversed (matrix transpose; vector/scalar copy). */
CBAT_API CBatError cbat_linalg_transpose(const CBatArray *a, CBatArray **out);
/** Solves square 2D A * X = B with partial-pivoted LU; B is vector or 2D RHS.
 * Finite inputs required. Exactly zero pivot: OUT_OF_RANGE (singular).
 * Nonfinite intermediate/result: OVERFLOW. Ill-conditioned results may lose
 * accuracy; no rank threshold is imposed. Empty square systems are accepted. */
CBAT_API CBatError cbat_linalg_solve(const CBatArray *a, const CBatArray *b, CBatArray **out);
/** Square matrix inverse via pivoted solve; same errors as solve. */
CBAT_API CBatError cbat_linalg_inverse(const CBatArray *a, CBatArray **out);
/** Square determinant via pivoted LU; singular gives 0, empty gives 1.
 * Finite inputs required; overflow gives OVERFLOW, underflow gives signed zero. */
CBAT_API CBatError cbat_linalg_determinant(const CBatArray *a, double *out);
/** New n*n identity matrix, including n=0. */
CBAT_API CBatError cbat_linalg_identity(size_t n, CBatArray **out);
CBAT_END_DECLS
#endif
