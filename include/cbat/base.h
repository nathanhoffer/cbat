/*
 * cbat/base.h - errors, allocators, arenas and small helpers used by every
 * other cbat module.
 *
 * Thread safety: objects have no internal locks. Synchronize mutation and
 * lifetime of shared objects, arenas, and allocator contexts. Configure the
 * default allocator before starting threads; keep it fixed while in use.
 * Custom allocators used across threads must provide their own locking.
 *
 * Part of cbat (C Batteries Included). MIT License.
 */
#ifndef CBAT_BASE_H
#define CBAT_BASE_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBAT_VERSION_MAJOR 0
#define CBAT_VERSION_MINOR 1
#define CBAT_VERSION_PATCH 0
#define CBAT_VERSION_STRING "0.1.0"

/* ---- Export / language glue ------------------------------------------- */

#if defined(CBAT_SHARED)
#  if defined(_WIN32)
#    if defined(CBAT_BUILDING)
#      define CBAT_API __declspec(dllexport)
#    else
#      define CBAT_API __declspec(dllimport)
#    endif
#  elif defined(__GNUC__)
#    define CBAT_API __attribute__((visibility("default")))
#  endif
#endif
#ifndef CBAT_API
#  define CBAT_API
#endif

#ifdef __cplusplus
#  define CBAT_BEGIN_DECLS extern "C" {
#  define CBAT_END_DECLS }
#else
#  define CBAT_BEGIN_DECLS
#  define CBAT_END_DECLS
#endif

/* Lets GCC/Clang type-check printf-style format strings. */
#if defined(__GNUC__) || defined(__clang__)
#  define CBAT_PRINTF_FORMAT(fmt_index, args_index) \
       __attribute__((format(printf, fmt_index, args_index)))
#else
#  define CBAT_PRINTF_FORMAT(fmt_index, args_index)
#endif

/* ---- Small helpers ------------------------------------------------------ */

/** Number of elements in a fixed-size array (not a pointer!). */
#define CBAT_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

/** Silences "unused parameter" warnings. */
#define CBAT_UNUSED(x) ((void)(x))

#define CBAT_MIN(a, b) ((a) < (b) ? (a) : (b))
#define CBAT_MAX(a, b) ((a) > (b) ? (a) : (b))

/** "No position": returned by find/index functions when nothing matches. */
#define CBAT_NPOS ((size_t)-1)

CBAT_BEGIN_DECLS

/**
 * Orders two elements: negative if a < b, zero if equal, positive if a > b.
 * Receives pointers *to the elements* (like qsort).
 */
typedef int (*CBatCompare)(const void *a, const void *b);

/**
 * Releases whatever an element owns. Receives a pointer *to the element slot*,
 * e.g. a `char **` for a list of strings.
 */
typedef void (*CBatDestructor)(void *element);

/** A CBatDestructor for slots holding a pointer from the default allocator. */
CBAT_API void cbat_destroy_pointer(void *element);

/* ---- Errors ------------------------------------------------------------- */

/**
 * Result of any cbat operation that can fail. CBAT_OK is zero, so
 * `if (err) { ... }` reads naturally.
 */
typedef enum CBatError {
    CBAT_OK = 0,
    CBAT_ERR_NO_MEMORY,        /* an allocation failed */
    CBAT_ERR_INVALID_ARGUMENT, /* NULL where not allowed, bad flag, ... */
    CBAT_ERR_OUT_OF_RANGE,     /* index or value outside the valid range */
    CBAT_ERR_NOT_FOUND,        /* key, file or item does not exist */
    CBAT_ERR_ALREADY_EXISTS,   /* key or file already exists */
    CBAT_ERR_EMPTY,            /* pop/peek on an empty container */
    CBAT_ERR_OVERFLOW,         /* arithmetic or size overflow */
    CBAT_ERR_PARSE,            /* malformed input text (see CBatParseInfo) */
    CBAT_ERR_IO,               /* operating-system I/O failure */
    CBAT_ERR_PERMISSION,       /* the OS refused access */
    CBAT_ERR_UNSUPPORTED,      /* not available on this platform */
    CBAT_ERR_INVALID_UTF8,     /* text is not valid UTF-8 */
    CBAT_ERR_TYPE,             /* value has the wrong type (e.g. JSON) */
    CBAT_ERR_END,              /* end of input reached (not a failure) */
    CBAT_ERR_NOT_EMPTY,        /* directory is not empty */
    CBAT_ERROR_COUNT
} CBatError;

/** Human-readable description, e.g. "key or item not found". Never NULL. */
CBAT_API const char *cbat_error_str(CBatError err);

/** Enumerator name, e.g. "CBAT_ERR_NOT_FOUND". Never NULL. */
CBAT_API const char *cbat_error_name(CBatError err);

