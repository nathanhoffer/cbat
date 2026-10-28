#include "cbat/linalg.h"
#include "cbat/test.h"
#include <math.h>

CBAT_TEST(test_pivot_solve_inverse_and_det) {
    size_t shape[] = {2,2}, vector = 2;
    double values[] = {0,2,1,3}, rhs[] = {4,7}, det = 99;
    CBatArray *a = NULL, *b = NULL, *x = NULL, *inverse = NULL, *product = NULL;
    CBAT_REQUIRE_OK(cbat_array_from_values(2,shape,values,&a)); CBAT_REQUIRE_OK(cbat_array_from_values(1,&vector,rhs,&b));
    CBAT_REQUIRE_OK(cbat_linalg_solve(a,b,&x)); CBAT_CHECK_NEAR(cbat_array_data(x)[0],1,1e-15); CBAT_CHECK_NEAR(cbat_array_data(x)[1],2,1e-15);
    CBAT_REQUIRE_OK(cbat_linalg_determinant(a,&det)); CBAT_CHECK_NEAR(det,-2,1e-15);
    CBAT_REQUIRE_OK(cbat_linalg_inverse(a,&inverse)); CBAT_REQUIRE_OK(cbat_linalg_matmul(a,inverse,&product));
    CBAT_CHECK_NEAR(cbat_array_data(product)[0],1,1e-14); CBAT_CHECK_NEAR(cbat_array_data(product)[1],0,1e-14);
    CBAT_CHECK_NEAR(cbat_array_data(product)[2],0,1e-14); CBAT_CHECK_NEAR(cbat_array_data(product)[3],1,1e-14);
    cbat_array_free(a); cbat_array_free(b); cbat_array_free(x); cbat_array_free(inverse); cbat_array_free(product);
}
CBAT_TEST(test_vectors_batches_and_transpose) {
    size_t shape[] = {2,2,2}, vector = 2;
    double values[] = {1,0,0,1,2,0,0,2}, v[] = {3,4};
    CBatArray *a = NULL, *b = NULL, *c = NULL, *t = NULL;
    CBAT_REQUIRE_OK(cbat_array_from_values(3,shape,values,&a)); CBAT_REQUIRE_OK(cbat_array_from_values(1,&vector,v,&b));
    CBAT_REQUIRE_OK(cbat_linalg_matmul(a,b,&c)); CBAT_CHECK_EQ_INT(cbat_array_dimensions(c),2); CBAT_CHECK_NEAR(cbat_array_data(c)[3],8,0);
    cbat_array_free(c); c = NULL; CBAT_REQUIRE_OK(cbat_linalg_matmul(b,b,&c)); CBAT_CHECK_EQ_INT(cbat_array_dimensions(c),0); CBAT_CHECK_NEAR(cbat_array_data(c)[0],25,0);
    CBAT_REQUIRE_OK(cbat_linalg_transpose(a,&t)); CBAT_CHECK_NEAR(cbat_array_data(t)[1],2,0);
    cbat_array_free(a); cbat_array_free(b); cbat_array_free(c); cbat_array_free(t);
}
CBAT_TEST(test_singular_empty_and_oom) {
    size_t shape[] = {2,2}, n;
    double values[] = {1,1,1,1}, det = 99;
    CBatArray *a = NULL, *result = NULL;
    bool succeeded = false;
    CBAT_REQUIRE_OK(cbat_array_from_values(2,shape,values,&a));
    CBAT_REQUIRE_OK(cbat_linalg_determinant(a,&det)); CBAT_CHECK_NEAR(det,0,0);
    CBAT_CHECK_ERR(cbat_linalg_inverse(a,&result),CBAT_ERR_OUT_OF_RANGE); CBAT_CHECK_NULL(result); cbat_array_free(a); a = NULL;
    CBAT_REQUIRE_OK(cbat_linalg_identity(0,&a)); CBAT_REQUIRE_OK(cbat_linalg_determinant(a,&det)); CBAT_CHECK_NEAR(det,1,0); cbat_array_free(a); a = NULL;
    CBAT_REQUIRE_OK(cbat_linalg_identity(3,&a));
    for (n = 0; n < 30; n++) {
        CBatError err;
        cbat_test_fail_allocations_after(n); err = cbat_linalg_inverse(a,&result); cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) { succeeded = true; cbat_array_free(result); break; }
        CBAT_CHECK_ERR(err,CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(result);
    }
    CBAT_CHECK(succeeded); cbat_array_free(a);
}
CBAT_TEST(test_scaled_det_rectangular_and_all_oom) {
    CBatArray *a = NULL,*b = NULL,*t = NULL;
    size_t rect[] = {2,3}, n, op;
    double determinant = 0, values[] = {1,2,3,4,5,6};
    CBAT_REQUIRE_OK(cbat_linalg_identity(4,&a));
    cbat_array_data(a)[0]=1e300;cbat_array_data(a)[5]=1e300;cbat_array_data(a)[10]=1e-300;cbat_array_data(a)[15]=1e-300;
    CBAT_REQUIRE_OK(cbat_linalg_determinant(a,&determinant)); CBAT_CHECK_NEAR(determinant,1,1e-14);
    cbat_array_free(a);a=NULL;
    CBAT_REQUIRE_OK(cbat_array_from_values(2,rect,values,&a)); CBAT_REQUIRE_OK(cbat_linalg_transpose(a,&t));
    CBAT_CHECK_EQ_INT(cbat_array_shape(t,0),3);CBAT_CHECK_EQ_INT(cbat_array_shape(t,1),2);
    CBAT_CHECK_NEAR(cbat_array_data(t)[1],4,0);CBAT_CHECK_NEAR(cbat_array_data(t)[4],3,0);
    CBAT_CHECK_ERR(cbat_linalg_inverse(a,&b),CBAT_ERR_INVALID_ARGUMENT);CBAT_CHECK_NULL(b);
    cbat_array_free(a);cbat_array_free(t);a=NULL;t=NULL;
    CBAT_REQUIRE_OK(cbat_linalg_identity(3,&a));CBAT_REQUIRE_OK(cbat_linalg_identity(3,&b));
    for (op=0;op<6;op++) {
        bool success=false;
        for (n=0;n<30;n++) {
            CBatArray *out=NULL; CBatError err; double det=77;
            cbat_test_fail_allocations_after(n);
            switch(op) {
            case 0:err=cbat_linalg_identity(3,&out);break;
            case 1:err=cbat_linalg_matmul(a,b,&out);break;
            case 2:err=cbat_linalg_transpose(a,&out);break;
            case 3:err=cbat_linalg_solve(a,b,&out);break;
            case 4:err=cbat_linalg_inverse(a,&out);break;
            default:err=cbat_linalg_determinant(a,&det);break;
            }
            cbat_test_stop_failing_allocations();
            if(err==CBAT_OK){success=true;cbat_array_free(out);break;}
            CBAT_CHECK_ERR(err,CBAT_ERR_NO_MEMORY);CBAT_CHECK_NULL(out);CBAT_CHECK_NEAR(det,77,0);
        }
        CBAT_CHECK(success);
    }
    CBAT_CHECK_ERR(cbat_linalg_identity(1,NULL),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_linalg_solve(NULL,b,&t),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_linalg_matmul(a,NULL,&t),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_linalg_transpose(NULL,&t),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_linalg_determinant(a,NULL),CBAT_ERR_INVALID_ARGUMENT);
    cbat_array_free(a);cbat_array_free(b);
}
static const CBatTestCase tests[] = {CBAT_TEST_ENTRY(test_pivot_solve_inverse_and_det), CBAT_TEST_ENTRY(test_vectors_batches_and_transpose), CBAT_TEST_ENTRY(test_singular_empty_and_oom),
    CBAT_TEST_ENTRY(test_scaled_det_rectangular_and_all_oom)};
CBAT_TEST_MAIN("linalg",tests)
