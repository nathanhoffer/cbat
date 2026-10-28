#include "cbat/ndarray.h"
#include "cbat/test.h"
#include <math.h>
#include <float.h>

CBAT_TEST(test_shapes_indexing_and_ranges) {
    CBatArray *a = NULL;
    size_t shape[] = {2,3}, index[] = {1,2}, reshaped[] = {3,2}, invalid[] = {7};
    double value = 99;
    CBAT_REQUIRE_OK(cbat_array_new(2, shape, &a));
    CBAT_CHECK_EQ_INT(cbat_array_size(a), 6); CBAT_CHECK_EQ_INT(cbat_array_dimensions(a), 2);
    CBAT_REQUIRE_OK(cbat_array_get(a, index, &value)); CBAT_CHECK_NEAR(value, 0, 0);
    CBAT_REQUIRE_OK(cbat_array_set(a, index, 42)); CBAT_CHECK_NEAR(cbat_array_data(a)[5], 42, 0);
    CBAT_REQUIRE_OK(cbat_array_reshape(a, 2, reshaped)); CBAT_CHECK_EQ_INT(cbat_array_shape(a,0), 3);
    CBAT_CHECK_ERR(cbat_array_reshape(a, 1, invalid), CBAT_ERR_INVALID_ARGUMENT);
    cbat_array_free(a); a = NULL;
    CBAT_REQUIRE_OK(cbat_array_new(0, NULL, &a)); CBAT_REQUIRE_OK(cbat_array_set(a,NULL,3));
    CBAT_REQUIRE_OK(cbat_array_get(a,NULL,&value)); CBAT_CHECK_NEAR(value,3,0); cbat_array_free(a); a = NULL;
    CBAT_REQUIRE_OK(cbat_array_arange(0,1,0.2,&a)); CBAT_CHECK_EQ_INT(cbat_array_size(a),5); CBAT_CHECK_NEAR(cbat_array_data(a)[4],0.8,1e-15); cbat_array_free(a); a = NULL;
    CBAT_REQUIRE_OK(cbat_array_arange(3,-1,-1,&a)); CBAT_CHECK_EQ_INT(cbat_array_size(a),4); CBAT_CHECK_NEAR(cbat_array_data(a)[3],0,0); cbat_array_free(a); a = NULL;
    CBAT_REQUIRE_OK(cbat_array_arange(-DBL_MAX,DBL_MAX,DBL_MAX/2,&a)); CBAT_CHECK_EQ_INT(cbat_array_size(a),4); CBAT_CHECK(isfinite(cbat_array_data(a)[3])); cbat_array_free(a); a = NULL;
    CBAT_REQUIRE_OK(cbat_array_linspace(-DBL_MAX,DBL_MAX,5,&a)); CBAT_CHECK_NEAR(cbat_array_data(a)[2],0,0); cbat_array_free(a);
}
CBAT_TEST(test_broadcast_reduce_print_and_empty) {
    size_t shape[] = {2,3}, vector_shape = 3, empty[] = {2,0};
    double values[] = {1,2,3,4,5,6}, vector[] = {10,20,30};
    CBatArray *a = NULL, *b = NULL, *c = NULL, *sum = NULL;
    CBatString *text = NULL;
    CBAT_REQUIRE_OK(cbat_array_from_values(2,shape,values,&a)); CBAT_REQUIRE_OK(cbat_array_from_values(1,&vector_shape,vector,&b));
    CBAT_REQUIRE_OK(cbat_array_add(a,b,&c)); CBAT_CHECK_NEAR(cbat_array_data(c)[5],36,0);
    CBAT_REQUIRE_OK(cbat_array_sum_axis(c,0,&sum)); CBAT_CHECK_NEAR(cbat_array_data(sum)[1],47,0); cbat_array_free(sum); sum = NULL;
    CBAT_REQUIRE_OK(cbat_array_mean_axis(a,1,&sum)); CBAT_CHECK_NEAR(cbat_array_data(sum)[1],5,0); cbat_array_free(sum); sum = NULL;
    CBAT_REQUIRE_OK(cbat_array_to_string(a,&text)); CBAT_CHECK_EQ_STR(text->data,"[[1 2 3]\n [4 5 6]]"); cbat_string_free(text);
    cbat_array_free(c); c = NULL; CBAT_REQUIRE_OK(cbat_array_subtract(a,b,&c)); CBAT_CHECK_NEAR(cbat_array_data(c)[0],-9,0); cbat_array_free(c); c = NULL;
    CBAT_REQUIRE_OK(cbat_array_multiply(a,b,&c)); CBAT_CHECK_NEAR(cbat_array_data(c)[5],180,0); cbat_array_free(c); c = NULL;
    CBAT_REQUIRE_OK(cbat_array_divide(a,b,&c)); cbat_array_scale(c,10); CBAT_CHECK_NEAR(cbat_array_data(c)[0],1,1e-15);
    cbat_array_free(c); cbat_array_free(a); cbat_array_free(b); a = NULL;
    CBAT_REQUIRE_OK(cbat_array_new(2,empty,&a)); CBAT_CHECK_NULL(cbat_array_data(a));
    CBAT_REQUIRE_OK(cbat_array_sum_axis(a,1,&sum)); CBAT_CHECK_EQ_INT(cbat_array_size(sum),2); CBAT_CHECK_NEAR(cbat_array_data(sum)[0],0,0); cbat_array_free(sum); sum = NULL;
    CBAT_REQUIRE_OK(cbat_array_mean_axis(a,1,&sum)); CBAT_CHECK(isnan(cbat_array_data(sum)[0])); cbat_array_free(sum); cbat_array_free(a);
}
CBAT_TEST(test_invalid_and_oom) {
    size_t shape = 3, huge[] = {SIZE_MAX,2}, n;
    double values[] = {1,2,3};
    CBatArray *a = NULL;
    bool succeeded = false;
    CBAT_CHECK_ERR(cbat_array_new(2,huge,&a),CBAT_ERR_OVERFLOW);
    CBAT_CHECK_ERR(cbat_array_new(5,&shape,&a),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_array_new(1,NULL,&a),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_array_arange(0,1,0,&a),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_REQUIRE_OK(cbat_array_from_values(1,&shape,values,&a));
    for (n = 0; n < 30; n++) {
        CBatArray *result = NULL; CBatString *text = NULL; CBatError err; bool good = true;
        cbat_test_fail_allocations_after(n); err = cbat_array_add(a,a,&result); cbat_test_stop_failing_allocations();
        if (err != CBAT_OK) { good = false; CBAT_CHECK_ERR(err,CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(result); } cbat_array_free(result);
        cbat_test_fail_allocations_after(n); err = cbat_array_to_string(a,&text); cbat_test_stop_failing_allocations();
        if (err != CBAT_OK) { good = false; CBAT_CHECK_ERR(err,CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(text); } cbat_string_free(text);
        if (good) { succeeded = true; break; }
    }
    CBAT_CHECK(succeeded); cbat_array_free(a); cbat_array_free(NULL);
}
CBAT_TEST(test_all_allocating_paths_and_arguments) {
    size_t length = 3, index = 3, op, n;
    double values[] = {1,2,3}, unchanged = 77;
    CBatArray *a = NULL;
    CBAT_REQUIRE_OK(cbat_array_from_values(1,&length,values,&a));
    CBAT_CHECK_ERR(cbat_array_get(a,&index,&unchanged),CBAT_ERR_OUT_OF_RANGE); CBAT_CHECK_NEAR(unchanged,77,0);
    CBAT_CHECK_ERR(cbat_array_get(NULL,NULL,&unchanged),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_array_get(a,NULL,&unchanged),CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_array_set(a,&index,4),CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_NULL(cbat_array_data(NULL)); CBAT_CHECK_NULL(cbat_array_const_data(NULL));
    CBAT_CHECK_EQ_INT(cbat_array_size(NULL),0); CBAT_CHECK_EQ_INT(cbat_array_dimensions(NULL),0); CBAT_CHECK_EQ_INT(cbat_array_shape(a,4),0);
    for (op = 0; op < 12; op++) {
        bool success = false;
        for (n = 0; n < 20; n++) {
            CBatArray *result = NULL; CBatError err;
            cbat_test_fail_allocations_after(n);
            switch (op) {
            case 0: err = cbat_array_new(1,&length,&result); break;
            case 1: err = cbat_array_new_with(1,&length,NULL,&result); break;
            case 2: err = cbat_array_from_values(1,&length,values,&result); break;
            case 3: err = cbat_array_from_values_with(1,&length,values,NULL,&result); break;
            case 4: err = cbat_array_arange(0,3,1,&result); break;
            case 5: err = cbat_array_linspace(0,3,3,&result); break;
            case 6: err = cbat_array_add(a,a,&result); break;
            case 7: err = cbat_array_subtract(a,a,&result); break;
            case 8: err = cbat_array_multiply(a,a,&result); break;
            case 9: err = cbat_array_divide(a,a,&result); break;
            case 10: err = cbat_array_sum_axis(a,0,&result); break;
            default: err = cbat_array_mean_axis(a,0,&result); break;
            }
            cbat_test_stop_failing_allocations();
            if (err == CBAT_OK) { success = true; cbat_array_free(result); break; }
            CBAT_CHECK_ERR(err,CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(result);
        }
        CBAT_CHECK(success);
    }
    {
        CBatArray *out = NULL;
        CBatArena *arena = cbat_arena_new(4096);
        CBatString *text = NULL;
        CBAT_REQUIRE_NOT_NULL(arena);
        CBAT_CHECK_ERR(cbat_array_from_values(1,&length,NULL,&out),CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_array_new(1,&length,NULL),CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_array_arange(NAN,1,1,&out),CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_array_linspace(0,INFINITY,3,&out),CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_array_sum_axis(a,1,&out),CBAT_ERR_OUT_OF_RANGE);
        CBAT_CHECK_ERR(cbat_array_mean_axis(NULL,0,&out),CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_array_add(NULL,a,&out),CBAT_ERR_INVALID_ARGUMENT);
        CBAT_CHECK_ERR(cbat_array_to_string(NULL,&text),CBAT_ERR_INVALID_ARGUMENT);
        CBAT_REQUIRE_OK(cbat_array_from_values_with(1,&length,values,cbat_arena_allocator(arena),&out));
        CBAT_REQUIRE_OK(cbat_array_to_string(out,&text)); CBAT_CHECK(text->allocator == cbat_arena_allocator(arena));
        cbat_string_free(text); cbat_array_free(out); cbat_arena_free(arena);
    }
    cbat_array_free(a);
}
CBAT_TEST(test_four_axis_and_empty_broadcast) {
    size_t as[] = {2,1,2,1}, bs[] = {1,3,1,2}, i,j,k,l, ix[4];
    double av[] = {1,2,3,4}, bv[] = {10,20,30,40,50,60};
    CBatArray *a = NULL,*b = NULL,*c = NULL;
    CBAT_REQUIRE_OK(cbat_array_from_values(4,as,av,&a)); CBAT_REQUIRE_OK(cbat_array_from_values(4,bs,bv,&b));
    CBAT_REQUIRE_OK(cbat_array_add(a,b,&c)); CBAT_CHECK_EQ_INT(cbat_array_size(c),24);
    for (i=0;i<2;i++) for (j=0;j<3;j++) for (k=0;k<2;k++) for (l=0;l<2;l++) {
        double value;
        ix[0]=i;ix[1]=j;ix[2]=k;ix[3]=l;
        CBAT_REQUIRE_OK(cbat_array_get(c,ix,&value)); CBAT_CHECK_NEAR(value,av[2*i+k]+bv[2*j+l],0);
    }
    cbat_array_free(a);cbat_array_free(b);cbat_array_free(c);a=NULL;b=NULL;c=NULL;
    as[0]=0; bs[0]=1;
    CBAT_REQUIRE_OK(cbat_array_new(4,as,&a)); CBAT_REQUIRE_OK(cbat_array_new(4,bs,&b));
    CBAT_REQUIRE_OK(cbat_array_add(a,b,&c)); CBAT_CHECK_EQ_INT(cbat_array_size(c),0);
    cbat_array_free(a);cbat_array_free(b);cbat_array_free(c);
}
static const CBatTestCase tests[] = {CBAT_TEST_ENTRY(test_shapes_indexing_and_ranges), CBAT_TEST_ENTRY(test_broadcast_reduce_print_and_empty), CBAT_TEST_ENTRY(test_invalid_and_oom),
    CBAT_TEST_ENTRY(test_all_allocating_paths_and_arguments), CBAT_TEST_ENTRY(test_four_axis_and_empty_broadcast)};
CBAT_TEST_MAIN("ndarray",tests)
