#include <math.h>
#include <string.h>

#include "cbat/random.h"
#include "cbat/test.h"

CBAT_TEST(test_pcg32_reference_vector) {
    /* From the PCG reference implementation's pcg32-demo (seed 42, stream 54). */
    static const uint32_t expected[6] = {
        0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu,
    };
    CBatRandom rng;
    int i;
    cbat_random_seed_stream(&rng, 42u, 54u);
    for (i = 0; i < 6; i++) {
        CBAT_CHECK_EQ_UINT(cbat_random_u32(&rng), expected[i]);
    }
}

CBAT_TEST(test_same_seed_same_sequence) {
    CBatRandom a;
    CBatRandom b;
    int i;
    cbat_random_seed(&a, 7);
    cbat_random_seed(&b, 7);
    for (i = 0; i < 100; i++) {
        CBAT_REQUIRE_EQ_UINT(cbat_random_u64(&a), cbat_random_u64(&b));
    }
    cbat_random_seed(&b, 8);
    CBAT_CHECK(cbat_random_u64(&a) != cbat_random_u64(&b));
}

CBAT_TEST(test_range_bounds_and_coverage) {
    CBatRandom rng;
    int seen[6] = {0};
    int i;
    cbat_random_seed(&rng, 1);
    for (i = 0; i < 6000; i++) {
        int64_t r = cbat_random_range(&rng, 1, 6);
        CBAT_REQUIRE(r >= 1 && r <= 6);
        seen[r - 1]++;
    }
    for (i = 0; i < 6; i++) {
        CBAT_CHECK(seen[i] > 800 && seen[i] < 1200); /* roughly uniform */
    }
    CBAT_CHECK_EQ_INT(cbat_random_range(&rng, 5, 5), 5);
    i = (int)cbat_random_range(&rng, 10, 1); /* swapped bounds are fine */
    CBAT_CHECK(i >= 1 && i <= 10);
    (void)cbat_random_range(&rng, INT64_MIN, INT64_MAX); /* full range must not hang */
    CBAT_CHECK(cbat_random_range(&rng, -3, -1) < 0);
}

CBAT_TEST(test_double_uniform_gaussian_chance) {
    CBatRandom rng;
    double sum = 0.0;
    double sum_sq = 0.0;
    int hits = 0;
    int i;
    cbat_random_seed(&rng, 123);
    for (i = 0; i < 10000; i++) {
        double d = cbat_random_double(&rng);
        double u = cbat_random_uniform(&rng, -2.0, 2.0);
        double g = cbat_random_gaussian(&rng, 10.0, 2.0);
        CBAT_REQUIRE(d >= 0.0 && d < 1.0);
        CBAT_REQUIRE(u >= -2.0 && u < 2.0);
        sum += g;
        sum_sq += g * g;
        hits += cbat_random_chance(&rng, 0.25) ? 1 : 0;
    }
    CBAT_CHECK_NEAR(sum / 10000.0, 10.0, 0.1);
    CBAT_CHECK_NEAR(sqrt(sum_sq / 10000.0 - (sum / 10000.0) * (sum / 10000.0)), 2.0, 0.1);
    CBAT_CHECK(hits > 2200 && hits < 2800);
    CBAT_CHECK(!cbat_random_chance(&rng, 0.0));
    CBAT_CHECK(cbat_random_chance(&rng, 1.5));
}

CBAT_TEST(test_shuffle_is_permutation) {
    CBatRandom rng;
    int values[50];
    int counts[50] = {0};
    int i;
    for (i = 0; i < 50; i++) {
        values[i] = i;
    }
    cbat_random_seed(&rng, 99);
    cbat_random_shuffle(&rng, values, 50, sizeof(int));
    for (i = 0; i < 50; i++) {
        counts[values[i]]++;
    }
    for (i = 0; i < 50; i++) {
        CBAT_CHECK_EQ_INT(counts[i], 1);
    }
    cbat_random_shuffle(&rng, NULL, 10, sizeof(int)); /* no crash */
}

CBAT_TEST(test_list_helpers_and_bytes) {
    CBatRandom rng;
    CBatList *list = cbat_list_new(sizeof(int));
    unsigned char buffer[37];
    int i;
    int *pick;
    CBAT_REQUIRE_NOT_NULL(list);
    cbat_random_seed(&rng, 5);
    CBAT_CHECK_NULL(cbat_random_choice(&rng, list));
    for (i = 0; i < 10; i++) {
        CBAT_LIST_PUSH(list, int, i);
    }
    pick = (int *)cbat_random_choice(&rng, list);
    CBAT_REQUIRE_NOT_NULL(pick);
    CBAT_CHECK(*pick >= 0 && *pick < 10);
    cbat_random_shuffle_list(&rng, list);
    CBAT_CHECK_EQ_SIZE(cbat_list_length(list), 10);
    memset(buffer, 0, sizeof(buffer));
    cbat_random_bytes(&rng, buffer, sizeof(buffer));
    for (i = 0, pick = NULL; i < (int)sizeof(buffer); i++) {
        if (buffer[i] != 0) {
            pick = (int *)buffer; /* at least one non-zero byte */
        }
    }
    CBAT_CHECK_NOT_NULL(pick);
    cbat_list_free(list);
}

