# cbat design conventions

This document records *how* cbat code is written. Read it before adding a module.

## Files

```
include/cbat/<module>.h   public header (one per module)
src/<module>.c            implementation
tests/test_<module>.c     unit tests (cbat/test.h)
cookbook/NN-name.c        runnable recipes
tools/amalgamate.py       builds dist/cbat.h
```

`include/cbat/cbat.h` includes every tier-1 header.

## Naming

| Kind | Style | Example |
|---|---|---|
| Type | `CBat` + PascalCase | `CBatString`, `CBatJsonType` |
| Function | `cbat_` + module + verb | `cbat_string_append` |
| Enum value / macro | `CBAT_` + UPPER_SNAKE | `CBAT_ERR_NOT_FOUND`, `CBAT_LIST_PUSH` |
| Internal (not API) | `cbat__` double underscore | `cbat__grow` |

## Errors

```c
CBatError err = cbat_list_insert(list, 3, &value);
if (err != CBAT_OK) {
    fprintf(stderr, "insert failed: %s\n", cbat_error_str(err));
}
```

- Fallible operations return `CBatError`; results come back through pointer
  out-parameters, which are left untouched on failure unless documented.
- `*_new` constructors return `NULL` only when memory runs out.
- Lookups that can legitimately miss (`cbat_dict_get`) return `NULL` rather
  than an error, because a missing key is a normal outcome, not a failure.
- `*_free(NULL)` is always a no-op.

## Memory

- `CBatAllocator` is a small vtable (`alloc`, `resize`, `release`, `ctx`).
- Passing `NULL` wherever an allocator is accepted means "the default
  allocator" (`malloc`-backed unless replaced via `cbat_set_default_allocator`).
- Containers remember the allocator they were created with and use it for
  their whole life.
- `CBatArena` hands out memory from large blocks; `cbat_arena_allocator()`
  turns it into a `CBatAllocator` whose `release` is a no-op, so containers can
  live inside an arena and be discarded with one `cbat_arena_free()`.
- Containers can own their elements through an optional *element destructor*
  (`cbat_list_set_destructor`). Removing an element *into an out-parameter*
  transfers ownership to the caller and does not run the destructor.

## Containers

- Type-erased: `cbat_list_new(sizeof(int))`. Elements are copied in by value
  from a pointer: `cbat_list_push(list, &x)`.
- Typed macros wrap the pointer dance with C99 compound literals:
  `CBAT_LIST_PUSH(list, int, 42)`, `CBAT_LIST_GET(list, int, 0)`.
- `CBatDict` and `CBatSet` use UTF-8 string keys (copied and owned) and
  preserve insertion order, like Python's `dict`.

## Portability notes

- No VLAs (MSVC does not support them).
- POSIX-only calls live behind `#ifdef _WIN32` in `fs.c`/`time.c` and those
  files define `_POSIX_C_SOURCE 200809L` before any include.
- Never rely on `long` being 64-bit; use `<stdint.h>` types.

## Thread safety

cbat objects do not provide internal locking. Give each thread its own
mutable objects, or synchronize all access to a shared object whenever any
thread may modify or free it. Borrowed pointers must not outlive their object
or be used concurrently with operations that invalidate them. Read-only
access to immutable objects is safe when their lifetime is synchronized.

Configure the default allocator and logging before starting worker threads;
do not change global configuration while workers use cbat. Allocator/context
objects must outlive their users, and custom allocators shared by threads must
provide their own synchronization. Arenas are mutable and require the same
protection as containers.

Random functions given NULL mutate a global generator, including during lazy
seeding. Initialize it before threads start and serialize **every** use, or
prefer a separate `CBatRandom` per thread seeded with `cbat_random_seed` or
`cbat_random_seed_stream`. `cbat_random_seed_from_time` also uses a shared
counter even with an explicit generator: call it during single-threaded
setup or protect it with a process-wide lock.

Logging configuration is global. A custom log handler, its user data, and
its output stream must tolerate concurrent calls or be protected by the
caller. Avoid configuration changes from handlers. The native test runner
uses global allocation-tracking/failure-injection state: run it serially,
without concurrent calls into cbat from other threads.

On Windows, call `cbat_time_monotonic` once during single-threaded startup
before sharing it across threads (it caches the performance-counter frequency
on first use). Do not change process locale or timezone concurrently with
locale/timezone-sensitive calls.

## Tests

```c
#include "cbat/test.h"

CBAT_TEST(test_push_and_get) {
    CBatList *list = cbat_list_new(sizeof(int));
    CBAT_REQUIRE(list != NULL);
    CBAT_CHECK_OK(CBAT_LIST_PUSH(list, int, 7));
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 0), 7);
    cbat_list_free(list);
}

static const CBatTestCase tests[] = {
    CBAT_TEST_ENTRY(test_push_and_get),
};
CBAT_TEST_MAIN("list", tests)
```

The runner swaps in a counting allocator around each test: any allocation not
released by the end of a test fails it. `cbat_test_fail_allocations_after(n)`
makes the (n+1)-th allocation fail so OOM paths can be tested in a loop.
