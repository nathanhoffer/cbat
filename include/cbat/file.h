/*
 * cbat/file.h - whole-file reading/writing, line-by-line reading and copying
 * (cbat's answer to Python's open(...).read(), readlines() and shutil.copy).
 *
 *     CBatString *text = NULL;
 *     CBatError err = cbat_file_read("notes.txt", &text);
 *     if (err == CBAT_OK) { ... cbat_string_free(text); }
 *
 *     CBatLineReader *reader = NULL;
 *     if (cbat_line_reader_open("data.txt", &reader) == CBAT_OK) {
 *         const char *line;
 *         size_t length;
 *         while (cbat_line_reader_next(reader, &line, &length) == CBAT_OK) {
 *             printf("%zu: %s\n", cbat_line_reader_line_number(reader), line);
 *         }
 *         cbat_line_reader_close(reader);
 *     }
 *
 * Files are opened in binary mode, so bytes come back exactly as stored.
 * Line functions strip "\n" and "\r\n" endings. Paths are UTF-8.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_FILE_H
#define CBAT_FILE_H

#include <stdio.h>

#include "cbat/base.h"
#include "cbat/list.h"
#include "cbat/string.h"

CBAT_BEGIN_DECLS

/** Reads an entire file into a new string (may contain NUL bytes). */
CBAT_API CBatError cbat_file_read(const char *path, CBatString **out);

/** Reads every line into a new list of `CBatString *` (owned by the list). */
CBAT_API CBatError cbat_file_read_lines(const char *path, CBatList **out);

/** Creates or truncates `path` and writes `length` bytes. */
CBAT_API CBatError cbat_file_write(const char *path, const void *data, size_t length);

/** Creates or truncates `path` and writes a C string. */
CBAT_API CBatError cbat_file_write_text(const char *path, const char *text);

/** Appends bytes, creating the file if needed. */
CBAT_API CBatError cbat_file_append(const char *path, const void *data, size_t length);

/** Copies a file's bytes to `to` (created or replaced). */
CBAT_API CBatError cbat_file_copy(const char *from, const char *to);

/* ---- Line reader -------------------------------------------------------- */

typedef struct CBatLineReader CBatLineReader;

/** Opens `path` for buffered line-by-line reading, using bounded read-ahead. */
CBAT_API CBatError cbat_line_reader_open(const char *path, CBatLineReader **out);

/**
 * Reads lines from an already-open FILE (e.g. stdin). The reader does not
 * close `file` or read ahead past a line, so stdin remains interactive and
 * the caller can continue reading the stream after closing the reader.
 * Do not read or reposition the stream while the reader is in use.
 */
CBAT_API CBatError cbat_line_reader_from_file(FILE *file, CBatLineReader **out);

/**
 * Reads the next line. *line stays valid until the next call. Returns
 * CBAT_ERR_END after the last line, CBAT_ERR_IO on a read error.
 * Embedded NUL bytes are preserved; use *length rather than strlen().
 * On allocation failure, outputs and the line number are unchanged; retrying
 * resumes the same line. A read error is terminal: close the reader.
 */
CBAT_API CBatError cbat_line_reader_next(CBatLineReader *reader, const char **line,
                                         size_t *length);

/** 1-based number of the line most recently returned (0 before the first). */
CBAT_API size_t cbat_line_reader_line_number(const CBatLineReader *reader);

/** Closes the reader (and its file, if it opened one). NULL is ignored. */
CBAT_API void cbat_line_reader_close(CBatLineReader *reader);

CBAT_END_DECLS

#endif /* CBAT_FILE_H */
