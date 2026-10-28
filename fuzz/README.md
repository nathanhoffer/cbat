# Parser fuzzing

Use upstream Clang with libFuzzer, AddressSanitizer, and UBSan installed:

```sh
cmake -S . -B build/fuzz -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DCBAT_FUZZ=ON -DCBAT_WERROR=ON
cmake --build build/fuzz --parallel
mkdir -p /tmp/cbat-fuzz-findings
build/fuzz/fuzz_json fuzz/corpus/json -max_total_time=60 -max_len=4096 -artifact_prefix=/tmp/cbat-fuzz-findings/
```

Targets: `fuzz_json`, `fuzz_csv`, `fuzz_datetime`, `fuzz_base64`, `fuzz_utf8`.
Replace both target and corpus name when running the other targets. Corpora
are starting examples, including malformed inputs. The fuzzer adds interesting
inputs to its corpus directory; use a scratch copy to keep your checkout clean.

Replay a failure by passing its artifact path instead of a corpus directory.
Report the input, target, compiler version and sanitizer output. Add a normal
unit-test regression when fixing a bug. Fuzz targets check parse/format and
encode/decode invariants as well as memory safety. They intentionally abort
on invariant failure. CI runs every target for 60 seconds and uploads findings
and logs if a target fails.
