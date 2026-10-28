/*
 * Recipe 4: Finding files by extension
 *
 * PROBLEM
 *   Recursively find every ".c" file under a directory and report sizes,
 *   like `find . -name '*.c'` or Python's pathlib.Path.rglob('*.c').
 *
 * SOLUTION
 *   cbat_fs_walk() calls you back for every entry; cbat_path_extension()
 *   checks the suffix. The callback's user_data pointer carries your state.
 *
 * DISCUSSION
 *   The walk is depth-first and sorted, so output is deterministic on every
 *   platform. Return false from the callback to stop early. The recipe
 *   builds its own sandbox in the temp directory and deletes it with
 *   cbat_fs_remove_tree() so it is safe to run anywhere.
 */
#include <stdio.h>

#include "cbat/cbat.h"

typedef struct Search {
    const char *extension;
    size_t root_length;
    int matches;
    uint64_t total_bytes;
} Search;

static bool visit(const char *path, bool is_dir, void *user_data) {
    Search *search = (Search *)user_data;
    CBatString *ext;
    if (is_dir) {
        return true;
    }
    ext = cbat_path_extension(path);
    if (ext != NULL && cbat_string_equals(ext, search->extension)) {
        uint64_t size = 0;
        cbat_fs_file_size(path, &size);
        printf("  %-28s %4lu bytes\n", path + search->root_length + 1, (unsigned long)size);
        search->matches++;
        search->total_bytes += size;
    }
    cbat_string_free(ext);
    return true;
}

static void make_file(const CBatString *root, const char *relative, const char *content) {
    CBatString *path = cbat_path_join(cbat_string_cstr(root), relative);
    CBatString *dir = cbat_path_dirname(cbat_string_cstr(path));
    cbat_fs_make_dirs(cbat_string_cstr(dir));
    cbat_file_write_text(cbat_string_cstr(path), content);
    cbat_string_free(dir);
    cbat_string_free(path);
}

int main(void) {
    CBatString *root = NULL;
    Search search = {".c", 0, 0, 0};
    CBatError err = cbat_fs_make_temp_dir("cookbook-", &root);
    if (err != CBAT_OK) {
        fprintf(stderr, "temp dir: %s\n", cbat_error_str(err));
        return 1;
    }
    make_file(root, "main.c", "int main(void) { return 0; }\n");
    make_file(root, "README.md", "# demo\n");
    make_file(root, "src/list.c", "/* list */\n");
    make_file(root, "src/list.h", "/* header */\n");
    make_file(root, "src/deep/er/tree.c", "/* tree */\n");

    search.root_length = cbat_string_length(root);
    printf("C files:\n");
    err = cbat_fs_walk(cbat_string_cstr(root), visit, &search);
    printf("%d file(s), %lu bytes total\n", search.matches, (unsigned long)search.total_bytes);

    cbat_fs_remove_tree(cbat_string_cstr(root));
    cbat_string_free(root);
    return err == CBAT_OK && search.matches == 3 ? 0 : 1;
}
