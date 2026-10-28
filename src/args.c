/*
 * cbat/args.c - argparse-style command-line parsing.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/args.h"

#include <string.h>

#include "cbat/list.h"

typedef enum cbat__arg_kind {
    CBAT__ARG_FLAG,
    CBAT__ARG_OPTION,
    CBAT__ARG_POSITIONAL
} cbat__arg_kind;

typedef struct cbat__arg_spec {
    cbat__arg_kind kind;
    char short_name;
    char *name;
    char *help;
    char *metavar;
    char *default_value;
    char *value; /* parsed value (owned) */
    bool present;
    bool required;
} cbat__arg_spec;

struct CBatArgs {
    char *program;
    char *description;
    CBatList *specs;  /* cbat__arg_spec by value */
    CBatList *extras; /* char * (owned) */
    CBatString *error;
    bool help;
};

static void cbat__arg_spec_destroy(void *element) {
    cbat__arg_spec *spec = (cbat__arg_spec *)element;
    cbat_free(spec->name);
    cbat_free(spec->help);
    cbat_free(spec->metavar);
    cbat_free(spec->default_value);
    cbat_free(spec->value);
}

CBatArgs *cbat_args_new(const char *program, const char *description) {
    CBatArgs *args = (CBatArgs *)cbat_alloc_zeroed(NULL, sizeof(CBatArgs));
    if (args == NULL) {
        return NULL;
    }
    args->program = cbat_strdup(NULL, program != NULL ? program : "program");
    args->description = cbat_strdup(NULL, description != NULL ? description : "");
    args->specs = cbat_list_new(sizeof(cbat__arg_spec));
    args->extras = cbat_list_new(sizeof(char *));
    args->error = cbat_string_new();
    if (args->program == NULL || args->description == NULL || args->specs == NULL ||
        args->extras == NULL || args->error == NULL) {
        cbat_args_free(args);
        return NULL;
    }
    cbat_list_set_destructor(args->specs, cbat__arg_spec_destroy);
    cbat_list_set_destructor(args->extras, cbat_destroy_pointer);
    return args;
}

void cbat_args_free(CBatArgs *args) {
    if (args == NULL) {
        return;
    }
    cbat_free(args->program);
    cbat_free(args->description);
    cbat_list_free(args->specs);
    cbat_list_free(args->extras);
    cbat_string_free(args->error);
    cbat_free(args);
}

static cbat__arg_spec *cbat__args_find(const CBatArgs *args, const char *name) {
    size_t i;
    for (i = 0; args != NULL && name != NULL && i < args->specs->length; i++) {
        cbat__arg_spec *spec = (cbat__arg_spec *)cbat_list_get(args->specs, i);
        if (strcmp(spec->name, name) == 0) {
            return spec;
        }
    }
    return NULL;
}

static cbat__arg_spec *cbat__args_find_short(const CBatArgs *args, char c) {
    size_t i;
    for (i = 0; i < args->specs->length; i++) {
        cbat__arg_spec *spec = (cbat__arg_spec *)cbat_list_get(args->specs, i);
        if (spec->kind != CBAT__ARG_POSITIONAL && spec->short_name == c) {
            return spec;
        }
    }
    return NULL;
}

/* Copies a possibly-NULL string; *ok becomes false if the copy failed. */
static char *cbat__args_copy(const char *s, bool *ok) {
    char *copy;
    if (s == NULL) {
        return NULL;
    }
    copy = cbat_strdup(NULL, s);
    if (copy == NULL) {
        *ok = false;
    }
    return copy;
}

