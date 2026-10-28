/*
 * cbat/args.h - command-line argument parsing with generated help
 * (cbat's answer to Python's argparse).
 *
 *     CBatArgs *args = cbat_args_new("wc", "Count words in files.");
 *     cbat_args_add_flag(args, 'l', "lines", "count lines instead of words");
 *     cbat_args_add_option(args, 'o', "output", "FILE", NULL, "write results to FILE");
 *     cbat_args_add_positional(args, "file", "input file", true);
 *
 *     if (cbat_args_parse(args, argc, argv) != CBAT_OK) {
 *         fprintf(stderr, "%s\n", cbat_args_error(args));
 *         return 2;
 *     }
 *     if (cbat_args_help_requested(args)) { print help; return 0; }
 *     bool lines = cbat_args_flag(args, "lines");
 *     const char *file = cbat_args_get(args, "file");
 *
 * Accepted forms: -l, --lines, -o FILE, -oFILE, --output FILE,
 * --output=FILE, grouped short flags (-lv), and "--" to end options.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_ARGS_H
#define CBAT_ARGS_H

#include "cbat/base.h"
#include "cbat/string.h"

CBAT_BEGIN_DECLS

typedef struct CBatArgs CBatArgs;

/** `program` and `description` are copied. NULL only when out of memory. */
CBAT_API CBatArgs *cbat_args_new(const char *program, const char *description);
CBAT_API void cbat_args_free(CBatArgs *args);

/**
 * A boolean switch. `short_name` may be 0 (no short form). `long_name` is
 * required and is the name used for lookups. "-h/--help" is built in.
 */
CBAT_API CBatError cbat_args_add_flag(CBatArgs *args, char short_name, const char *long_name,
                                      const char *help);

/** An option taking a value; `default_value` may be NULL. */
CBAT_API CBatError cbat_args_add_option(CBatArgs *args, char short_name, const char *long_name,
                                        const char *metavar, const char *default_value,
                                        const char *help);

/** A positional argument, matched in the order added. */
CBAT_API CBatError cbat_args_add_positional(CBatArgs *args, const char *name, const char *help,
                                            bool required);

/**
 * Parses argv[1..argc-1]. On failure returns CBAT_ERR_PARSE and
 * cbat_args_error() explains (unknown option, missing value, missing
 * required positional...). Extra positionals are kept (see extra_*).
 */
CBAT_API CBatError cbat_args_parse(CBatArgs *args, int argc, char **argv);

/** Message for the last parse failure ("" if none). */
CBAT_API const char *cbat_args_error(const CBatArgs *args);

/** True if -h or --help was given (required positionals are then not enforced). */
CBAT_API bool cbat_args_help_requested(const CBatArgs *args);

/** Whether a flag was given. */
CBAT_API bool cbat_args_flag(const CBatArgs *args, const char *long_name);

/** Value of an option or positional (its default, or NULL, if absent). */
CBAT_API const char *cbat_args_get(const CBatArgs *args, const char *name);

/** Value parsed as an integer / double (CBAT_ERR_NOT_FOUND if absent). */
CBAT_API CBatError cbat_args_get_int(const CBatArgs *args, const char *name, int64_t *out);
CBAT_API CBatError cbat_args_get_double(const CBatArgs *args, const char *name, double *out);

/** Positionals beyond the declared ones. */
CBAT_API size_t cbat_args_extra_count(const CBatArgs *args);
CBAT_API const char *cbat_args_extra(const CBatArgs *args, size_t index);

/** Usage text listing every option and positional. */
CBAT_API CBatError cbat_args_help(const CBatArgs *args, CBatString **out);

CBAT_END_DECLS

#endif /* CBAT_ARGS_H */
