/*
 * cbat/csv.h - reading and writing CSV (RFC 4180) (cbat's answer to
 * Python's csv module).
 *
 *     CBatList *rows = NULL;
 *     CBatParseInfo info;
 *     if (cbat_csv_parse(text, strlen(text), NULL, &rows, &info) == CBAT_OK) {
 *         CBatList *first = CBAT_LIST_GET(rows, CBatList *, 0);
 *         puts(cbat_string_cstr(CBAT_LIST_GET(first, CBatString *, 0)));
 *         cbat_list_free(rows);
 *     }
 *
 * Rows are lists of `CBatString *`; quoted fields may contain the delimiter,
 * doubled quotes ("") and newlines. "\n", "\r\n" and "\r" all end a row.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_CSV_H
#define CBAT_CSV_H

#include <stdio.h>

#include "cbat/base.h"
#include "cbat/list.h"
#include "cbat/string.h"

CBAT_BEGIN_DECLS

typedef struct CBatCsvOptions {
    char delimiter;          /* default ',' */
    char quote;              /* default '"' */
    const char *line_ending; /* used when writing; default "\n" */
} CBatCsvOptions;

/** The defaults: ',', '"', "\n". */
CBAT_API CBatCsvOptions cbat_csv_default_options(void);

/**
 * Parses all of `text`. *rows receives a list of `CBatList *` rows, each a
 * list of `CBatString *` fields; freeing *rows frees everything.
 * `options` and `info` may be NULL. An unterminated quoted field is
 * CBAT_ERR_PARSE with its position in *info.
 */
CBAT_API CBatError cbat_csv_parse(const char *text, size_t length, const CBatCsvOptions *options,
                                  CBatList **rows, CBatParseInfo *info);

/** Reads and parses a whole CSV file. */
CBAT_API CBatError cbat_csv_read_file(const char *path, const CBatCsvOptions *options,
                                      CBatList **rows, CBatParseInfo *info);

/* ---- Streaming reader --------------------------------------------------- */

typedef struct CBatCsvReader CBatCsvReader;

/** Streams rows from a file without loading it all. */
CBAT_API CBatError cbat_csv_reader_open(const char *path, const CBatCsvOptions *options,
                                        CBatCsvReader **out);

/**
 * Next row as a list of `CBatString *` owned by the reader and valid until
 * the next call. CBAT_ERR_END after the last row.
 */
CBAT_API CBatError cbat_csv_reader_next(CBatCsvReader *reader, const CBatList **fields);

/** Line on which the most recent row started (1-based). */
CBAT_API size_t cbat_csv_reader_line_number(const CBatCsvReader *reader);

CBAT_API void cbat_csv_reader_close(CBatCsvReader *reader);

/* ---- Writing ------------------------------------------------------------ */

/**
 * Appends one row to `out`, quoting fields that contain the delimiter, the
 * quote character, a line break, or leading/trailing spaces.
 */
CBAT_API CBatError cbat_csv_append_row(CBatString *out, const char *const *fields, size_t count,
                                       const CBatCsvOptions *options);

/** Same, for a list of `CBatString *`. */
CBAT_API CBatError cbat_csv_append_row_list(CBatString *out, const CBatList *fields,
                                            const CBatCsvOptions *options);

CBAT_END_DECLS

#endif /* CBAT_CSV_H */
