# Build and test

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The project is designed to build cleanly in CI on Windows, Linux, and macOS.
