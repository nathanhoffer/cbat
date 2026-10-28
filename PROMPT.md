# cbat — Master Prompt (library)

> This is the working brief for **cbat** ("C Batteries Included"), the open-source
> half of the project. The closed-source mobile app lives in `../cbat-app` and has
> its own prompt. Update this file whenever a design decision changes.

## Mission

Give C programmers the "batteries" that Python ships with: strings, lists, dicts,
sets, files and paths, JSON/CSV, hashing, random numbers, dates, logging,
argument parsing, statistics, and a unit-test framework, all in one small,
portable, well-tested C99 library with one consistent naming style.

cbat is also the standard library that the free **cbat-app** (a
Pythonista-style C IDE for iOS and Android) exposes to user scripts. Everything
cbat does must therefore be safe to call from an interpreted sandbox: no
global surprises, no aborts on bad input, and every failure reported as a value.

## Non-negotiable rules

1. **C99**, no compiler extensions in public headers. Must build warning-free on
   GCC, Clang, MSVC (2019+), Apple Clang (iOS) and the Android NDK.
2. **Naming:** types `CBatThing`, functions `cbat_thing_verb()`, macros/enums
   `CBAT_THING`. Full words (`cbat_string_append`, not `cbat_str_app`).
3. **Errors:** every operation that can fail for a reason other than running out
   of memory returns `CBatError` (`CBAT_OK == 0`). Constructors (`*_new`) return
   a pointer and return `NULL` *only* on allocation failure. Parsers also fill an
   optional `CBatParseInfo` with line/column/message.
4. **Memory:** explicit ownership. Every `*_new` has a matching `*_free` that
   accepts `NULL`. All allocations go through a `CBatAllocator`; a `CBatArena`
   can back any container so a whole group of objects is freed at once.
   Library code never calls `malloc`/`free` directly.
5. **No aborts / no exit / no asserts on user input.** Invalid arguments return
   `CBAT_ERR_INVALID_ARGUMENT`; out-of-range indices return
   `CBAT_ERR_OUT_OF_RANGE` or `NULL`.
6. **Containers are type-erased** (element size chosen at runtime) with typed
   convenience macros (`CBAT_LIST_PUSH(list, int, 42)`), so a single compiled
   implementation can be bound into the app's VM.
7. **Every public function has unit tests**, written with cbat's own
   `cbat/test.h`. The test runner checks every test for allocation leaks and
   supports failure injection for out-of-memory paths.
8. **Distribution:** CMake (with `find_package(cbat)` support) *and* a generated
   single header `dist/cbat.h` (stb-style, `#define CBAT_IMPLEMENTATION`).
9. **License:** MIT.

## Module tiers

**Tier 1 (v0.1, first-class):**

| Area | Headers |
|---|---|
| Core | `base.h` (errors, allocators, arena, checked math), `test.h` |
| Containers | `string.h` (+UTF-8), `list.h`, `heap.h`, `dict.h`, `map.h`, `set.h`, `deque.h`, `sort.h` |
| Files & OS | `path.h`, `file.h`, `fs.h` |
| Data formats | `json.h`, `csv.h`, `bytes.h` (binary/endian I/O), `url.h`, `ini.h`, `encoding.h` (base64, hex) |
| Utilities | `random.h`, `stats.h` (math/statistics), `time.h`, `log.h`, `args.h`, `hash.h` |

**Tier 2 (later, explicitly second-class):** `ndarray` (numpy-like n-d arrays),
`linalg`, `ml` (scikit-like basics: linear regression, k-means, kNN), `regex`,
`bigint`/`decimal`, compression, and **`ui`**, an egui/microui-style
*immediate-mode* UI toolkit. `ui` is renderer-agnostic: it takes input events
(touch/mouse/keys) and produces a flat list of draw commands (rects, text, clip),
so the app can render it with Flutter's canvas and desktop users with SDL or
similar. This also enables a later on-device UI designer in the app. Tier-2
modules may depend on tier 1, never the reverse.

## Cookbook

`cookbook/` holds task-oriented recipes in the spirit of the *Python Cookbook*.
Each recipe is one `NN-short-name.c` file (a runnable program with a header
comment that states the problem, the solution and a discussion) and is compiled
and run by CTest, so recipes can never rot. The app bundles these.

## Definition of done (per module)

- Public header documented (each function: what, ownership, errors).
- Implementation passes `-Wall -Wextra -Wpedantic` (GCC/Clang) and `/W4` (MSVC)
  with zero warnings.
- Unit tests for every public function, including error paths and at least one
  OOM-injection test for anything that allocates.
- Leak-free under the test runner; clean under ASan/UBSan on Linux/macOS CI.
- Included in the amalgamated header and covered by at least one cookbook
  recipe.
