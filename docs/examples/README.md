# Writing your own tests

[`writing-tests.c`](writing-tests.c) is a complete native C recipe for testing
an application helper with cbat's test framework. It demonstrates normal
results, invalid input, allocation failure during construction and growth,
and cleanup verified by the runner's allocation tracking.

Build this standalone example project from the library root. It builds cbat
as a dependency with matching compiler and runtime settings:

```powershell
cmake -S docs/examples -B build/testing-recipe
cmake --build build/testing-recipe --config Debug
ctest --test-dir build/testing-recipe -C Debug --output-on-failure
```

The commands also work on Linux and macOS. Run the resulting `writing_tests`
executable directly to see its report, pass `--list` to list cases, or pass
`allocation` to run just the allocation test. Multi-configuration generators
place the executable under `build/testing-recipe/Debug`. A failing check makes
the runner return a nonzero exit status, so it can be used in CI.

`CHECK` records failure and continues. `REQUIRE` returns from the current test
on failure, so use it before acquiring resources or where failure leaves
nothing to release. The recipe uses explicit cleanup after acquisition.

The allocation test allows successively more allocations until the helper
succeeds. Every earlier attempt must report `CBAT_ERR_NO_MEMORY`, preserve
the output pointer, and leave no live allocations. Injection is disabled
before checking results and cleaning up.

This example runs against the native library. The app VM currently does not
bind the test runner (`cbat_test_main`), so this recipe lives here instead of
in the cookbook directory automatically imported into the app.
