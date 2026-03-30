# Release process

## Versioning

Use semantic version tags such as `v1.0.0`.

## Pre-release checklist

```bash
cmake -S . -B build
cmake --build build --config Debug --clean-first
ctest --test-dir build -C Debug --output-on-failure
```

## Documentation checklist

- update `README.md` if public usage changed
- update `CHANGELOG.md`
- confirm workflow files match the intended release process
- review repository metadata and topics

## Tagging

```bash
git add -A
git commit -m "docs: finalize release notes for vX.Y.Z"
git tag vX.Y.Z
git push origin main --tags
```

The `release.yml` workflow will build, test, archive, and publish a GitHub
release for matching tags.