CBAT_TEST(test_global_generator) {
    int64_t r = cbat_random_range(NULL, 1, 100);
    CBAT_CHECK(r >= 1 && r <= 100);
    cbat_random_seed(NULL, 3);
    r = cbat_random_range(NULL, 1, 100);
    cbat_random_seed(NULL, 3);
    CBAT_CHECK_EQ_INT(cbat_random_range(NULL, 1, 100), r);
    cbat_random_seed_from_time(NULL);
}

CBAT_TEST(test_sample_weighted_uuid) {
    CBatRandom rng;
    CBatList *source = cbat_list_new(sizeof(int)), *sample = NULL;
    double weights[] = {0, 1, 0};
    char uuid[37];
    size_t i, j, index = 99, allowed;
    bool succeeded = false;
    CBAT_REQUIRE_NOT_NULL(source);
    for (i = 0; i < 50; i++) { int n = (int)i; CBAT_REQUIRE_OK(cbat_list_push(source, &n)); }
    cbat_random_seed(&rng, 42);
    CBAT_REQUIRE_OK(cbat_random_sample(&rng, source, 20, &sample));
    CBAT_CHECK_EQ_INT(sample->length, 20);
    for (i = 0; i < sample->length; i++) {
        int n = *(int *)cbat_list_get(sample, i);
        CBAT_CHECK(n >= 0 && n < 50);
        for (j = 0; j < i; j++) CBAT_CHECK(n != *(int *)cbat_list_get(sample, j));
    }
    cbat_list_free(sample); sample = NULL;
    CBAT_CHECK_ERR(cbat_random_sample(&rng, source, 51, &sample), CBAT_ERR_OUT_OF_RANGE);
    CBAT_CHECK_ERR(cbat_random_sample(&rng, NULL, 0, &sample), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_random_sample(&rng, source, 0, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_REQUIRE_OK(cbat_random_sample(&rng, source, 0, &sample));
    CBAT_CHECK_EQ_INT(sample->length, 0); cbat_list_free(sample); sample = NULL;
    for (i = 0; i < 100; i++) {
        CBAT_REQUIRE_OK(cbat_random_weighted_index(&rng, weights, 3, &index)); CBAT_CHECK_EQ_INT(index, 1);
    }
    weights[1] = 0;
    CBAT_CHECK_ERR(cbat_random_weighted_index(&rng, weights, 3, &index), CBAT_ERR_INVALID_ARGUMENT);
    weights[1] = -1;
    CBAT_CHECK_ERR(cbat_random_weighted_index(&rng, weights, 3, &index), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_random_weighted_index(&rng, NULL, 0, &index), CBAT_ERR_INVALID_ARGUMENT);
    cbat_random_uuid4(&rng, uuid); cbat_random_uuid4(&rng, NULL);
    CBAT_CHECK_EQ_INT(strlen(uuid), 36); CBAT_CHECK_EQ_INT(uuid[14], '4');
    CBAT_CHECK(uuid[19] == '8' || uuid[19] == '9' || uuid[19] == 'a' || uuid[19] == 'b');
    CBAT_CHECK_EQ_INT(uuid[8], '-'); CBAT_CHECK_EQ_INT(uuid[13], '-');
    for (allowed = 0; allowed < 20; allowed++) {
        CBatError err;
        cbat_test_fail_allocations_after(allowed);
        err = cbat_random_sample(&rng, source, 20, &sample);
        cbat_test_stop_failing_allocations();
        if (err == CBAT_OK) { succeeded = true; cbat_list_free(sample); break; }
        CBAT_CHECK_ERR(err, CBAT_ERR_NO_MEMORY); CBAT_CHECK_NULL(sample);
    }
    CBAT_CHECK(succeeded); cbat_list_free(source);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_sample_weighted_uuid),
    CBAT_TEST_ENTRY(test_pcg32_reference_vector),
    CBAT_TEST_ENTRY(test_same_seed_same_sequence),
    CBAT_TEST_ENTRY(test_range_bounds_and_coverage),
    CBAT_TEST_ENTRY(test_double_uniform_gaussian_chance),
    CBAT_TEST_ENTRY(test_shuffle_is_permutation),
    CBAT_TEST_ENTRY(test_list_helpers_and_bytes),
    CBAT_TEST_ENTRY(test_global_generator),
};

CBAT_TEST_MAIN("random", tests)
