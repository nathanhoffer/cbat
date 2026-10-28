#include <string.h>

#include "cbat/args.h"
#include "cbat/test.h"

static CBatArgs *make_parser(void) {
    CBatArgs *args = cbat_args_new("wc", "Count things.");
    if (args == NULL) {
        return NULL;
    }
    if (cbat_args_add_flag(args, 'l', "lines", "count lines") != CBAT_OK ||
        cbat_args_add_flag(args, 'v', "verbose", "chatty output") != CBAT_OK ||
        cbat_args_add_option(args, 'o', "output", "FILE", NULL, "write to FILE") != CBAT_OK ||
        cbat_args_add_option(args, 'n', "count", "N", "10", "how many") != CBAT_OK ||
        cbat_args_add_positional(args, "input", "file to read", true) != CBAT_OK) {
        cbat_args_free(args);
        return NULL;
    }
    return args;
}

CBAT_TEST(test_basic_parse) {
    CBatArgs *args = make_parser();
    char *argv[] = {"wc", "-l", "--output", "out.txt", "in.txt"};
    int64_t n = 0;
    CBAT_REQUIRE_NOT_NULL(args);
    CBAT_REQUIRE_OK(cbat_args_parse(args, 5, argv));
    CBAT_CHECK(cbat_args_flag(args, "lines"));
    CBAT_CHECK(!cbat_args_flag(args, "verbose"));
    CBAT_CHECK_EQ_STR(cbat_args_get(args, "output"), "out.txt");
    CBAT_CHECK_EQ_STR(cbat_args_get(args, "input"), "in.txt");
    CBAT_CHECK_OK(cbat_args_get_int(args, "count", &n)); /* default */
    CBAT_CHECK_EQ_INT(n, 10);
    CBAT_CHECK(!cbat_args_help_requested(args));
    cbat_args_free(args);
}

CBAT_TEST(test_all_option_forms) {
    CBatArgs *args = make_parser();
    char *argv[] = {"wc", "-lvn5", "--output=o.txt", "in", "extra1", "--", "-not-an-option"};
    double d = 0;
    CBAT_REQUIRE_NOT_NULL(args);
    CBAT_REQUIRE_OK(cbat_args_parse(args, 7, argv));
    CBAT_CHECK(cbat_args_flag(args, "lines"));
    CBAT_CHECK(cbat_args_flag(args, "verbose"));
    CBAT_CHECK_EQ_STR(cbat_args_get(args, "count"), "5");
    CBAT_CHECK_OK(cbat_args_get_double(args, "count", &d));
    CBAT_CHECK_NEAR(d, 5.0, 0);
    CBAT_CHECK_EQ_STR(cbat_args_get(args, "output"), "o.txt");
    CBAT_REQUIRE_EQ_SIZE(cbat_args_extra_count(args), 2);
    CBAT_CHECK_EQ_STR(cbat_args_extra(args, 0), "extra1");
    CBAT_CHECK_EQ_STR(cbat_args_extra(args, 1), "-not-an-option");
    CBAT_CHECK_NULL(cbat_args_extra(args, 2));
    cbat_args_free(args);
}

CBAT_TEST(test_short_option_with_separate_value) {
    CBatArgs *args = make_parser();
    char *argv[] = {"wc", "-o", "x", "-", "-n", "-3"};
    int64_t n = 0;
    CBAT_REQUIRE_NOT_NULL(args);
    CBAT_REQUIRE_OK(cbat_args_parse(args, 6, argv));
    CBAT_CHECK_EQ_STR(cbat_args_get(args, "output"), "x");
    CBAT_CHECK_EQ_STR(cbat_args_get(args, "input"), "-"); /* "-" means stdin by convention */
    CBAT_CHECK_OK(cbat_args_get_int(args, "count", &n));
    CBAT_CHECK_EQ_INT(n, -3);
    cbat_args_free(args);
}

