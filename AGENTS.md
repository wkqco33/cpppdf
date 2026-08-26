# Development Guide

## Workflow

- Read the public API and relevant tests before changing implementation code.
- Follow test-driven development: add a failing regression test first, implement the smallest fix, then run the full test suite.
- Keep parser, document, extractor, converter, and renderer responsibilities separate.
- Treat PDF input as untrusted. Validate sizes, offsets, references, and recursion before allocation or traversal.

## Build and Test

```bash
cmake --preset debug-native
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Use repository fixtures only when they are synthetic or anonymized. A missing required fixture is a test failure, not a skip.

## C++ and Documentation

- Use C++17, 4-space indentation, and the checked-in `.clang-format` configuration.
- Run `clang-format` on changed C++ files and keep warnings enabled.
- Prefer clear names and small functions over explanatory comments. Comments should document PDF rules or non-obvious safety constraints only.
- Update README and public API comments when behavior or supported input changes.
- Never commit credentials, personal documents, generated coverage files, or build output.

## Change Review

- Add tests for normal behavior, malformed input, boundary values, and public API behavior where applicable.
- Run formatting, build, and CTest before submitting a change.
- Do not rewrite Git history or force-push without explicit maintainer approval.
