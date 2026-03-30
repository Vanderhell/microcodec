# Contributing to microcodec

Thank you for considering a contribution.

## Ground rules

- Keep the library C99-compatible.
- Do not introduce heap allocation into codec implementations.
- Do not add hidden global mutable state.
- Prefer small, reviewable pull requests with focused scope.
- New public API must be documented in `include/microcodec.h`.
- New behavior should come with tests.

## Development setup

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Coding expectations

- Follow the existing naming and file layout.
- Keep public APIs caller-buffer driven.
- Preserve compile-time feature flags where relevant.
- Prefer clear correctness over clever compression tricks.

## Pull request checklist

- Code builds cleanly.
- Relevant tests were added or updated.
- `ctest` passes locally.
- Documentation was updated if public behavior changed.
- Changelog entry was added when appropriate.

## Commit style

Preferred prefixes:

- `feat:`
- `fix:`
- `test:`
- `docs:`
- `chore:`

Examples:

- `feat(lzss): add fixed-window encoder`
- `test(huff): cover corrupt embedded table`
- `docs: expand quick start and release notes`
