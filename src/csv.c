/*
 * cbat/csv.c - RFC 4180 CSV reader and writer.
 *
 * One state machine reads from either a memory buffer or a FILE. Parsing is
 * lenient like Python's csv module: characters after a closing quote are
 * kept, and quotes inside unquoted fields are literal. Blank lines are
 * skipped.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/csv.h"

#include <errno.h>
#include <string.h>

#include "cbat/file.h"
#include "cbat/fs.h"

#define CBAT__CSV_NONE (-2)

typedef struct cbat__csv_source {
    const char *text;   /* memory source, or NULL */
    size_t length;
    FILE *file;         /* file source, or NULL */
    int pushed;         /* pushed-back character or CBAT__CSV_NONE */
    bool previous_cr;
    size_t line;        /* 1-based line of the next character */
    size_t column;
    size_t offset;
} cbat__csv_source;

static void cbat__csv_source_init(cbat__csv_source *src, const char *text, size_t length,
                                  FILE *file) {
    src->text = text;
    src->length = length;
    src->file = file;
    src->pushed = CBAT__CSV_NONE;
    src->previous_cr = false;
    src->line = 1;
    src->column = 1;
    src->offset = 0;
}

static int cbat__csv_getc(cbat__csv_source *src) {
    int c;
    if (src->pushed != CBAT__CSV_NONE) {
        c = src->pushed;
        src->pushed = CBAT__CSV_NONE;
        return c;
    }
    if (src->file != NULL) {
        c = fgetc(src->file);
    } else if (src->offset < src->length) {
        c = (unsigned char)src->text[src->offset];
    } else {
        c = EOF;
    }
    if (c == EOF) {
        return EOF;
    }
    src->offset++;
    /* "\r\n" counts as one line break. */
    if (c == '\n') {
        if (!src->previous_cr) {
            src->line++;
        }
        src->column = 1;
    } else if (c == '\r') {
        src->line++;
        src->column = 1;
    } else {
        src->column++;
    }
    src->previous_cr = c == '\r';
    return c;
}

/* Only ever used for the character just read, so position bookkeeping is
   unaffected: the character is simply handed out again. */
static void cbat__csv_ungetc(cbat__csv_source *src, int c) {
    src->pushed = c;
}

