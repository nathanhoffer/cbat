/*
 * cbat/fs.c - file-system access with POSIX and Win32 backends.
 * On Windows every path is converted from UTF-8 to UTF-16 and the wide
 * APIs are used, so non-ASCII file names work.
 * Part of cbat (C Batteries Included). MIT License.
 */
/* cbat:feature-macros-begin */
#if !defined(_WIN32)
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#  endif
#  if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#    define _DARWIN_C_SOURCE 1
#  endif
#endif
/* cbat:feature-macros-end */

#include "cbat/fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cbat/path.h"
#include "cbat/sort.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <direct.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <wchar.h>
#else
#  include <dirent.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

typedef enum cbat__fs_kind {
    CBAT__FS_NONE,
    CBAT__FS_FILE,
    CBAT__FS_DIR,
    CBAT__FS_LINK,
    CBAT__FS_OTHER
} cbat__fs_kind;

/* ==== Platform layer ===================================================== */

#ifdef _WIN32

static CBatError cbat__fs_win_error(DWORD code) {
    switch (code) {
    case ERROR_SUCCESS:
        return CBAT_OK;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
        return CBAT_ERR_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_WRITE_PROTECT:
        return CBAT_ERR_PERMISSION;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return CBAT_ERR_ALREADY_EXISTS;
    case ERROR_DIR_NOT_EMPTY:
        return CBAT_ERR_NOT_EMPTY;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return CBAT_ERR_NO_MEMORY;
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_FILENAME_EXCED_RANGE:
        return CBAT_ERR_INVALID_ARGUMENT;
    default:
        return CBAT_ERR_IO;
    }
}

/* UTF-8 -> newly allocated UTF-16 (release with cbat_free). Sets *err. */
static wchar_t *cbat__fs_wide(const char *utf8, CBatError *err) {
    int count;
    wchar_t *wide;
    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (count <= 0) {
        *err = CBAT_ERR_INVALID_UTF8;
        return NULL;
    }
    wide = (wchar_t *)cbat_alloc(NULL, (size_t)count * sizeof(wchar_t));
    if (wide == NULL) {
        *err = CBAT_ERR_NO_MEMORY;
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide, count);
    *err = CBAT_OK;
    return wide;
}

/* UTF-16 -> new CBatString. NULL when out of memory. */
static CBatString *cbat__fs_narrow(const wchar_t *wide) {
    int count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    CBatString *s;
    if (count <= 0) {
        return cbat_string_new();
    }
    s = cbat_string_new();
    if (s == NULL || cbat_string_reserve(s, (size_t)count) != CBAT_OK) {
        cbat_string_free(s);
        return NULL;
    }
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, s->data, count, NULL, NULL);
    s->length = (size_t)count - 1;
    s->data[s->length] = '\0';
    return s;
}

FILE *cbat_fs_open(const char *path, const char *mode) {
    CBatError err;
    wchar_t *wpath;
    wchar_t *wmode;
    FILE *file;
    if (path == NULL || mode == NULL) {
        errno = EINVAL;
        return NULL;
    }
    wpath = cbat__fs_wide(path, &err);
    wmode = wpath == NULL ? NULL : cbat__fs_wide(mode, &err);
    if (wmode == NULL) {
        cbat_free(wpath);
        errno = err == CBAT_ERR_NO_MEMORY ? ENOMEM : EINVAL;
        return NULL;
    }
    file = _wfopen(wpath, wmode);
    cbat_free(wpath);
    cbat_free(wmode);
    return file;
}

static CBatError cbat__fs_kind_of(const char *path, bool follow, cbat__fs_kind *kind,
                                  uint64_t *size) {
    CBatError err;
    wchar_t *wpath = cbat__fs_wide(path, &err);
    WIN32_FILE_ATTRIBUTE_DATA data;
    *kind = CBAT__FS_NONE;
    if (wpath == NULL) {
        return err;
    }
    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &data)) {
        err = cbat__fs_win_error(GetLastError());
        cbat_free(wpath);
        return err;
    }
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && follow) {
        struct _stat64 st;
        if (_wstat64(wpath, &st) != 0) {
            err = cbat_error_from_errno(errno);
            cbat_free(wpath);
            return err;
        }
        *kind = (st.st_mode & _S_IFDIR) ? CBAT__FS_DIR : CBAT__FS_FILE;
        if (size != NULL) {
            *size = (uint64_t)st.st_size;
        }
        cbat_free(wpath);
        return CBAT_OK;
    }
    cbat_free(wpath);
    if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        *kind = CBAT__FS_LINK;
    } else if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        *kind = CBAT__FS_DIR;
    } else {
        *kind = CBAT__FS_FILE;
    }
    if (size != NULL) {
        *size = ((uint64_t)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    }
    return CBAT_OK;
}

