/*
 * cbat/path.h - pure string operations on file paths (cbat's answer to
 * Python's os.path / pathlib.PurePath). Nothing here touches the disk.
 *
 *     CBatString *p = cbat_path_join("data", "report.csv");   // "data/report.csv"
 *     CBatString *ext = cbat_path_extension(cbat_string_cstr(p)); // ".csv"
 *
 * cbat writes '/' as the separator on every platform (Windows accepts it).
 * On Windows, '\' is also recognized as a separator and "C:" as a drive.
 * Functions returning CBatString * return NULL only when out of memory;
 * a NULL path argument is treated as "".
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_PATH_H
#define CBAT_PATH_H

#include "cbat/base.h"
#include "cbat/string.h"

CBAT_BEGIN_DECLS

/** True for '/' everywhere, and also '\' on Windows. */
CBAT_API bool cbat_path_is_separator(char c);

/** True for "/x", and on Windows also "\x", "C:\x", "C:/x" and "\\server\x". */
CBAT_API bool cbat_path_is_absolute(const char *path);

/**
 * Joins two paths with '/'. If `child` is absolute it replaces `base`
 * (like os.path.join).
 */
CBAT_API CBatString *cbat_path_join(const char *base, const char *child);

/** Joins several parts in order with the same rules as cbat_path_join(). */
CBAT_API CBatString *cbat_path_join_many(const char *const *parts, size_t count);

/** Last component, ignoring trailing separators: "a/b.txt" -> "b.txt", "a/b/" -> "b". */
CBAT_API CBatString *cbat_path_basename(const char *path);

/** Everything but the last component: "a/b" -> "a", "b" -> ".", "/a" -> "/". */
CBAT_API CBatString *cbat_path_dirname(const char *path);

/** Final suffix of the basename: "x.tar.gz" -> ".gz", ".bashrc" -> "", "x" -> "". */
CBAT_API CBatString *cbat_path_extension(const char *path);

/** Basename without its extension: "dir/x.tar.gz" -> "x.tar". */
CBAT_API CBatString *cbat_path_stem(const char *path);

/**
 * Replaces the extension ("a/x.txt", ".md" -> "a/x.md"). `extension` may
 * omit the dot; "" removes the extension.
 */
CBAT_API CBatString *cbat_path_with_extension(const char *path, const char *extension);

/**
 * Lexically cleans a path: collapses repeated separators, removes "." parts,
 * resolves "name/.." pairs ("a/./b/../c" -> "a/c"). Never touches the disk,
 * so symlinks are not resolved. An empty result becomes ".".
 */
CBAT_API CBatString *cbat_path_normalize(const char *path);

CBAT_END_DECLS

#endif /* CBAT_PATH_H */
