# Changelog

All notable changes to this project are documented here, following
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions use [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Decimal double parsing accepts the same ASCII syntax across numeric
  locales. JSON shares this parser and now uses shortest round-trip output;
  equivalent numeric JSON text may use different fixed/scientific notation.
- File-backed line readers scan buffered chunks instead of reading one byte
  at a time. Readers over borrowed streams preserve interactive behavior.
- Line reads interrupted by allocation failure can be retried without losing
  the partial line. I/O errors are terminal for the reader.

### Fixed

- A CSV record containing a single empty field is now quoted when written,
  preserving the record when parsed again instead of treating it as a blank line.

### Added

- Percent/form encoding, strict decoding, URI-reference parsing with IPv6
  literals and validated ports, and owned query dictionaries with binary values.
- `CBatBytes` binary buffers and `CBatByteReader` checked cursors, explicit
  little/big-endian integers and IEEE floats, binary file helpers, and a BMP recipe.
- `CBatHeap` min-priority queue, allocation-free push/pop combination,
  linear-time construction, smallest/largest list selection, and a Dijkstra recipe.
- Bounded splitting, line splitting, partition, ASCII predicates, natural
  ordering, padding, prefix/suffix removal, byte-set stripping, and validated
  UTF-8 reversal/code-point slicing. UTF-8 fuzzing checks reverse/slice invariants.
- Locale-independent double/int64 formatting, uint64 parsing, and grouped
  int64 output, with exact binary64 round-trip coverage.
- Dictionary Counter helpers (`increment`, stable `most_common`),
  get-or-insert, shallow copy/update, set copy and symmetric difference.
- `CBatMap` with fixed-size keys/values, integer-key convenience macros,
  insertion-order iteration, custom hashing/equality, and ownership callbacks.
- Opt-in libFuzzer targets for JSON, CSV, ISO dates, Base64 and UTF-8, with
  seed corpora and a Linux CI sanitizer run for each parser.
- A native example for writing tests, allocation failure injection, filtering,
  and leak detection under `docs/examples/`.

## [0.1.0] - 2026-10-27

Initial release.

### Added

- Portable C99 errors, custom allocators, arenas, and a unit-test framework.
- Strings and UTF-8, lists, insertion-ordered dictionaries and sets, deques,
  sorting, and binary search.
- UTF-8 paths, filesystem operations, whole-file I/O, and line readers.
- JSON with JSON Pointer, CSV including streaming input, Base64, and hex.
- SHA-256, CRC-32, FNV-1a, PCG32 random numbers, math, statistics, dates,
  logging, and command-line argument parsing.
- Twelve executable cookbook recipes and tests with OOM/leak checks.
- CMake static/shared builds, installable `cbat::cbat` target, and a generated
  single-header distribution.
- CI builds for Linux, macOS, Windows, iOS, and Android.

[Unreleased]: https://github.com/nathanhoffer/cbat/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/nathanhoffer/cbat/releases/tag/v0.1.0
