# microcodec

> Five compression algorithms for embedded sensor data.
> Zero allocations. Zero dependencies. #include and go.

[![CI](https://github.com/Vanderhell/microcodec/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/microcodec/actions/workflows/ci.yml)
[![Release](https://github.com/Vanderhell/microcodec/actions/workflows/release.yml/badge.svg)](https://github.com/Vanderhell/microcodec/actions/workflows/release.yml)
[![Language: C99](https://img.shields.io/badge/language-C99-blue.svg)](#)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Tests: 140+](https://img.shields.io/badge/tests-140%2B-brightgreen.svg)](#)

## Why microcodec?

`microcodec` is a small C99 compression library focused on the data patterns
that actually show up in embedded and IoT systems:

- repeated bytes and zero-padded frames
- small counters, IDs, and flags
- slowly changing sensor readings
- monotonic timestamps
- compact binary payloads

The library is designed around caller-owned buffers, no heap usage, no hidden
state, and simple APIs that fit microcontroller codebases.

Additional project resources:

- [Changelog](CHANGELOG.md)
- [Contributing guide](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Documentation index](docs/index.md)
- [Wiki source pages](wiki/Home.md)

## Quick start

```c
#include "microcodec.h"

float temperatures[64];
uint8_t encoded[512];
float recovered[64];

mc_buf_t dst = MC_BUF(encoded, sizeof(encoded));

if (mc_delta_encode_f32(temperatures, 64, 1, &dst) == MC_OK) {
    mc_delta_decode_f32(MC_SLICE(encoded, dst.len), recovered, 64);
}
```

For general binary blobs:

```c
mc_lzss_ctx_t ctx;
mc_lzss_ctx_init(&ctx);

uint8_t out[mc_lzss_max_encoded_size(256)];
mc_buf_t dst = MC_BUF(out, sizeof(out));

mc_err_t err = mc_lzss_encode(&ctx, MC_SLICE(data, data_len), &dst);
if (err == MC_ERR_INCOMPRESS) {
    /* store raw data instead */
}
```

## Algorithms

| Algorithm | Best for | Typical ratio | Notes |
|-----------|----------|---------------|-------|
| RLE | Repeated bytes, padding, zeros | 10-50x for uniform data | Simplest, fastest |
| Varint | Small integers, IDs, counters | 2-4x | Exact for small values |
| Delta | Slowly-changing sensor values | 3-8x | Float quantization |
| LZSS | Binary blobs, config data | 2-4x | General purpose |
| Static Huffman | Structured binary data | 1.5-3x | Pre-built table |

## Configuration

All configuration is compile-time and lives in `microcodec.h`.

```c
#define MICROCODEC_ENABLE_RLE    1
#define MICROCODEC_ENABLE_VARINT 1
#define MICROCODEC_ENABLE_DELTA  1
#define MICROCODEC_ENABLE_LZSS   1
#define MICROCODEC_ENABLE_HUFF   1

#define MICROCODEC_LZSS_WINDOW_SIZE   256
#define MICROCODEC_LZSS_MIN_MATCH     3
#define MICROCODEC_LZSS_MAX_MATCH     18

#define MICROCODEC_HUFF_SYMBOLS       256
#define MICROCODEC_HUFF_MAX_CODE_LEN  15

#define MICROCODEC_DELTA_F32_SCALE    100.0f
```

## API reference

Main families:

- `mc_rle_encode` / `mc_rle_decode`
- `mc_varint_encode_*` / `mc_varint_decode_*`
- `mc_delta_encode_*` / `mc_delta_decode_*`
- `mc_lzss_ctx_init`, `mc_lzss_encode`, `mc_lzss_decode`
- `mc_huff_build_table`, `mc_huff_encode_static`, `mc_huff_decode_static`
- `mc_huff_encode_adaptive`, `mc_huff_decode_adaptive`

Dispatch API:

- `mc_encode`
- `mc_decode`
- `mc_max_encoded_size`
- `mc_alg_name`

Current dispatch contract in this repo:

- `MC_ALG_RLE`, `MC_ALG_LZSS`, `MC_ALG_HUFF` operate on raw bytes
- `MC_ALG_VARINT` operates on `uint32_t[]` passed via `src.ptr`
- `MC_ALG_DELTA` operates on `float[]` passed via `src.ptr` with default order `1`

## Integration with microdb

`microcodec` is intentionally separate from `microdb`. A typical pattern is:

```c
float readings[50];
uint8_t compressed[256];

mc_buf_t cbuf = MC_BUF(compressed, sizeof(compressed));
mc_delta_encode_f32(readings, 50, 1, &cbuf);

microdb_kv_put(&db, "sensor_burst", compressed, cbuf.len);
```

On load, read the compressed bytes from `microdb` and decode them back into the
target array with the matching `mc_*_decode` call.

## Design decisions

- No heap allocation anywhere in the codec implementations
- No global mutable state
- All APIs are caller-buffer driven
- Compile-time feature toggles for each algorithm
- Separate focused codecs instead of one opaque auto-tuned compressor

## Part of micro-toolkit

`microcodec` is intended to live alongside small focused embedded libraries such
as `microdb` and other components in the micro-toolkit ecosystem.

## License

MIT
