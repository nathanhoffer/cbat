/*
 * cbat/file.c - whole-file I/O and the line reader.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/file.h"

#include <errno.h>
#include <string.h>

#include "cbat/fs.h"

#define CBAT__FILE_CHUNK 65536

struct CBatLineReader {
    FILE *file;
    bool owns_file;
    CBatString *line;
    size_t line_number;
    bool finished;
};

static CBatError cbat__file_open(const char *path, const char *mode, FILE **out) {
    FILE *file;
    if (path == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    errno = 0;
    file = cbat_fs_open(path, mode);
    if (file == NULL) {
        return errno != 0 ? cbat_error_from_errno(errno) : CBAT_ERR_IO;
    }
    *out = file;
    return CBAT_OK;
}

CBatError cbat_file_read(const char *path, CBatString **out) {
    FILE *file = NULL;
    CBatString *content;
    CBatError err;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__file_open(path, "rb", &file);
    if (err != CBAT_OK) {
        return err;
    }
    content = cbat_string_new();
    if (content == NULL) {
        fclose(file);
        return CBAT_ERR_NO_MEMORY;
    }
    for (;;) {
        size_t got;
        err = cbat_string_reserve(content, CBAT__FILE_CHUNK);
        if (err != CBAT_OK) {
            break;
        }
        got = fread(content->data + content->length, 1, CBAT__FILE_CHUNK, file);
        content->length += got;
        content->data[content->length] = '\0';
        if (got < CBAT__FILE_CHUNK) {
            if (ferror(file)) {
                err = CBAT_ERR_IO;
            }
            break;
        }
    }
    fclose(file);
    if (err != CBAT_OK) {
        cbat_string_free(content);
        return err;
    }
    *out = content;
    return CBAT_OK;
}

static CBatError cbat__file_write_mode(const char *path, const char *mode, const void *data,
                                       size_t length) {
    FILE *file = NULL;
    CBatError err;
    if (data == NULL && length > 0) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__file_open(path, mode, &file);
    if (err != CBAT_OK) {
        return err;
    }
    if (length > 0 && fwrite(data, 1, length, file) != length) {
        err = CBAT_ERR_IO;
    }
    if (fclose(file) != 0 && err == CBAT_OK) {
        err = CBAT_ERR_IO;
    }
    return err;
}

CBatError cbat_file_write(const char *path, const void *data, size_t length) {
    return cbat__file_write_mode(path, "wb", data, length);
}

CBatError cbat_file_write_text(const char *path, const char *text) {
    if (text == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat__file_write_mode(path, "wb", text, strlen(text));
}

CBatError cbat_file_append(const char *path, const void *data, size_t length) {
    return cbat__file_write_mode(path, "ab", data, length);
}

CBatError cbat_file_copy(const char *from, const char *to) {
    FILE *in = NULL;
    FILE *out = NULL;
    unsigned char *buffer;
    CBatError err;
    if (from == NULL || to == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    buffer = (unsigned char *)cbat_alloc(NULL, CBAT__FILE_CHUNK);
    if (buffer == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    err = cbat__file_open(from, "rb", &in);
    if (err == CBAT_OK) {
        err = cbat__file_open(to, "wb", &out);
    }
    while (err == CBAT_OK) {
        size_t got = fread(buffer, 1, CBAT__FILE_CHUNK, in);
        if (got > 0 && fwrite(buffer, 1, got, out) != got) {
            err = CBAT_ERR_IO;
        } else if (got < CBAT__FILE_CHUNK) {
            if (ferror(in)) {
                err = CBAT_ERR_IO;
            }
            break;
        }
    }
    if (in != NULL) {
        fclose(in);
    }
    if (out != NULL && fclose(out) != 0 && err == CBAT_OK) {
        err = CBAT_ERR_IO;
    }
    cbat_free(buffer);
    return err;
}

/* ---- Line reader -------------------------------------------------------- */

static CBatError cbat__line_reader_new(FILE *file, bool owns_file, CBatLineReader **out) {
    CBatLineReader *reader = (CBatLineReader *)cbat_alloc(NULL, sizeof(CBatLineReader));
    if (reader == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    reader->line = cbat_string_new();
    if (reader->line == NULL) {
        cbat_free(reader);
        return CBAT_ERR_NO_MEMORY;
    }
    reader->file = file;
    reader->owns_file = owns_file;
    reader->line_number = 0;
    reader->finished = false;
    *out = reader;
    return CBAT_OK;
}

CBatError cbat_line_reader_open(const char *path, CBatLineReader **out) {
    FILE *file = NULL;
    CBatError err;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__file_open(path, "rb", &file);
    if (err != CBAT_OK) {
        return err;
    }
    err = cbat__line_reader_new(file, true, out);
    if (err != CBAT_OK) {
        fclose(file);
    }
    return err;
}

CBatError cbat_line_reader_from_file(FILE *file, CBatLineReader **out) {
    if (file == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat__line_reader_new(file, false, out);
}

CBatError cbat_line_reader_next(CBatLineReader *reader, const char **line, size_t *length) {
    CBatString *s;
    bool any = false;
    if (reader == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (reader->finished) {
        return CBAT_ERR_END;
    }
    s = reader->line;
    cbat_string_clear(s);
    for (;;) {
        int c = fgetc(reader->file);
        CBatError err;
        if (c == EOF) {
            if (ferror(reader->file)) {
                return CBAT_ERR_IO;
            }
            reader->finished = true;
            if (!any) {
                return CBAT_ERR_END;
            }
            break;
        }
        any = true;
        if (c == '\n') {
            break;
        }
        err = cbat_string_append_char(s, (char)c);
        if (err != CBAT_OK) {
            return err;
        }
    }
    if (s->length > 0 && s->data[s->length - 1] == '\r') {
        cbat_string_truncate(s, s->length - 1);
    }
    reader->line_number++;
    if (line != NULL) {
        *line = s->data;
    }
    if (length != NULL) {
        *length = s->length;
    }
    return CBAT_OK;
}

size_t cbat_line_reader_line_number(const CBatLineReader *reader) {
    return reader == NULL ? 0 : reader->line_number;
}

void cbat_line_reader_close(CBatLineReader *reader) {
    if (reader == NULL) {
        return;
    }
    if (reader->owns_file) {
        fclose(reader->file);
    }
    cbat_string_free(reader->line);
    cbat_free(reader);
}

CBatError cbat_file_read_lines(const char *path, CBatList **out) {
    CBatLineReader *reader = NULL;
    CBatList *lines;
    const char *line;
    size_t length;
    CBatError err;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat_line_reader_open(path, &reader);
    if (err != CBAT_OK) {
        return err;
    }
    lines = cbat_list_new(sizeof(CBatString *));
    if (lines == NULL) {
        cbat_line_reader_close(reader);
        return CBAT_ERR_NO_MEMORY;
    }
    cbat_list_set_destructor(lines, cbat_string_destructor);
    while ((err = cbat_line_reader_next(reader, &line, &length)) == CBAT_OK) {
        CBatString *copy = cbat_string_from_length(line, length);
        if (copy == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            break;
        }
        err = cbat_list_push(lines, &copy);
        if (err != CBAT_OK) {
            cbat_string_free(copy);
            break;
        }
    }
    cbat_line_reader_close(reader);
    if (err != CBAT_ERR_END) {
        cbat_list_free(lines);
        return err;
    }
    *out = lines;
    return CBAT_OK;
}