static CBatError cbat__fs_mkdir(const char *path) {
    CBatError err;
    wchar_t *wpath = cbat__fs_wide(path, &err);
    if (wpath == NULL) {
        return err;
    }
    err = _wmkdir(wpath) == 0 ? CBAT_OK : cbat_error_from_errno(errno);
    cbat_free(wpath);
    return err;
}

static CBatError cbat__fs_unlink(const char *path) {
    CBatError err;
    wchar_t *wpath = cbat__fs_wide(path, &err);
    if (wpath == NULL) {
        return err;
    }
    err = DeleteFileW(wpath) ? CBAT_OK : cbat__fs_win_error(GetLastError());
    cbat_free(wpath);
    return err;
}

static CBatError cbat__fs_rmdir(const char *path) {
    CBatError err;
    wchar_t *wpath = cbat__fs_wide(path, &err);
    if (wpath == NULL) {
        return err;
    }
    err = RemoveDirectoryW(wpath) ? CBAT_OK : cbat__fs_win_error(GetLastError());
    cbat_free(wpath);
    return err;
}

/* Removes a symlink or junction without following it. */
static CBatError cbat__fs_remove_link(const char *path) {
    CBatError err;
    wchar_t *wpath = cbat__fs_wide(path, &err);
    DWORD attributes;
    BOOL ok;
    if (wpath == NULL) {
        return err;
    }
    attributes = GetFileAttributesW(wpath);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        ok = RemoveDirectoryW(wpath);
    } else {
        ok = DeleteFileW(wpath);
    }
    err = ok ? CBAT_OK : cbat__fs_win_error(GetLastError());
    cbat_free(wpath);
    return err;
}

