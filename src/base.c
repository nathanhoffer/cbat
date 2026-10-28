/*
 * cbat/base.c - errors, allocators and the arena.
 * Part of cbat (C Batteries Included). MIT License.
 */
#include "cbat/base.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* ---- Errors ------------------------------------------------------------- */

static const char *const cbat__error_strings[CBAT_ERROR_COUNT] = {
    "success",
    "out of memory",
    "invalid argument",
    "index or value out of range",
    "key or item not found",
    "key or item already exists",
    "container is empty",
    "arithmetic or size overflow",
    "malformed input",
    "input/output error",
    "permission denied",
    "operation not supported on this platform",
    "invalid UTF-8",
    "value has the wrong type",
    "end of input",
    "directory not empty",
};

static const char *const cbat__error_names[CBAT_ERROR_COUNT] = {
    "CBAT_OK",
    "CBAT_ERR_NO_MEMORY",
    "CBAT_ERR_INVALID_ARGUMENT",
    "CBAT_ERR_OUT_OF_RANGE",
    "CBAT_ERR_NOT_FOUND",
    "CBAT_ERR_ALREADY_EXISTS",
    "CBAT_ERR_EMPTY",
    "CBAT_ERR_OVERFLOW",
    "CBAT_ERR_PARSE",
    "CBAT_ERR_IO",
    "CBAT_ERR_PERMISSION",
    "CBAT_ERR_UNSUPPORTED",
    "CBAT_ERR_INVALID_UTF8",
    "CBAT_ERR_TYPE",
    "CBAT_ERR_END",
    "CBAT_ERR_NOT_EMPTY",
};

const char *cbat_error_str(CBatError err) {
    if ((int)err < 0 || err >= CBAT_ERROR_COUNT) {
        return "unknown error";
    }
    return cbat__error_strings[err];
}

const char *cbat_error_name(CBatError err) {
    if ((int)err < 0 || err >= CBAT_ERROR_COUNT) {
        return "CBAT_ERR_UNKNOWN";
    }
    return cbat__error_names[err];
}

CBatError cbat_error_from_errno(int errno_value) {
    switch (errno_value) {
    case 0:
        return CBAT_OK;
    case ENOENT:
    case ENOTDIR:
        return CBAT_ERR_NOT_FOUND;
    case EACCES:
    case EPERM:
    case EROFS:
        return CBAT_ERR_PERMISSION;
    case EEXIST:
        return CBAT_ERR_ALREADY_EXISTS;
    case ENOTEMPTY:
        return CBAT_ERR_NOT_EMPTY;
    case ENOMEM:
        return CBAT_ERR_NO_MEMORY;
    case EINVAL:
    case ENAMETOOLONG:
        return CBAT_ERR_INVALID_ARGUMENT;
    default:
        return CBAT_ERR_IO;
    }
}

/* ---- Heap allocator ----------------------------------------------------- */

static void *cbat__heap_alloc(void *ctx, size_t size) {
    CBAT_UNUSED(ctx);
    return malloc(size);
}

static void *cbat__heap_resize(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    CBAT_UNUSED(ctx);
    CBAT_UNUSED(old_size);
    return realloc(ptr, new_size);
}

static void cbat__heap_release(void *ctx, void *ptr) {
    CBAT_UNUSED(ctx);
    free(ptr);
}

static const CBatAllocator cbat__heap = {
    cbat__heap_alloc, cbat__heap_resize, cbat__heap_release, NULL
};

static const CBatAllocator *cbat__default = &cbat__heap;

const CBatAllocator *cbat_heap_allocator(void) {
    return &cbat__heap;
}

const CBatAllocator *cbat_default_allocator(void) {
    return cbat__default;
}

void cbat_set_default_allocator(const CBatAllocator *allocator) {
    cbat__default = allocator != NULL ? allocator : &cbat__heap;
}

void *cbat_alloc(const CBatAllocator *allocator, size_t size) {
    if (allocator == NULL) {
        allocator = cbat__default;
    }
    return allocator->alloc(allocator->ctx, size == 0 ? 1 : size);
}

void *cbat_alloc_zeroed(const CBatAllocator *allocator, size_t size) {
    void *ptr = cbat_alloc(allocator, size);
    if (ptr != NULL) {
        memset(ptr, 0, size == 0 ? 1 : size);
    }
    return ptr;
}

