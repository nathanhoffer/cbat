# cbat: C Batteries Included

[![CI](https://github.com/nathanhoffer/cbat/actions/workflows/ci.yml/badge.svg)](https://github.com/nathanhoffer/cbat/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Python ships with batteries: strings, lists, dicts, JSON, CSV, file and path
tools, hashing, dates, logging, argument parsing, statistics and unit tests.
**cbat** brings the same batteries to C, in one small, portable, thoroughly
tested C99 library with one consistent style.

```c
#include "cbat/cbat.h"

int main(void) {
    CBatList *words = NULL;
    CBatDict *counts = cbat_dict_new(sizeof(int));
    /* Split the text into words using whitespace. */
    cbat_string_split_whitespace("the cat saw the other cat", &words);

    /* Visit each word produced above; FOREACH does not split text. */
    CBAT_LIST_FOREACH(CBatString *, w, words) {
        const char *word = cbat_string_cstr(*w);
        CBAT_DICT_SET(counts, word, int, CBAT_DICT_GET(counts, word, int, 0) + 1);
    }
    printf("cat appears %d times\n", CBAT_DICT_GET(counts, "cat", int, 0));

    cbat_list_free(words);
    cbat_dict_free(counts);
    return 0;
}
```

cbat is also the standard library of **cbat-app**, a free C playground for
iOS and Android.

## What's inside

See the generated [API reference](docs/API.md) for public types, signatures,
and header documentation.

| Header | Python equivalent | Highlights |
|---|---|---|
| `cbat/base.h` | (none) | `CBatError` codes, pluggable allocators, `CBatArena` |
| `cbat/test.h` | `unittest` | check macros, automatic leak detection, out-of-memory injection |
| `cbat/string.h` | `str` | growable strings, find/replace/split/join/trim, number parsing, UTF-8 |
| `cbat/list.h` | `list` | type-erased dynamic array, typed macros, element destructors |
| `cbat/heap.h` | `heapq` | min-priority queue, O(n) heap construction, smallest/largest selection |
| `cbat/dict.h` | `dict` | insertion-ordered string-keyed hash map |
| `cbat/map.h` | `dict` | fixed-size keys (integers, structs), custom hash/equality, insertion order |
| `cbat/set.h` | `set` | union, intersection, difference, subset |
| `cbat/deque.h` | `collections.deque` | ring buffer with O(1) ends |
| `cbat/sort.h` | `sorted`, `bisect` | stable sort, binary search, comparators |
| `cbat/path.h` | `os.path`, `pathlib` | join, basename, dirname, extension, normalize |
| `cbat/fs.h` | `os`, `shutil`, `tempfile` | mkdir -p, list, walk, rename, rmtree, temp dirs (UTF-8 paths on Windows) |
| `cbat/file.h` | `open()` | read/write whole files, line reader, copy |
| `cbat/json.h` | `json` | DOM, error line/column, JSON Pointer, pretty printing |
| `cbat/csv.h` | `csv` | RFC 4180 reader (in-memory or streaming) and writer |
| `cbat/encoding.h` | `base64`, `binascii` | Base64 (standard and URL-safe), hex |
| `cbat/hash.h` | `hashlib`, `zlib.crc32` | SHA-256, CRC-32, FNV-1a |
| `cbat/random.h` | `random` | PCG32, unbiased ranges, gaussian, shuffle, choice |
| `cbat/math.h` | `math` | gcd/lcm, isqrt, is_prime, is_close, checked int64 |
| `cbat/stats.h` | `statistics` | mean, median, percentiles, stdev, correlation, regression |
| `cbat/time.h` | `time`, `datetime` | clocks, sleep, `CBatDateTime`, ISO 8601, strftime |
| `cbat/log.h` | `logging` | levels, handlers, timestamps |
| `cbat/args.h` | `argparse` | flags, options, positionals, generated `--help` |

## The rules every module follows

- **Naming:** types are `CBatThing`, functions are `cbat_thing_verb()`,
  macros are `CBAT_THING`.
- **Errors are values.** Anything that can fail returns `CBatError`
  (`CBAT_OK == 0`), and `cbat_error_str()` explains it. Constructors return
  `NULL` only when memory runs out. Parsers report line and column through
  `CBatParseInfo`. cbat never aborts or exits on bad input.
- **Explicit ownership.** Every `*_new` has a `*_free` that accepts `NULL`.
  For many short-lived objects, create them inside a `CBatArena` and free them
  all at once.
- **Portable C99.** It builds warning-free with GCC, Clang, MSVC, Apple Clang
  (iOS) and the Android NDK.

## Building

```bash
cmake -S . -B build
```

```bash
cmake --build build
```

```bash
ctest --test-dir build --output-on-failure
```

Options: `-DCBAT_WERROR=ON`, `-DCBAT_SANITIZE=ON` (ASan+UBSan),
`-DBUILD_SHARED_LIBS=ON`, `-DCBAT_BUILD_TESTS=OFF`, `-DCBAT_BUILD_COOKBOOK=OFF`.

## Install

Build and install to a prefix you own (no administrator access required):

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/path/to/cbat-install
cmake --build build --config Release
cmake --install build --config Release
```

In a consuming CMake project, configure with
`-DCMAKE_PREFIX_PATH=/path/to/cbat-install`, then use:

```cmake
find_package(cbat 0.1 CONFIG REQUIRED)
add_executable(my_program main.c)
target_link_libraries(my_program PRIVATE cbat::cbat)
```

Alternatively, vendor this repository alongside your sources:

```cmake
add_subdirectory(external/cbat)
add_executable(my_program main.c)
target_link_libraries(my_program PRIVATE cbat::cbat)
```

Include `cbat/cbat.h` for all modules, or individual headers such as
`cbat/json.h`. Use the same compiler/runtime configuration for the library
and your application. With a shared build on Windows, put the installed DLL
beside your executable or on `PATH`.

**Single header:** `python tools/amalgamate.py` writes `dist/cbat.h`. In
exactly one `.c` file, write:

```c
#define CBAT_IMPLEMENTATION
#include "cbat.h"
```

Include `cbat.h` normally in every other translation unit. Put the
implementation define before any system headers; compile as C99 and link
`libm` on platforms that require it (for example, `cc main.c -lm`).

## Cookbook

[`cookbook/`](cookbook) holds task-focused recipes in the spirit of the
*Python Cookbook*. Each one states a problem, gives a solution and discusses
it. Every recipe is compiled and run by the test suite. Topics include word
frequencies, JSON config with defaults, CSV reports, finding files, error
handling, arenas, CLI tools, dates, hashing, dice simulations, BFS on a grid,
and sets with logging.

## Writing tests

```c
#include "cbat/test.h"

CBAT_TEST(test_push) {
    CBatList *list = cbat_list_new(sizeof(int));
    CBAT_REQUIRE_NOT_NULL(list);
    CBAT_CHECK_OK(CBAT_LIST_PUSH(list, int, 7));
    CBAT_CHECK_EQ_INT(CBAT_LIST_GET(list, int, 0), 7);
    cbat_list_free(list);   /* forget this and the test fails with "leaked" */
}

static const CBatTestCase tests[] = { CBAT_TEST_ENTRY(test_push) };
CBAT_TEST_MAIN("list", tests)
```

## Roadmap

Tier 2, explicitly second-class to the modules above: `ui` (an
immediate-mode UI in the style of egui or microui), `ndarray`/`linalg`
(numpy-like), `ml` (scikit-like basics), `regex`, `bigint` and compression.
See [PROMPT.md](PROMPT.md) and [docs/DESIGN.md](docs/DESIGN.md).

Changes are recorded in [CHANGELOG.md](CHANGELOG.md). Build and contribution
instructions are in [CONTRIBUTING.md](CONTRIBUTING.md).

## License

MIT. See [LICENSE](LICENSE).
