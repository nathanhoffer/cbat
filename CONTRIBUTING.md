# Contributing to cbat

## Build and test

Requirements: a C99 compiler, CMake 3.16+, and Python 3 for generating the
single header. From the repository root:

```bash
cmake -S . -B build -DCBAT_WERROR=ON
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

On Linux or macOS with GCC/Clang, add `-DCBAT_SANITIZE=ON` when configuring
to check for memory errors and undefined behavior. CI additionally compiles
the mobile targets and tests the single-header distribution.

Run one suite with `ctest --test-dir build -C Debug -R '^json$'`.
Individual test executables accept `--list` or a test-name substring.
See [docs/examples/](docs/examples/) for a complete native testing recipe.

## Code conventions

Read [docs/DESIGN.md](docs/DESIGN.md) and [PROMPT.md](PROMPT.md).

- Keep public headers portable C99: no compiler extensions, VLAs, or platform
  types in cross-platform APIs. Use `snprintf` rather than `sprintf`.
- Use `CBatThing` types, `cbat_module_verb` functions, and `CBAT_` macros.
- Return `CBatError` for fallible operations and deliver results through
  output parameters, unchanged on failure unless documented otherwise.
- Allocate through `CBatAllocator`. Pair constructors with NULL-safe frees;
  document ownership and pointer lifetime. Containers accept custom allocators.
- Do not abort, assert, or exit on caller input. Validate arguments and sizes.
- Put a module's header, implementation, and tests in `include/cbat/`, `src/`,
  and `tests/`. Add it in dependency order to `CBAT_MODULES` and `cbat.h`.
- Document every public function. Test normal behavior, invalid input,
  boundaries, allocation failures, and cleanup. The runner detects leaks.
- Update the changelog for user-visible changes and add an executable recipe
  for user-facing modules.

## Generated artifacts and compatibility

`python tools/amalgamate.py` regenerates `dist/cbat.h`. Do not edit generated
output. The amalgamation is built and tested by CMake and CI.

The companion app embeds public headers and generates VM bindings. Header
changes, including comments, require coordination with its maintainer to
regenerate headers/bindings and test its engine and bundled examples. New C
sources also require regenerating iOS forwarders. Avoid struct-by-value
arguments/returns in APIs intended for VM binding. Callbacks may require
manual trampolines in the app.

Recipes in `cookbook/` run natively and inside the app VM: use only bound
functions, without `FILE *` APIs or the native test runner. Native-only
examples belong under `docs/examples/`.

## Before submitting

Keep changes focused, explain the concrete behavior changed, and report the
builds/tests performed. Run with warnings as errors and check both Clang and
MSVC where available; CI covers the other supported platforms. Ensure the
single-header build still works. Do not include generated build directories,
credentials, or unrelated changes.

Contributions are licensed under the repository's MIT license.