static CBatError cbat__args_add(CBatArgs *args, cbat__arg_kind kind, char short_name,
                                const char *name, const char *metavar,
                                const char *default_value, const char *help, bool required) {
    cbat__arg_spec spec;
    bool ok = true;
    CBatError err;
    if (args == NULL || name == NULL || name[0] == '\0' || name[0] == '-') {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (strcmp(name, "help") == 0 || short_name == 'h' || short_name == '-' ||
        cbat__args_find(args, name) != NULL ||
        (short_name != 0 && cbat__args_find_short(args, short_name) != NULL)) {
        return CBAT_ERR_ALREADY_EXISTS;
    }
    memset(&spec, 0, sizeof(spec));
    spec.kind = kind;
    spec.short_name = short_name;
    spec.required = required;
    spec.name = cbat__args_copy(name, &ok);
    spec.help = cbat__args_copy(help != NULL ? help : "", &ok);
    spec.metavar = cbat__args_copy(metavar, &ok);
    spec.default_value = cbat__args_copy(default_value, &ok);
    if (!ok) {
        cbat__arg_spec_destroy(&spec);
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_list_push(args->specs, &spec);
    if (err != CBAT_OK) {
        cbat__arg_spec_destroy(&spec);
    }
    return err;
}

CBatError cbat_args_add_flag(CBatArgs *args, char short_name, const char *long_name,
                             const char *help) {
    return cbat__args_add(args, CBAT__ARG_FLAG, short_name, long_name, NULL, NULL, help, false);
}

CBatError cbat_args_add_option(CBatArgs *args, char short_name, const char *long_name,
                               const char *metavar, const char *default_value,
                               const char *help) {
    return cbat__args_add(args, CBAT__ARG_OPTION, short_name, long_name,
                          metavar != NULL ? metavar : "VALUE", default_value, help, false);
}

CBatError cbat_args_add_positional(CBatArgs *args, const char *name, const char *help,
                                   bool required) {
    return cbat__args_add(args, CBAT__ARG_POSITIONAL, 0, name, NULL, NULL, help, required);
}

static CBatError cbat__args_fail(CBatArgs *args, const char *format, const char *detail) {
    cbat_string_clear(args->error);
    if (cbat_string_append_format(args->error, format, detail) != CBAT_OK) {
        return CBAT_ERR_NO_MEMORY;
    }
    return CBAT_ERR_PARSE;
}

static CBatError cbat__args_set_value(cbat__arg_spec *spec, const char *value) {
    char *copy = cbat_strdup(NULL, value);
    if (copy == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    cbat_free(spec->value);
    spec->value = copy;
    spec->present = true;
    return CBAT_OK;
}

static CBatError cbat__args_positional(CBatArgs *args, const char *value) {
    size_t i;
    char *copy;
    CBatError err;
    for (i = 0; i < args->specs->length; i++) {
        cbat__arg_spec *spec = (cbat__arg_spec *)cbat_list_get(args->specs, i);
        if (spec->kind == CBAT__ARG_POSITIONAL && !spec->present) {
            return cbat__args_set_value(spec, value);
        }
    }
    copy = cbat_strdup(NULL, value);
    if (copy == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_list_push(args->extras, &copy);
    if (err != CBAT_OK) {
        cbat_free(copy);
    }
    return err;
}

static void cbat__args_reset(CBatArgs *args) {
    size_t i;
    for (i = 0; i < args->specs->length; i++) {
        cbat__arg_spec *spec = (cbat__arg_spec *)cbat_list_get(args->specs, i);
        cbat_free(spec->value);
        spec->value = NULL;
        spec->present = false;
    }
    cbat_list_clear(args->extras);
    cbat_string_clear(args->error);
    args->help = false;
}

/* Handles "--name" / "--name=value"; may consume argv[*i + 1]. */
static CBatError cbat__args_long(CBatArgs *args, int argc, char **argv, int *i) {
    const char *arg = argv[*i] + 2;
    const char *equals = strchr(arg, '=');
    size_t name_length = equals != NULL ? (size_t)(equals - arg) : strlen(arg);
    cbat__arg_spec *spec = NULL;
    size_t k;
    if (name_length == 4 && strncmp(arg, "help", 4) == 0 && equals == NULL) {
        args->help = true;
        return CBAT_OK;
    }
    for (k = 0; k < args->specs->length; k++) {
        cbat__arg_spec *candidate = (cbat__arg_spec *)cbat_list_get(args->specs, k);
        if (candidate->kind != CBAT__ARG_POSITIONAL && strlen(candidate->name) == name_length &&
            strncmp(candidate->name, arg, name_length) == 0) {
            spec = candidate;
            break;
        }
    }
    if (spec == NULL) {
        return cbat__args_fail(args, "unknown option '%s'", argv[*i]);
    }
    if (spec->kind == CBAT__ARG_FLAG) {
        if (equals != NULL) {
            return cbat__args_fail(args, "option '--%s' does not take a value", spec->name);
        }
        spec->present = true;
        return CBAT_OK;
    }
    if (equals != NULL) {
        return cbat__args_set_value(spec, equals + 1);
    }
    if (*i + 1 >= argc) {
        return cbat__args_fail(args, "option '--%s' needs a value", spec->name);
    }
    (*i)++;
    return cbat__args_set_value(spec, argv[*i]);
}

/* Handles "-abc" / "-oVALUE" / "-o VALUE". */
static CBatError cbat__args_short(CBatArgs *args, int argc, char **argv, int *i) {
    const char *arg = argv[*i];
    size_t j;
    for (j = 1; arg[j] != '\0'; j++) {
        char letter[2] = {arg[j], '\0'};
        cbat__arg_spec *spec;
        if (arg[j] == 'h') {
            args->help = true;
            continue;
        }
        spec = cbat__args_find_short(args, arg[j]);
        if (spec == NULL) {
            cbat_string_clear(args->error);
            return cbat_string_append_format(args->error, "unknown option '-%s'", letter) == CBAT_OK
                       ? CBAT_ERR_PARSE
                       : CBAT_ERR_NO_MEMORY;
        }
        if (spec->kind == CBAT__ARG_FLAG) {
            spec->present = true;
            continue;
        }
        if (arg[j + 1] != '\0') {
            return cbat__args_set_value(spec, arg + j + 1);
        }
        if (*i + 1 >= argc) {
            return cbat__args_fail(args, "option '--%s' needs a value", spec->name);
        }
        (*i)++;
        return cbat__args_set_value(spec, argv[*i]);
    }
    return CBAT_OK;
}

CBatError cbat_args_parse(CBatArgs *args, int argc, char **argv) {
    bool only_positionals = false;
    int i;
    size_t k;
    CBatError err = CBAT_OK;
    if (args == NULL || (argv == NULL && argc > 0)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    cbat__args_reset(args);
    for (i = 1; i < argc && err == CBAT_OK; i++) {
        const char *arg = argv[i];
        if (arg == NULL) {
            continue;
        }
        if (only_positionals || arg[0] != '-' || strcmp(arg, "-") == 0) {
            err = cbat__args_positional(args, arg);
        } else if (strcmp(arg, "--") == 0) {
            only_positionals = true;
        } else if (arg[1] == '-') {
            err = cbat__args_long(args, argc, argv, &i);
        } else {
            err = cbat__args_short(args, argc, argv, &i);
        }
    }
    if (err != CBAT_OK || args->help) {
        return err;
    }
    for (k = 0; k < args->specs->length; k++) {
        cbat__arg_spec *spec = (cbat__arg_spec *)cbat_list_get(args->specs, k);
        if (spec->kind == CBAT__ARG_POSITIONAL && spec->required && !spec->present) {
            return cbat__args_fail(args, "missing required argument '%s'", spec->name);
        }
    }
    return CBAT_OK;
}

const char *cbat_args_error(const CBatArgs *args) {
    return args == NULL ? "" : cbat_string_cstr(args->error);
}

bool cbat_args_help_requested(const CBatArgs *args) {
    return args != NULL && args->help;
}

bool cbat_args_flag(const CBatArgs *args, const char *long_name) {
    cbat__arg_spec *spec = cbat__args_find(args, long_name);
    return spec != NULL && spec->kind == CBAT__ARG_FLAG && spec->present;
}

const char *cbat_args_get(const CBatArgs *args, const char *name) {
    cbat__arg_spec *spec = cbat__args_find(args, name);
    if (spec == NULL || spec->kind == CBAT__ARG_FLAG) {
        return NULL;
    }
    return spec->present ? spec->value : spec->default_value;
}

CBatError cbat_args_get_int(const CBatArgs *args, const char *name, int64_t *out) {
    const char *value = cbat_args_get(args, name);
    if (value == NULL) {
        return out == NULL ? CBAT_ERR_INVALID_ARGUMENT : CBAT_ERR_NOT_FOUND;
    }
    return cbat_string_to_int64(value, out);
}

CBatError cbat_args_get_double(const CBatArgs *args, const char *name, double *out) {
    const char *value = cbat_args_get(args, name);
    if (value == NULL) {
        return out == NULL ? CBAT_ERR_INVALID_ARGUMENT : CBAT_ERR_NOT_FOUND;
    }
    return cbat_string_to_double(value, out);
}

size_t cbat_args_extra_count(const CBatArgs *args) {
    return args == NULL ? 0 : args->extras->length;
}

const char *cbat_args_extra(const CBatArgs *args, size_t index) {
    char **slot;
    if (args == NULL) {
        return NULL;
    }
    slot = (char **)cbat_list_get(args->extras, index);
    return slot == NULL ? NULL : *slot;
}

/* The left column of a help line, e.g. "-o, --output FILE". */
static CBatError cbat__args_left_column(const cbat__arg_spec *spec, CBatString *out) {
    CBatError err = CBAT_OK;
    cbat_string_clear(out);
    if (spec->kind == CBAT__ARG_POSITIONAL) {
        return cbat_string_append(out, spec->name);
    }
    if (spec->short_name != 0) {
        err = cbat_string_append_format(out, "-%c, ", spec->short_name);
    } else {
        err = cbat_string_append(out, "    ");
    }
    if (err == CBAT_OK) {
        err = cbat_string_append_format(out, "--%s", spec->name);
    }
    if (err == CBAT_OK && spec->kind == CBAT__ARG_OPTION) {
        err = cbat_string_append_format(out, " %s", spec->metavar);
    }
    return err;
}

static CBatError cbat__args_help_line(CBatString *out, const char *left, const char *help,
                                      const char *default_value, size_t width) {
    CBatError err = cbat_string_append_format(out, "  %s", left);
    size_t pad = strlen(left) < width ? width - strlen(left) : 1;
    while (err == CBAT_OK && pad-- > 0) {
        err = cbat_string_append_char(out, ' ');
    }
    if (err == CBAT_OK) {
        err = cbat_string_append(out, help);
    }
    if (err == CBAT_OK && default_value != NULL) {
        err = cbat_string_append_format(out, " (default: %s)", default_value);
    }
    if (err == CBAT_OK) {
        err = cbat_string_append_char(out, '\n');
    }
    return err;
}

CBatError cbat_args_help(const CBatArgs *args, CBatString **out) {
    CBatString *text;
    CBatString *left;
    size_t width = strlen("-h, --help");
    size_t k;
    bool any_positional = false;
    CBatError err;
    if (args == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    text = cbat_string_new();
    left = cbat_string_new();
    if (text == NULL || left == NULL) {
        cbat_string_free(text);
        cbat_string_free(left);
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_string_append_format(text, "usage: %s [options]", args->program);
    for (k = 0; k < args->specs->length && err == CBAT_OK; k++) {
        const cbat__arg_spec *spec = (const cbat__arg_spec *)cbat_list_get(args->specs, k);
        if (spec->kind == CBAT__ARG_POSITIONAL) {
            any_positional = true;
            err = cbat_string_append_format(text, spec->required ? " %s" : " [%s]", spec->name);
        }
        if (err == CBAT_OK) {
            err = cbat__args_left_column(spec, left);
        }
        if (err == CBAT_OK && left->length > width) {
            width = left->length;
        }
    }
    width = CBAT_MIN(width, 28) + 2;
    if (err == CBAT_OK && args->description[0] != '\0') {
        err = cbat_string_append_format(text, "\n\n%s", args->description);
    }
    if (err == CBAT_OK && any_positional) {
        err = cbat_string_append(text, "\n\npositional arguments:\n");
        for (k = 0; k < args->specs->length && err == CBAT_OK; k++) {
            const cbat__arg_spec *spec = (const cbat__arg_spec *)cbat_list_get(args->specs, k);
            if (spec->kind == CBAT__ARG_POSITIONAL) {
                err = cbat__args_help_line(text, spec->name, spec->help, NULL, width);
            }
        }
        if (err == CBAT_OK) {
            cbat_string_truncate(text, text->length - 1);
        }
    }
    if (err == CBAT_OK) {
        err = cbat_string_append(text, "\n\noptions:\n");
    }
    if (err == CBAT_OK) {
        err = cbat__args_help_line(text, "-h, --help", "show this help and exit", NULL, width);
    }
    for (k = 0; k < args->specs->length && err == CBAT_OK; k++) {
        const cbat__arg_spec *spec = (const cbat__arg_spec *)cbat_list_get(args->specs, k);
        if (spec->kind == CBAT__ARG_POSITIONAL) {
            continue;
        }
        err = cbat__args_left_column(spec, left);
        if (err == CBAT_OK) {
            err = cbat__args_help_line(text, left->data, spec->help, spec->default_value, width);
        }
    }
    cbat_string_free(left);
    if (err != CBAT_OK) {
        cbat_string_free(text);
        return err;
    }
    *out = text;
    return CBAT_OK;
}
