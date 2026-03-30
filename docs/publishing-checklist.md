# Publishing checklist

Use this before pushing the repository public.

## Repository content

- `README.md`
- `LICENSE`
- `CHANGELOG.md`
- `CONTRIBUTING.md`
- `SECURITY.md`
- `CODE_OF_CONDUCT.md`
- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`
- issue and pull request templates
- wiki source pages

## GitHub settings

- set repository description
- set topics from `.github/repository-metadata.md`
- enable Actions
- enable Discussions or Security reporting if desired
- configure branch protection for `main`
- optionally enable GitHub Pages for `docs/`

## Validation

- clean build passes
- test suite passes
- no accidental build artifacts are staged thanks to `.gitignore`