CBAT_TEST(test_errors) {
    CBatArgs *args = make_parser();
    char *unknown_long[] = {"wc", "--bogus", "in"};
    char *unknown_short[] = {"wc", "-z", "in"};
    char *missing_value[] = {"wc", "in", "--output"};
    char *missing_positional[] = {"wc", "-l"};
    char *flag_with_value[] = {"wc", "--lines=yes", "in"};
    char *bad_int[] = {"wc", "-n", "ten", "in"};
    int64_t n;
    CBAT_REQUIRE_NOT_NULL(args);
    CBAT_CHECK_ERR(cbat_args_parse(args, 3, unknown_long), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_STR(cbat_args_error(args), "unknown option '--bogus'");
    CBAT_CHECK_ERR(cbat_args_parse(args, 3, unknown_short), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_STR(cbat_args_error(args), "unknown option '-z'");
    CBAT_CHECK_ERR(cbat_args_parse(args, 3, missing_value), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_STR(cbat_args_error(args), "option '--output' needs a value");
    CBAT_CHECK_ERR(cbat_args_parse(args, 2, missing_positional), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_STR(cbat_args_error(args), "missing required argument 'input'");
    CBAT_CHECK_ERR(cbat_args_parse(args, 3, flag_with_value), CBAT_ERR_PARSE);
    CBAT_CHECK_EQ_STR(cbat_args_error(args), "option '--lines' does not take a value");
    CBAT_REQUIRE_OK(cbat_args_parse(args, 4, bad_int));
    CBAT_CHECK_ERR(cbat_args_get_int(args, "count", &n), CBAT_ERR_PARSE);
    CBAT_CHECK_ERR(cbat_args_get_int(args, "output", &n), CBAT_ERR_NOT_FOUND);
    CBAT_CHECK_EQ_STR(cbat_args_error(args), "");
    cbat_args_free(args);
}

CBAT_TEST(test_reparse_resets_state) {
    CBatArgs *args = make_parser();
    char *first[] = {"wc", "-l", "a", "b"};
    char *second[] = {"wc", "c"};
    CBAT_REQUIRE_NOT_NULL(args);
    CBAT_REQUIRE_OK(cbat_args_parse(args, 4, first));
    CBAT_REQUIRE_OK(cbat_args_parse(args, 2, second));
    CBAT_CHECK(!cbat_args_flag(args, "lines"));
    CBAT_CHECK_EQ_STR(cbat_args_get(args, "input"), "c");
    CBAT_CHECK_EQ_SIZE(cbat_args_extra_count(args), 0);
    cbat_args_free(args);
}

CBAT_TEST(test_help) {
    CBatArgs *args = make_parser();
    char *argv[] = {"wc", "--help"};
    char *short_help[] = {"wc", "-lh"};
    CBatString *help = NULL;
    CBAT_REQUIRE_NOT_NULL(args);
    CBAT_REQUIRE_OK(cbat_args_parse(args, 2, argv)); /* required input not enforced */
    CBAT_CHECK(cbat_args_help_requested(args));
    CBAT_REQUIRE_OK(cbat_args_parse(args, 2, short_help));
    CBAT_CHECK(cbat_args_help_requested(args));
    CBAT_REQUIRE_OK(cbat_args_help(args, &help));
    CBAT_CHECK_EQ_STR(cbat_string_cstr(help),
                      "usage: wc [options] input\n"
                      "\n"
                      "Count things.\n"
                      "\n"
                      "positional arguments:\n"
                      "  input              file to read\n"
                      "\n"
                      "options:\n"
                      "  -h, --help         show this help and exit\n"
                      "  -l, --lines        count lines\n"
                      "  -v, --verbose      chatty output\n"
                      "  -o, --output FILE  write to FILE\n"
                      "  -n, --count N      how many (default: 10)\n");
    cbat_string_free(help);
    cbat_args_free(args);
}

CBAT_TEST(test_definition_errors) {
    CBatArgs *args = cbat_args_new(NULL, NULL);
    CBAT_REQUIRE_NOT_NULL(args);
    CBAT_CHECK_OK(cbat_args_add_flag(args, 'x', "extract", NULL));
    CBAT_CHECK_ERR(cbat_args_add_flag(args, 'x', "other", "dup short"), CBAT_ERR_ALREADY_EXISTS);
    CBAT_CHECK_ERR(cbat_args_add_flag(args, 0, "extract", "dup long"), CBAT_ERR_ALREADY_EXISTS);
    CBAT_CHECK_ERR(cbat_args_add_flag(args, 'h', "hat", "reserved"), CBAT_ERR_ALREADY_EXISTS);
    CBAT_CHECK_ERR(cbat_args_add_flag(args, 0, "help", "reserved"), CBAT_ERR_ALREADY_EXISTS);
    CBAT_CHECK_ERR(cbat_args_add_option(args, 0, "", NULL, NULL, NULL), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_args_add_positional(args, "--bad", NULL, false), CBAT_ERR_INVALID_ARGUMENT);
    CBAT_CHECK_ERR(cbat_args_add_flag(NULL, 'q', "quiet", NULL), CBAT_ERR_INVALID_ARGUMENT);
    cbat_args_free(args);
    cbat_args_free(NULL);
}

CBAT_TEST(test_out_of_memory) {
    size_t n;
    char *argv[] = {"wc", "-l", "-o", "x", "in", "extra"};
    for (n = 0; n < 40; n++) {
        CBatArgs *args;
        CBatError err;
        cbat_test_fail_allocations_after(n);
        args = make_parser();
        if (args != NULL) {
            err = cbat_args_parse(args, 6, argv);
            CBAT_CHECK(err == CBAT_OK || err == CBAT_ERR_NO_MEMORY);
            cbat_args_free(args);
        }
        cbat_test_stop_failing_allocations();
    }
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_basic_parse),
    CBAT_TEST_ENTRY(test_all_option_forms),
    CBAT_TEST_ENTRY(test_short_option_with_separate_value),
    CBAT_TEST_ENTRY(test_errors),
    CBAT_TEST_ENTRY(test_reparse_resets_state),
    CBAT_TEST_ENTRY(test_help),
    CBAT_TEST_ENTRY(test_definition_errors),
    CBAT_TEST_ENTRY(test_out_of_memory),
};

CBAT_TEST_MAIN("args", tests)