static CBatError cbat__csv_push_field(CBatList *fields, const CBatString *buffer) {
    CBatString *field = cbat_string_from_length(buffer->data, buffer->length);
    CBatError err;
    if (field == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat_list_push(fields, &field);
    if (err != CBAT_OK) {
        cbat_string_free(field);
    }
    return err;
}

/*
 * Reads one row into `fields` (which must be empty). Returns CBAT_ERR_END at
 * end of input. *row_line receives the row's first line.
 */
static CBatError cbat__csv_read_row(cbat__csv_source *src, const CBatCsvOptions *options,
                                    CBatList *fields, CBatString *buffer, size_t *row_line,
                                    CBatParseInfo *info) {
    int c = cbat__csv_getc(src);
    CBatError err;
    /* Skip blank lines. */
    while (c == '\n' || c == '\r') {
        c = cbat__csv_getc(src);
    }
    if (c == EOF) {
        if (src->file != NULL && ferror(src->file)) {
            return CBAT_ERR_IO;
        }
        return CBAT_ERR_END;
    }
    *row_line = src->line;
    for (;;) {
        cbat_string_clear(buffer);
        if (c == options->quote) {
            size_t start_line = src->line;
            size_t start_column = src->column - 1;
            size_t start_offset = src->offset - 1;
            for (;;) {
                c = cbat__csv_getc(src);
                if (c == EOF) {
                    if (info != NULL) {
                        info->line = start_line;
                        info->column = start_column;
                        info->offset = start_offset;
                        info->message = "unterminated quoted field";
                    }
                    return CBAT_ERR_PARSE;
                }
                if (c == options->quote) {
                    c = cbat__csv_getc(src);
                    if (c != options->quote) {
                        break; /* closing quote; c is the following character */
                    }
                }
                err = cbat_string_append_char(buffer, (char)c);
                if (err != CBAT_OK) {
                    return err;
                }
            }
        }
        while (c != options->delimiter && c != '\n' && c != '\r' && c != EOF) {
            err = cbat_string_append_char(buffer, (char)c);
            if (err != CBAT_OK) {
                return err;
            }
            c = cbat__csv_getc(src);
        }
        err = cbat__csv_push_field(fields, buffer);
        if (err != CBAT_OK) {
            return err;
        }
        if (c == options->delimiter) {
            c = cbat__csv_getc(src);
            continue;
        }
        if (c == '\r') {
            int next = cbat__csv_getc(src);
            if (next != '\n' && next != EOF) {
                cbat__csv_ungetc(src, next);
            }
        }
        if (c == EOF && src->file != NULL && ferror(src->file)) {
            return CBAT_ERR_IO;
        }
        return CBAT_OK;
    }
}

CBatCsvOptions cbat_csv_default_options(void) {
    CBatCsvOptions options;
    options.delimiter = ',';
    options.quote = '"';
    options.line_ending = "\n";
    return options;
}

static CBatCsvOptions cbat__csv_resolve(const CBatCsvOptions *options) {
    CBatCsvOptions resolved = cbat_csv_default_options();
    if (options != NULL) {
        if (options->delimiter != '\0') {
            resolved.delimiter = options->delimiter;
        }
        if (options->quote != '\0') {
            resolved.quote = options->quote;
        }
        if (options->line_ending != NULL) {
            resolved.line_ending = options->line_ending;
        }
    }
    return resolved;
}

static CBatList *cbat__csv_new_field_list(void) {
    CBatList *fields = cbat_list_new(sizeof(CBatString *));
    if (fields != NULL) {
        cbat_list_set_destructor(fields, cbat_string_destructor);
    }
    return fields;
}

CBatError cbat_csv_parse(const char *text, size_t length, const CBatCsvOptions *options,
                         CBatList **rows, CBatParseInfo *info) {
    CBatCsvOptions resolved = cbat__csv_resolve(options);
    cbat__csv_source src;
    CBatList *result;
    CBatString *buffer;
    CBatError err;
    size_t row_line;
    if ((text == NULL && length > 0) || rows == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (resolved.delimiter == resolved.quote) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    cbat__csv_source_init(&src, text, length, NULL);
    result = cbat_list_new(sizeof(CBatList *));
    buffer = cbat_string_new();
    if (result == NULL || buffer == NULL) {
        cbat_list_free(result);
        cbat_string_free(buffer);
        return CBAT_ERR_NO_MEMORY;
    }
    cbat_list_set_destructor(result, cbat_list_destructor);
    for (;;) {
        CBatList *fields = cbat__csv_new_field_list();
        if (fields == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            break;
        }
        err = cbat__csv_read_row(&src, &resolved, fields, buffer, &row_line, info);
        if (err == CBAT_OK) {
            err = cbat_list_push(result, &fields);
        }
        if (err != CBAT_OK) {
            cbat_list_free(fields);
            break;
        }
    }
    cbat_string_free(buffer);
    if (err != CBAT_ERR_END) {
        cbat_list_free(result);
        return err;
    }
    *rows = result;
    return CBAT_OK;
}

CBatError cbat_csv_read_file(const char *path, const CBatCsvOptions *options, CBatList **rows,
                             CBatParseInfo *info) {
    CBatString *text = NULL;
    CBatError err = cbat_file_read(path, &text);
    if (err != CBAT_OK) {
        return err;
    }
    err = cbat_csv_parse(text->data, text->length, options, rows, info);
    cbat_string_free(text);
    return err;
}

/* ---- Streaming reader --------------------------------------------------- */

struct CBatCsvReader {
    cbat__csv_source source;
    CBatCsvOptions options;
    CBatList *fields;
    CBatString *buffer;
    size_t row_line;
};

CBatError cbat_csv_reader_open(const char *path, const CBatCsvOptions *options,
                               CBatCsvReader **out) {
    CBatCsvReader *reader;
    FILE *file;
    if (path == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    reader = (CBatCsvReader *)cbat_alloc_zeroed(NULL, sizeof(CBatCsvReader));
    if (reader == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    reader->options = cbat__csv_resolve(options);
    reader->fields = cbat__csv_new_field_list();
    reader->buffer = cbat_string_new();
    if (reader->fields == NULL || reader->buffer == NULL) {
        cbat_csv_reader_close(reader);
        return CBAT_ERR_NO_MEMORY;
    }
    errno = 0;
    file = cbat_fs_open(path, "rb");
    if (file == NULL) {
        CBatError err = errno != 0 ? cbat_error_from_errno(errno) : CBAT_ERR_IO;
        cbat_csv_reader_close(reader);
        return err;
    }
    cbat__csv_source_init(&reader->source, NULL, 0, file);
    *out = reader;
    return CBAT_OK;
}

CBatError cbat_csv_reader_next(CBatCsvReader *reader, const CBatList **fields) {
    CBatError err;
    if (reader == NULL || fields == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    cbat_list_clear(reader->fields);
    err = cbat__csv_read_row(&reader->source, &reader->options, reader->fields, reader->buffer,
                             &reader->row_line, NULL);
    if (err == CBAT_OK) {
        *fields = reader->fields;
    }
    return err;
}

size_t cbat_csv_reader_line_number(const CBatCsvReader *reader) {
    return reader == NULL ? 0 : reader->row_line;
}

void cbat_csv_reader_close(CBatCsvReader *reader) {
    if (reader == NULL) {
        return;
    }
    if (reader->source.file != NULL) {
        fclose(reader->source.file);
    }
    cbat_list_free(reader->fields);
    cbat_string_free(reader->buffer);
    cbat_free(reader);
}

/* ---- Writing ------------------------------------------------------------ */

static bool cbat__csv_needs_quotes(const char *field, size_t length,
                                   const CBatCsvOptions *options) {
    size_t i;
    if (length == 0) {
        return false;
    }
    if (field[0] == ' ' || field[length - 1] == ' ') {
        return true;
    }
    for (i = 0; i < length; i++) {
        char c = field[i];
        if (c == options->delimiter || c == options->quote || c == '\n' || c == '\r') {
            return true;
        }
    }
    return false;
}

static CBatError cbat__csv_append_field(CBatString *out, const char *field, size_t length,
                                        const CBatCsvOptions *options, bool only_field) {
    CBatError err;
    size_t i;
    /* A single empty field must not become a blank (skipped) record. */
    if (!(only_field && length == 0) && !cbat__csv_needs_quotes(field, length, options)) {
        return cbat_string_append_length(out, field, length);
    }
    err = cbat_string_append_char(out, options->quote);
    for (i = 0; i < length && err == CBAT_OK; i++) {
        if (field[i] == options->quote) {
            err = cbat_string_append_char(out, options->quote);
        }
        if (err == CBAT_OK) {
            err = cbat_string_append_char(out, field[i]);
        }
    }
    if (err == CBAT_OK) {
        err = cbat_string_append_char(out, options->quote);
    }
    return err;
}

CBatError cbat_csv_append_row(CBatString *out, const char *const *fields, size_t count,
                              const CBatCsvOptions *options) {
    CBatCsvOptions resolved = cbat__csv_resolve(options);
    size_t original_length;
    size_t i;
    CBatError err = CBAT_OK;
    if (out == NULL || (fields == NULL && count > 0)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    original_length = out->length;
    for (i = 0; i < count && err == CBAT_OK; i++) {
        const char *field = fields[i] == NULL ? "" : fields[i];
        if (i > 0) {
            err = cbat_string_append_char(out, resolved.delimiter);
        }
        if (err == CBAT_OK) {
            err = cbat__csv_append_field(out, field, strlen(field), &resolved, count == 1);
        }
    }
    if (err == CBAT_OK) {
        err = cbat_string_append(out, resolved.line_ending);
    }
    if (err != CBAT_OK) {
        cbat_string_truncate(out, original_length); /* all or nothing */
    }
    return err;
}

CBatError cbat_csv_append_row_list(CBatString *out, const CBatList *fields,
                                   const CBatCsvOptions *options) {
    CBatCsvOptions resolved = cbat__csv_resolve(options);
    size_t original_length;
    size_t i;
    CBatError err = CBAT_OK;
    if (out == NULL || fields == NULL || fields->element_size != sizeof(CBatString *)) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    original_length = out->length;
    for (i = 0; i < fields->length && err == CBAT_OK; i++) {
        const CBatString *field = *(CBatString *const *)cbat_list_get(fields, i);
        if (i > 0) {
            err = cbat_string_append_char(out, resolved.delimiter);
        }
        if (err == CBAT_OK) {
            err = cbat__csv_append_field(out, field == NULL ? "" : field->data,
                                          field == NULL ? 0 : field->length, &resolved,
                                          fields->length == 1);
        }
    }
    if (err == CBAT_OK) {
        err = cbat_string_append(out, resolved.line_ending);
    }
    if (err != CBAT_OK) {
        cbat_string_truncate(out, original_length);
    }
    return err;
}
