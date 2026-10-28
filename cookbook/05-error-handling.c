/*
 * Recipe 5: Handling errors the cbat way
 *
 * PROBLEM
 *   C has no exceptions. You want failures to be impossible to ignore,
 *   easy to report, and cleaned up properly.
 *
 * SOLUTION
 *   Every cbat function that can fail returns a CBatError (CBAT_OK is 0).
 *   cbat_error_str() turns it into a message. The "goto cleanup" pattern
 *   keeps one exit path that frees everything, which is safe because every
 *   cbat *_free() accepts NULL.
 *
 * DISCUSSION
 *   Missing files map to CBAT_ERR_NOT_FOUND on every OS (ENOENT on POSIX,
 *   ERROR_FILE_NOT_FOUND on Windows), so your code can branch on meaning
 *   instead of platform-specific numbers.
 */
#include <stdio.h>

#include "cbat/cbat.h"

static CBatError load_and_count_lines(const char *path, size_t *count) {
    CBatList *lines = NULL;
    CBatError err = cbat_file_read_lines(path, &lines);
    if (err != CBAT_OK) {
        return err; /* nothing allocated yet */
    }
    *count = cbat_list_length(lines);
    cbat_list_free(lines);
    return CBAT_OK;
}

static int run(void) {
    CBatString *dir = NULL;
    CBatString *path = NULL;
    size_t count = 0;
    int status = 1;
    CBatError err;

    err = load_and_count_lines("this/file/does/not/exist.txt", &count);
    if (err == CBAT_ERR_NOT_FOUND) {
        printf("expected failure: %s (%s)\n", cbat_error_str(err), cbat_error_name(err));
    }

    err = cbat_fs_make_temp_dir("cookbook-", &dir);
    if (err != CBAT_OK) {
        goto cleanup;
    }
    path = cbat_path_join(cbat_string_cstr(dir), "notes.txt");
    if (path == NULL) {
        err = CBAT_ERR_NO_MEMORY;
        goto cleanup;
    }
    err = cbat_file_write_text(cbat_string_cstr(path), "first\nsecond\n");
    if (err != CBAT_OK) {
        goto cleanup;
    }
    err = cbat_file_append(cbat_string_cstr(path), "third\n", 6);
    if (err != CBAT_OK) {
        goto cleanup;
    }
    err = load_and_count_lines(cbat_string_cstr(path), &count);
    if (err != CBAT_OK) {
        goto cleanup;
    }
    printf("notes.txt has %lu lines\n", (unsigned long)count);
    status = count == 3 ? 0 : 1;

cleanup:
    if (err != CBAT_OK) {
        fprintf(stderr, "error: %s\n", cbat_error_str(err));
    }
    if (dir != NULL) {
        cbat_fs_remove_tree(cbat_string_cstr(dir));
    }
    cbat_string_free(path); /* NULL-safe */
    cbat_string_free(dir);
    return status;
}

int main(void) {
    return run();
}