/**
 * Maps a C `errno` value to the closest CBatError (ENOENT -> NOT_FOUND,
 * EACCES/EPERM -> PERMISSION, EEXIST -> ALREADY_EXISTS, ENOTEMPTY -> NOT_EMPTY,
 * ENOMEM -> NO_MEMORY,
 * anything else -> IO). 0 maps to CBAT_OK.
 */
CBAT_API CBatError cbat_error_from_errno(int errno_value);

/** Where and why a parser rejected its input. Lines and columns are 1-based. */
typedef struct CBatParseInfo {
    size_t line;
    size_t column;      /* in bytes */
    size_t offset;      /* 0-based byte offset into the input */
    const char *message; /* static string, never freed */
} CBatParseInfo;

/* ---- Allocators --------------------------------------------------------- */

/**
 * A pluggable memory allocator. `resize` receives the old size so that
 * allocators which cannot reallocate in place (arenas) can copy.
 * `release` may be NULL-safe; cbat never passes NULL to it.
 */
typedef struct CBatAllocator {
    void *(*alloc)(void *ctx, size_t size);
    void *(*resize)(void *ctx, void *ptr, size_t old_size, size_t new_size);
    void (*release)(void *ctx, void *ptr);
    void *ctx;
} CBatAllocator;

/** The plain malloc/realloc/free allocator. */
CBAT_API const CBatAllocator *cbat_heap_allocator(void);

/** The allocator used whenever a cbat function is given NULL. */
CBAT_API const CBatAllocator *cbat_default_allocator(void);

/**
 * Replaces the default allocator (NULL restores the heap allocator).
 * Not thread-safe: call once at startup, before creating cbat objects.
 * The allocator and its context must outlive every object using them.
 */
CBAT_API void cbat_set_default_allocator(const CBatAllocator *allocator);

/** Allocates `size` bytes (a size of 0 is treated as 1). NULL on failure. */
CBAT_API void *cbat_alloc(const CBatAllocator *allocator, size_t size);

/** Like cbat_alloc() but the memory is zero-filled. */
CBAT_API void *cbat_alloc_zeroed(const CBatAllocator *allocator, size_t size);

/**
 * Grows or shrinks a block. `ptr` may be NULL (then it acts like alloc).
 * On failure returns NULL and leaves the original block untouched.
 */
CBAT_API void *cbat_resize(const CBatAllocator *allocator, void *ptr,
                           size_t old_size, size_t new_size);

/** Frees a block from `allocator`. NULL is ignored. */
CBAT_API void cbat_release(const CBatAllocator *allocator, void *ptr);

/** Frees a block from the default allocator (e.g. strings returned by cbat). */
CBAT_API void cbat_free(void *ptr);

/** Copies a NUL-terminated string. NULL if `s` is NULL or memory runs out. */
CBAT_API char *cbat_strdup(const CBatAllocator *allocator, const char *s);

/** Copies at most `n` bytes of `s` and NUL-terminates the copy. */
CBAT_API char *cbat_strndup(const CBatAllocator *allocator, const char *s, size_t n);

/* ---- Checked size arithmetic ------------------------------------------- */

/** Stores a + b in *out and returns true, or returns false on overflow. */
CBAT_API bool cbat_size_add(size_t a, size_t b, size_t *out);

/** Stores a * b in *out and returns true, or returns false on overflow. */
CBAT_API bool cbat_size_mul(size_t a, size_t b, size_t *out);

/* ---- Arena -------------------------------------------------------------- */

/**
 * A region allocator: many small allocations, one free. Pass
 * cbat_arena_allocator(arena) to any *_new_with() constructor to place a
 * container inside the arena; freeing such a container is then optional.
 */
typedef struct CBatArena CBatArena;

/** Creates an arena that grabs `block_size` bytes at a time (0 = 64 KiB). */
CBAT_API CBatArena *cbat_arena_new(size_t block_size);

/** Like cbat_arena_new() but takes its blocks from `backing`. */
CBAT_API CBatArena *cbat_arena_new_with(size_t block_size, const CBatAllocator *backing);

/** Releases every block and the arena itself. NULL is ignored. */
CBAT_API void cbat_arena_free(CBatArena *arena);

/** Returns suitably aligned memory, or NULL when out of memory. */
CBAT_API void *cbat_arena_alloc(CBatArena *arena, size_t size);

/** Copies a string into the arena. */
CBAT_API char *cbat_arena_strdup(CBatArena *arena, const char *s);

/**
 * Forgets every allocation but keeps the first block for reuse.
 * Pointers handed out before the reset become invalid.
 */
CBAT_API void cbat_arena_reset(CBatArena *arena);

/** Total bytes handed out since creation or the last reset. */
CBAT_API size_t cbat_arena_bytes_used(const CBatArena *arena);

/** An allocator view of the arena, valid for as long as the arena lives. */
CBAT_API const CBatAllocator *cbat_arena_allocator(CBatArena *arena);

CBAT_END_DECLS

#endif /* CBAT_BASE_H */
