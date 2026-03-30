## Summary

- what changed
- why it changed
- risk or compatibility notes

## Validation

- [ ] `cmake -S . -B build`
- [ ] `cmake --build build --config Debug`
- [ ] `ctest --test-dir build -C Debug --output-on-failure`

## Checklist

- [ ] Tests were added or updated where needed
- [ ] Public docs were updated if behavior changed
- [ ] Changelog was updated when appropriate
