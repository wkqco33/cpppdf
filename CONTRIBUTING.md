# Contributing

## Before Opening a Change

- Check existing issues and keep changes focused.
- Add or update tests before implementation changes when fixing behavior.
- Do not add real PDFs containing personal or confidential information. Use synthetic fixtures.

## Local Checks

```bash
cmake --preset debug-native
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Format changed C++ files with `clang-format` and review the resulting diff. Keep public headers compatible with C++17.

## Pull Requests

Describe the behavior change, tests run, and any compatibility or performance impact. Security issues must be reported privately as described in `SECURITY.md`.
