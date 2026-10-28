# Changelog

All notable changes to this project are documented here, following
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions use [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- File-backed line readers scan buffered chunks instead of reading one byte
  at a time. Readers over borrowed streams preserve interactive behavior.
- Line reads interrupted by allocation failure can be retried without losing
  the partial line. I/O errors are terminal for the reader.

### Added

- A native example for writing tests, allocation failure injection, filtering,
  and leak detection under `docs/examples/`.

## [0.1.0] - Unreleased

Initial version; the release tag has not yet been published.

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

[Unreleased]: https://github.com/nathanhoffer/cbat/commits/main
[0.1.0]: https://github.com/nathanhoffer/cbat/tree/main
