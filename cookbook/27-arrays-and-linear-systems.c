/* Recipe 27: Centering data and solving a linear system.
 * PROBLEM: Process a table of doubles without hand-written indexing loops.
 * SOLUTION: Reduce columns, broadcast subtraction, and solve A*x=b with LU.
 * DISCUSSION: Arrays own contiguous row-major data. Reductions remove an axis;
 * broadcasting aligns trailing axes. Solve uses partial pivoting; singular
 * systems return an error and ill-conditioned systems can lose accuracy.
 */
#include "cbat/linalg.h"
#include <stdio.h>

int main(void) {
    size_t table_shape[] = {3,2}, matrix_shape[] = {2,2}, vector_shape = 2;
    double readings[] = {10,100,20,110,30,120}, coefficients[] = {0,2,1,3}, rhs[] = {4,7};
    CBatArray *table=NULL,*means=NULL,*centered=NULL,*a=NULL,*b=NULL,*x=NULL,*check=NULL;
    CBatString *text=NULL;
    CBatError err=cbat_array_from_values(2,table_shape,readings,&table);
    if(err!=CBAT_OK)goto cleanup;
    err=cbat_array_mean_axis(table,0,&means);if(err!=CBAT_OK)goto cleanup;
    err=cbat_array_subtract(table,means,&centered);if(err!=CBAT_OK)goto cleanup;
    err=cbat_array_to_string(centered,&text);if(err!=CBAT_OK)goto cleanup;
    printf("Centered columns:\n%s\n",text->data);cbat_string_free(text);text=NULL;
    err=cbat_array_from_values(2,matrix_shape,coefficients,&a);if(err!=CBAT_OK)goto cleanup;
    err=cbat_array_from_values(1,&vector_shape,rhs,&b);if(err!=CBAT_OK)goto cleanup;
    err=cbat_linalg_solve(a,b,&x);if(err!=CBAT_OK)goto cleanup;
    err=cbat_linalg_matmul(a,x,&check);if(err!=CBAT_OK)goto cleanup;
    err=cbat_array_to_string(x,&text);if(err!=CBAT_OK)goto cleanup;
    printf("Solution: %s\nCheck: [%g %g]\n",text->data,cbat_array_data(check)[0],cbat_array_data(check)[1]);
cleanup:
    if(err!=CBAT_OK)fprintf(stderr,"%s\n",cbat_error_str(err));
    cbat_string_free(text);cbat_array_free(table);cbat_array_free(means);cbat_array_free(centered);
    cbat_array_free(a);cbat_array_free(b);cbat_array_free(x);cbat_array_free(check);
    return err==CBAT_OK?0:1;
}