CBatError cbat_fs_rename(const char *from, const char *to) {
    CBatError err;
    wchar_t *wfrom;
    wchar_t *wto;
    if (from == NULL || to == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    wfrom = cbat__fs_wide(from, &err);
    if (wfrom == NULL) {
        return err;
    }
    wto = cbat__fs_wide(to, &err);
    if (wto == NULL) {
        cbat_free(wfrom);
        return err;
    }
    err = MoveFileExW(wfrom, wto, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)
              ? CBAT_OK
              : cbat__fs_win_error(GetLastError());
    cbat_free(wfrom);
    cbat_free(wto);
    return err;
}

/* Appends every entry name (except . and ..) to `names`. */
static CBatError cbat__fs_read_dir(const char *path, CBatList *names) {
    CBatError err;
    CBatString *pattern = cbat_path_join(path, "*");
    wchar_t *wpattern;
    WIN32_FIND_DATAW data;
    HANDLE find;
    if (pattern == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    wpattern = cbat__fs_wide(cbat_string_cstr(pattern), &err);
    cbat_string_free(pattern);
    if (wpattern == NULL) {
        return err;
    }
    find = FindFirstFileW(wpattern, &data);
    cbat_free(wpattern);
    if (find == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();
        /* An existing but empty directory still lists "."; a missing one fails. */
        return cbat__fs_win_error(code == ERROR_FILE_NOT_FOUND ? ERROR_PATH_NOT_FOUND : code);
    }
    err = CBAT_OK;
    do {
        CBatString *name;
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        name = cbat__fs_narrow(data.cFileName);
        if (name == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            break;
        }
        err = cbat_list_push(names, &name);
        if (err != CBAT_OK) {
            cbat_string_free(name);
            break;
        }
    } while (FindNextFileW(find, &data));
    if (err == CBAT_OK && GetLastError() != ERROR_NO_MORE_FILES) {
        err = cbat__fs_win_error(GetLastError());
    }
    FindClose(find);
    return err;
}

CBatError cbat_fs_temp_dir(CBatString **out) {
    wchar_t buffer[MAX_PATH + 2];
    DWORD length;
    CBatString *s;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    length = GetTempPathW(MAX_PATH + 1, buffer);
    if (length == 0 || length > MAX_PATH + 1) {
        return CBAT_ERR_IO;
    }
    while (length > 3 && (buffer[length - 1] == L'\\' || buffer[length - 1] == L'/')) {
        buffer[--length] = L'\0';
    }
    s = cbat__fs_narrow(buffer);
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    *out = s;
    return CBAT_OK;
}

static unsigned long cbat__fs_unique_seed(void) {
    return (unsigned long)GetCurrentProcessId() ^ (unsigned long)GetTickCount() ^
           (unsigned long)time(NULL);
}

CBatError cbat_fs_current_dir(CBatString **out) {
    wchar_t *wide;
    CBatString *s;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    wide = _wgetcwd(NULL, 0);
    if (wide == NULL) {
        return cbat_error_from_errno(errno);
    }
    s = cbat__fs_narrow(wide);
    free(wide); /* allocated by the CRT */
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    *out = s;
    return CBAT_OK;
}

#else /* POSIX */

FILE *cbat_fs_open(const char *path, const char *mode) {
    if (path == NULL || mode == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return fopen(path, mode);
}

static CBatError cbat__fs_kind_of(const char *path, bool follow, cbat__fs_kind *kind,
                                  uint64_t *size) {
    struct stat st;
    int rc = follow ? stat(path, &st) : lstat(path, &st);
    *kind = CBAT__FS_NONE;
    if (rc != 0) {
        return cbat_error_from_errno(errno);
    }
    if (S_ISLNK(st.st_mode)) {
        *kind = CBAT__FS_LINK;
    } else if (S_ISDIR(st.st_mode)) {
        *kind = CBAT__FS_DIR;
    } else if (S_ISREG(st.st_mode)) {
        *kind = CBAT__FS_FILE;
    } else {
        *kind = CBAT__FS_OTHER;
    }
    if (size != NULL) {
        *size = (uint64_t)st.st_size;
    }
    return CBAT_OK;
}

static CBatError cbat__fs_mkdir(const char *path) {
    return mkdir(path, 0777) == 0 ? CBAT_OK : cbat_error_from_errno(errno);
}

static CBatError cbat__fs_unlink(const char *path) {
    return unlink(path) == 0 ? CBAT_OK : cbat_error_from_errno(errno);
}

static CBatError cbat__fs_rmdir(const char *path) {
    if (rmdir(path) == 0) {
        return CBAT_OK;
    }
    /* POSIX allows EEXIST as well as ENOTEMPTY for a non-empty directory. */
    return errno == EEXIST ? CBAT_ERR_NOT_EMPTY : cbat_error_from_errno(errno);
}

static CBatError cbat__fs_remove_link(const char *path) {
    return cbat__fs_unlink(path);
}

CBatError cbat_fs_rename(const char *from, const char *to) {
    if (from == NULL || to == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return rename(from, to) == 0 ? CBAT_OK : cbat_error_from_errno(errno);
}

static CBatError cbat__fs_read_dir(const char *path, CBatList *names) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    CBatError err = CBAT_OK;
    if (dir == NULL) {
        return cbat_error_from_errno(errno);
    }
    for (;;) {
        CBatString *name;
        errno = 0;
        entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) {
                err = cbat_error_from_errno(errno);
            }
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        name = cbat_string_from(entry->d_name);
        if (name == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            break;
        }
        err = cbat_list_push(names, &name);
        if (err != CBAT_OK) {
            cbat_string_free(name);
            break;
        }
    }
    closedir(dir);
    return err;
}

CBatError cbat_fs_temp_dir(CBatString **out) {
    const char *dir = getenv("TMPDIR");
    CBatString *s;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (dir == NULL || dir[0] == '\0') {
        dir = "/tmp";
    }
    s = cbat_string_from(dir);
    if (s == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    while (s->length > 1 && s->data[s->length - 1] == '/') {
        cbat_string_truncate(s, s->length - 1);
    }
    *out = s;
    return CBAT_OK;
}

static unsigned long cbat__fs_unique_seed(void) {
    return (unsigned long)getpid() ^ (unsigned long)time(NULL) ^ (unsigned long)clock();
}

CBatError cbat_fs_current_dir(CBatString **out) {
    size_t capacity = 256;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    for (;;) {
        CBatString *s = cbat_string_new();
        if (s == NULL || cbat_string_reserve(s, capacity) != CBAT_OK) {
            cbat_string_free(s);
            return CBAT_ERR_NO_MEMORY;
        }
        if (getcwd(s->data, s->capacity) != NULL) {
            s->length = strlen(s->data);
            *out = s;
            return CBAT_OK;
        }
        cbat_string_free(s);
        if (errno != ERANGE || capacity > 1024 * 1024) {
            return cbat_error_from_errno(errno);
        }
        capacity *= 2;
    }
}

#endif /* _WIN32 */

/* ==== Portable layer ===================================================== */

bool cbat_fs_exists(const char *path) {
    cbat__fs_kind kind;
    return path != NULL && cbat__fs_kind_of(path, true, &kind, NULL) == CBAT_OK;
}

bool cbat_fs_is_file(const char *path) {
    cbat__fs_kind kind;
    return path != NULL && cbat__fs_kind_of(path, true, &kind, NULL) == CBAT_OK &&
           kind == CBAT__FS_FILE;
}

bool cbat_fs_is_dir(const char *path) {
    cbat__fs_kind kind;
    return path != NULL && cbat__fs_kind_of(path, true, &kind, NULL) == CBAT_OK &&
           kind == CBAT__FS_DIR;
}

CBatError cbat_fs_file_size(const char *path, uint64_t *size) {
    cbat__fs_kind kind;
    uint64_t bytes = 0;
    CBatError err;
    if (path == NULL || size == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__fs_kind_of(path, true, &kind, &bytes);
    if (err != CBAT_OK) {
        return err;
    }
    if (kind == CBAT__FS_DIR) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    *size = bytes;
    return CBAT_OK;
}

CBatError cbat_fs_make_dir(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat__fs_mkdir(path);
}

CBatError cbat_fs_make_dirs(const char *path) {
    CBatString *work;
    size_t i;
    size_t start = 0;
    CBatError err = CBAT_OK;
    if (path == NULL || path[0] == '\0') {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    work = cbat_string_from(path);
    if (work == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    /* Skip the root ("/", "C:\") so we never try to create it. */
    if (work->length >= 2 && work->data[1] == ':') {
        start = 2;
    }
    while (start < work->length && cbat_path_is_separator(work->data[start])) {
        start++;
    }
    for (i = start; i <= work->length && err == CBAT_OK; i++) {
        char saved;
        if (i < work->length && !cbat_path_is_separator(work->data[i])) {
            continue;
        }
        if (i > 0 && cbat_path_is_separator(work->data[i - 1])) {
            continue; /* repeated separator */
        }
        saved = work->data[i];
        work->data[i] = '\0';
        err = cbat__fs_mkdir(work->data);
        if (err == CBAT_ERR_ALREADY_EXISTS || err == CBAT_ERR_PERMISSION) {
            /* Fine if a directory is already there (permission errors are
               common for existing parents such as /home on locked systems). */
            err = cbat_fs_is_dir(work->data) ? CBAT_OK : err;
        }
        work->data[i] = saved;
    }
    cbat_string_free(work);
    return err;
}

CBatError cbat_fs_remove_file(const char *path) {
    if (path == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat__fs_unlink(path);
}

CBatError cbat_fs_remove_dir(const char *path) {
    if (path == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat__fs_rmdir(path);
}

CBatError cbat_fs_list_dir(const char *path, CBatList **out) {
    CBatList *names;
    CBatError err;
    if (path == NULL || out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    names = cbat_list_new(sizeof(CBatString *));
    if (names == NULL) {
        return CBAT_ERR_NO_MEMORY;
    }
    cbat_list_set_destructor(names, cbat_string_destructor);
    err = cbat__fs_read_dir(path, names);
    if (err == CBAT_OK) {
        err = cbat_list_sort(names, cbat_string_compare_elements);
    }
    if (err != CBAT_OK) {
        cbat_list_free(names);
        return err;
    }
    *out = names;
    return CBAT_OK;
}

CBatError cbat_fs_remove_tree(const char *path) {
    cbat__fs_kind kind;
    CBatList *names = NULL;
    CBatError err;
    size_t i;
    if (path == NULL || path[0] == '\0') {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    err = cbat__fs_kind_of(path, false, &kind, NULL);
    if (err != CBAT_OK) {
        return err;
    }
    if (kind == CBAT__FS_LINK) {
        return cbat__fs_remove_link(path);
    }
    if (kind != CBAT__FS_DIR) {
        return cbat__fs_unlink(path);
    }
    err = cbat_fs_list_dir(path, &names);
    if (err != CBAT_OK) {
        return err;
    }
    for (i = 0; i < names->length && err == CBAT_OK; i++) {
        const CBatString *name = CBAT_LIST_GET(names, CBatString *, i);
        CBatString *child = cbat_path_join(path, cbat_string_cstr(name));
        if (child == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            break;
        }
        err = cbat_fs_remove_tree(cbat_string_cstr(child));
        cbat_string_free(child);
    }
    cbat_list_free(names);
    if (err != CBAT_OK) {
        return err;
    }
    return cbat__fs_rmdir(path);
}

static CBatError cbat__fs_walk(const char *dir, CBatWalkFunc visit, void *user_data,
                               bool *stopped) {
    CBatList *names = NULL;
    CBatError err = cbat_fs_list_dir(dir, &names);
    size_t i;
    if (err != CBAT_OK) {
        return err;
    }
    for (i = 0; i < names->length && err == CBAT_OK && !*stopped; i++) {
        const CBatString *name = CBAT_LIST_GET(names, CBatString *, i);
        CBatString *child = cbat_path_join(dir, cbat_string_cstr(name));
        cbat__fs_kind kind;
        if (child == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            break;
        }
        err = cbat__fs_kind_of(cbat_string_cstr(child), false, &kind, NULL);
        if (err == CBAT_OK) {
            bool is_dir = kind == CBAT__FS_DIR;
            if (!visit(cbat_string_cstr(child), is_dir, user_data)) {
                *stopped = true;
            } else if (is_dir) {
                err = cbat__fs_walk(cbat_string_cstr(child), visit, user_data, stopped);
            }
        }
        cbat_string_free(child);
    }
    cbat_list_free(names);
    return err;
}

CBatError cbat_fs_walk(const char *root, CBatWalkFunc visit, void *user_data) {
    bool stopped = false;
    if (root == NULL || visit == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    return cbat__fs_walk(root, visit, user_data, &stopped);
}

CBatError cbat_fs_make_temp_dir(const char *prefix, CBatString **out) {
    static unsigned long counter = 0;
    CBatString *base = NULL;
    CBatError err;
    int attempt;
    if (out == NULL) {
        return CBAT_ERR_INVALID_ARGUMENT;
    }
    if (prefix == NULL) {
        prefix = "cbat-";
    }
    err = cbat_fs_temp_dir(&base);
    if (err != CBAT_OK) {
        return err;
    }
    for (attempt = 0; attempt < 100; attempt++) {
        CBatString *candidate;
        unsigned long tag = cbat__fs_unique_seed() ^ (++counter * 2654435761u);
        candidate = cbat_string_from_format("%s/%s%08lx", cbat_string_cstr(base), prefix,
                                            tag & 0xFFFFFFFFul);
        if (candidate == NULL) {
            err = CBAT_ERR_NO_MEMORY;
            break;
        }
        err = cbat__fs_mkdir(cbat_string_cstr(candidate));
        if (err == CBAT_OK) {
            cbat_string_free(base);
            *out = candidate;
            return CBAT_OK;
        }
        cbat_string_free(candidate);
        if (err != CBAT_ERR_ALREADY_EXISTS) {
            break;
        }
    }
    cbat_string_free(base);
    return err;
}
