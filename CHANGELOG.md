# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog and this project follows Semantic
Versioning.

## [Unreleased]

### Added

- Public single-header API in `include/microcodec.h`
- RLE codec with encode/decode and tests
- Varint and zig-zag integer codec with array helpers and tests
- Delta codec for `int32_t`, `uint32_t`, and `float` sensor arrays
- LZSS codec with caller-provided window context
- Static and adaptive Huffman coding
- Dispatch API: `mc_encode`, `mc_decode`, `mc_max_encoded_size`, `mc_alg_name`
- CMake build system and automated test targets
- CI, release workflow, contribution guides, security policy, and docs

## [1.0.0] - 2026-03-30

### Added

- Initial public release of `microcodec`
- Five focused compression algorithms for embedded sensor data
- Zero-allocation, zero-dependency C99 implementation
- Test suite covering unit and integration scenarios
