/*
 * cbat/fs.h - the file system: existence checks, directories, renaming,
 * listing, walking and temporary directories (cbat's answer to Python's
 * os, os.path.exists, shutil and tempfile).
 *
 *     CBatList *names = NULL;
 *     if (cbat_fs_list_dir(".", &names) == CBAT_OK) {
 *         CBAT_LIST_FOREACH(CBatString *, name, names) puts(cbat_string_cstr(*name));
 *         cbat_list_free(names);
 *     }
 *
 * All paths are UTF-8, including on Windows.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_FS_H
#define CBAT_FS_H

#include <stdio.h>

#include "cbat/base.h"
#include "cbat/list.h"
#include "cbat/string.h"

CBAT_BEGIN_DECLS

/**
 * fopen() that understands UTF-8 paths on every platform. Returns NULL on
 * failure with errno set (see cbat_error_from_errno()).
 */
CBAT_API FILE *cbat_fs_open(const char *path, const char *mode);

CBAT_API bool cbat_fs_exists(const char *path);
CBAT_API bool cbat_fs_is_file(const char *path);
CBAT_API bool cbat_fs_is_dir(const char *path);

/** Size of a regular file in bytes. */
CBAT_API CBatError cbat_fs_file_size(const char *path, uint64_t *size);

/** Creates one directory. CBAT_ERR_ALREADY_EXISTS if it exists. */
CBAT_API CBatError cbat_fs_make_dir(const char *path);

/** Creates a directory and any missing parents (mkdir -p). OK if it exists. */
CBAT_API CBatError cbat_fs_make_dirs(const char *path);

/** Deletes a file. */
CBAT_API CBatError cbat_fs_remove_file(const char *path);

/** Deletes an empty directory. */
CBAT_API CBatError cbat_fs_remove_dir(const char *path);

/**
 * Deletes a file, or a directory and EVERYTHING inside it (shutil.rmtree).
 * Symbolic links are removed, never followed.
 */
CBAT_API CBatError cbat_fs_remove_tree(const char *path);

/** Renames/moves, replacing `to` if it is an existing file. */
CBAT_API CBatError cbat_fs_rename(const char *from, const char *to);

/**
 * Names (not paths) of the entries in a directory, sorted, without "." and
 * "..". *out is a list of `CBatString *` that owns its strings.
 */
CBAT_API CBatError cbat_fs_list_dir(const char *path, CBatList **out);

/**
 * Called by cbat_fs_walk() for every entry below the root (depth first,
 * sorted). Return false to stop the walk early.
 */
typedef bool (*CBatWalkFunc)(const char *path, bool is_dir, void *user_data);

/** Visits every file and directory below `root` (not following symlinks). */
CBAT_API CBatError cbat_fs_walk(const char *root, CBatWalkFunc visit, void *user_data);

/** The system's temporary directory (TMPDIR, TEMP, /tmp, ...). */
CBAT_API CBatError cbat_fs_temp_dir(CBatString **out);

/** Creates a new, uniquely named directory inside the temp directory. */
CBAT_API CBatError cbat_fs_make_temp_dir(const char *prefix, CBatString **out);

/** The current working directory. */
CBAT_API CBatError cbat_fs_current_dir(CBatString **out);

CBAT_END_DECLS

#endif /* CBAT_FS_H */
