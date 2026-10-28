/*
 * Recipe 7: A command-line tool with options and --help
 *
 * PROBLEM
 *   Parse flags, options with values and positional arguments, and print a
 *   helpful usage message - without hand-rolling an argv loop.
 *
 * SOLUTION
 *   Declare everything on a CBatArgs, call cbat_args_parse(), then read
 *   values by name. Help text is generated from the declarations.
 *
 * DISCUSSION
 *   This recipe parses two fake command lines so it runs unattended; in a
 *   real program pass main's argc/argv. Exit code 2 for usage errors is
 *   the Unix convention (argparse does the same).
 */
#include <stdio.h>

#include "cbat/cbat.h"

static int tool_main(int argc, char **argv) {
    CBatArgs *args = cbat_args_new("greet", "Print a greeting several times.");
    int64_t times = 1;
    int64_t i;
    int status = 0;
    if (args == NULL) {
        return 1;
    }
    cbat_args_add_flag(args, 's', "shout", "use upper case");
    cbat_args_add_option(args, 'n', "times", "N", "1", "how many greetings");
    cbat_args_add_positional(args, "name", "who to greet", true);

    if (cbat_args_parse(args, argc, argv) != CBAT_OK) {
        fprintf(stderr, "greet: %s (try --help)\n", cbat_args_error(args));
        status = 2;
    } else if (cbat_args_help_requested(args)) {
        CBatString *help = NULL;
        if (cbat_args_help(args, &help) == CBAT_OK) {
            fputs(cbat_string_cstr(help), stdout);
            cbat_string_free(help);
        }
    } else if (cbat_args_get_int(args, "times", &times) != CBAT_OK || times < 1) {
        fprintf(stderr, "greet: --times must be a positive integer\n");
        status = 2;
    } else {
        CBatString *message = cbat_string_from_format("Hello, %s!", cbat_args_get(args, "name"));
        if (message != NULL && cbat_args_flag(args, "shout")) {
            cbat_string_to_upper(message);
        }
        for (i = 0; message != NULL && i < times; i++) {
            puts(cbat_string_cstr(message));
        }
        cbat_string_free(message);
    }
    cbat_args_free(args);
    return status;
}

int main(void) {
    char *help_argv[] = {"greet", "--help"};
    char *real_argv[] = {"greet", "-s", "--times=2", "world"};
    char *bad_argv[] = {"greet", "--loud", "world"};
    int ok = tool_main(2, help_argv) == 0;
    puts("---");
    ok = ok && tool_main(4, real_argv) == 0;
    puts("---");
    ok = ok && tool_main(3, bad_argv) == 2;
    return ok ? 0 : 1;
}