void *cbat_resize(const CBatAllocator *allocator, void *ptr, size_t old_size, size_t new_size) {
    if (allocator == NULL) {
        allocator = cbat__default;
    }
    if (ptr == NULL) {
        return cbat_alloc(allocator, new_size);
    }
    return allocator->resize(allocator->ctx, ptr, old_size, new_size == 0 ? 1 : new_size);
}

void cbat_release(const CBatAllocator *allocator, void *ptr) {
    if (ptr == NULL) {
        return;
    }
    if (allocator == NULL) {
        allocator = cbat__default;
    }
    allocator->release(allocator->ctx, ptr);
}

void cbat_free(void *ptr) {
    cbat_release(NULL, ptr);
}

void cbat_destroy_pointer(void *element) {
    if (element != NULL) {
        cbat_free(*(void **)element);
    }
}

char *cbat_strndup(const CBatAllocator *allocator, const char *s, size_t n) {
    size_t len = 0;
    char *copy;
    if (s == NULL) {
        return NULL;
    }
    while (len < n && s[len] != '\0') {
        len++;
    }
    copy = (char *)cbat_alloc(allocator, len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

char *cbat_strdup(const CBatAllocator *allocator, const char *s) {
    if (s == NULL) {
        return NULL;
    }
    return cbat_strndup(allocator, s, strlen(s));
}

/* ---- Checked arithmetic ------------------------------------------------- */

bool cbat_size_add(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b) {
        return false;
    }
    *out = a + b;
    return true;
}

bool cbat_size_mul(size_t a, size_t b, size_t *out) {
    if (a != 0 && b > SIZE_MAX / a) {
        return false;
    }
    *out = a * b;
    return true;
}

/* ---- Arena -------------------------------------------------------------- */

/* Strictest fundamental alignment, computed portably in C99. */
typedef union cbat__max_align {
    long double ld;
    long long ll;
    double d;
    void *p;
    void (*fp)(void);
} cbat__max_align;

struct cbat__align_probe {
    char c;
    cbat__max_align u;
};

#define CBAT__ALIGN offsetof(struct cbat__align_probe, u)
#define CBAT__DEFAULT_BLOCK ((size_t)64 * 1024)

typedef struct cbat__arena_block {
    struct cbat__arena_block *next;
    size_t capacity; /* usable bytes after the header */
    size_t used;
} cbat__arena_block;

/* Header size rounded up so block data starts aligned. */
#define CBAT__BLOCK_HEADER \
    ((sizeof(cbat__arena_block) + CBAT__ALIGN - 1) / CBAT__ALIGN * CBAT__ALIGN)

struct CBatArena {
    CBatAllocator view;          /* returned by cbat_arena_allocator() */
    const CBatAllocator *backing;
    cbat__arena_block *head;     /* current block (most recent first) */
    size_t block_size;
    size_t bytes_used;
    void *last_ptr;              /* most recent allocation, for in-place resize */
    size_t last_size;
};

static unsigned char *cbat__block_data(cbat__arena_block *block) {
    return (unsigned char *)block + CBAT__BLOCK_HEADER;
}

static cbat__arena_block *cbat__arena_add_block(CBatArena *arena, size_t min_size) {
    size_t capacity = CBAT_MAX(arena->block_size, min_size);
    size_t total;
    cbat__arena_block *block;
    if (!cbat_size_add(capacity, CBAT__BLOCK_HEADER, &total)) {
        return NULL;
    }
    block = (cbat__arena_block *)cbat_alloc(arena->backing, total);
    if (block == NULL) {
        return NULL;
    }
    block->next = arena->head;
    block->capacity = capacity;
    block->used = 0;
    arena->head = block;
    return block;
}

void *cbat_arena_alloc(CBatArena *arena, size_t size) {
    cbat__arena_block *block;
    size_t offset;
    size_t needed;
    void *ptr;
    if (arena == NULL) {
        return NULL;
    }
    if (size == 0) {
        size = 1;
    }
    block = arena->head;
    if (block != NULL) {
        offset = (block->used + CBAT__ALIGN - 1) / CBAT__ALIGN * CBAT__ALIGN;
        if (offset <= block->capacity && size <= block->capacity - offset) {
            goto have_space;
        }
    }
    block = cbat__arena_add_block(arena, size);
    if (block == NULL) {
        return NULL;
    }
    offset = 0;
have_space:
    if (!cbat_size_add(offset, size, &needed)) {
        return NULL;
    }
    ptr = cbat__block_data(block) + offset;
    block->used = needed;
    arena->bytes_used += size;
    arena->last_ptr = ptr;
    arena->last_size = size;
    return ptr;
}

static void *cbat__arena_view_alloc(void *ctx, size_t size) {
    return cbat_arena_alloc((CBatArena *)ctx, size);
}

static void *cbat__arena_view_resize(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    CBatArena *arena = (CBatArena *)ctx;
    void *fresh;
    /* Grow or shrink in place when ptr is the most recent allocation. */
    if (ptr == arena->last_ptr && arena->head != NULL) {
        cbat__arena_block *block = arena->head;
        size_t start = (size_t)((unsigned char *)ptr - cbat__block_data(block));
        if (new_size <= block->capacity - start) {
            block->used = start + new_size;
            if (new_size >= arena->last_size) {
                arena->bytes_used += new_size - arena->last_size;
            } else {
                arena->bytes_used -= arena->last_size - new_size;
            }
            arena->last_size = new_size;
            return ptr;
        }
    }
    fresh = cbat_arena_alloc(arena, new_size);
    if (fresh != NULL) {
        memcpy(fresh, ptr, CBAT_MIN(old_size, new_size));
    }
    return fresh;
}

static void cbat__arena_view_release(void *ctx, void *ptr) {
    /* Arena memory is reclaimed all at once by reset/free. */
    CBAT_UNUSED(ctx);
    CBAT_UNUSED(ptr);
}

CBatArena *cbat_arena_new_with(size_t block_size, const CBatAllocator *backing) {
    CBatArena *arena;
    if (backing == NULL) {
        backing = cbat_default_allocator();
    }
    arena = (CBatArena *)cbat_alloc(backing, sizeof(CBatArena));
    if (arena == NULL) {
        return NULL;
    }
    arena->view.alloc = cbat__arena_view_alloc;
    arena->view.resize = cbat__arena_view_resize;
    arena->view.release = cbat__arena_view_release;
    arena->view.ctx = arena;
    arena->backing = backing;
    arena->head = NULL;
    arena->block_size = block_size == 0 ? CBAT__DEFAULT_BLOCK : block_size;
    arena->bytes_used = 0;
    arena->last_ptr = NULL;
    arena->last_size = 0;
    return arena;
}

CBatArena *cbat_arena_new(size_t block_size) {
    return cbat_arena_new_with(block_size, NULL);
}

void cbat_arena_free(CBatArena *arena) {
    cbat__arena_block *block;
    if (arena == NULL) {
        return;
    }
    block = arena->head;
    while (block != NULL) {
        cbat__arena_block *next = block->next;
        cbat_release(arena->backing, block);
        block = next;
    }
    cbat_release(arena->backing, arena);
}

void cbat_arena_reset(CBatArena *arena) {
    cbat__arena_block *block;
    if (arena == NULL || arena->head == NULL) {
        return;
    }
    /* Keep the oldest block (the tail of the list); free the rest. */
    block = arena->head;
    while (block->next != NULL) {
        cbat__arena_block *next = block->next;
        cbat_release(arena->backing, block);
        block = next;
    }
    block->used = 0;
    arena->head = block;
    arena->bytes_used = 0;
    arena->last_ptr = NULL;
    arena->last_size = 0;
}

char *cbat_arena_strdup(CBatArena *arena, const char *s) {
    size_t len;
    char *copy;
    if (s == NULL) {
        return NULL;
    }
    len = strlen(s);
    copy = (char *)cbat_arena_alloc(arena, len + 1);
    if (copy != NULL) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

size_t cbat_arena_bytes_used(const CBatArena *arena) {
    return arena == NULL ? 0 : arena->bytes_used;
}

const CBatAllocator *cbat_arena_allocator(CBatArena *arena) {
    return arena == NULL ? NULL : &arena->view;
}
