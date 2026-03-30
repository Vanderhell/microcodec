# microcodec — Compression Library for Embedded Sensor Data

> **C99 · Zero dependencies · Zero allocations · Five algorithms · #include and go**
>
> A collection of focused compression algorithms tuned for the data patterns
> that actually appear in embedded IoT systems: slowly-changing sensor values,
> monotonic timestamps, repeated byte sequences, and small binary payloads.

---

## Table of Contents

1. [Design philosophy](#1-design-philosophy)
2. [Repository layout](#2-repository-layout)
3. [Configuration reference](#3-configuration-reference)
4. [Error codes](#4-error-codes)
5. [Common types](#5-common-types)
6. [Algorithm — RLE](#6-algorithm--rle)
7. [Algorithm — Varint](#7-algorithm--varint)
8. [Algorithm — Delta](#8-algorithm--delta)
9. [Algorithm — LZSS](#9-algorithm--lzss)
10. [Algorithm — Static Huffman](#10-algorithm--static-huffman)
11. [Dispatch API](#11-dispatch-api)
12. [Compile-time limits and defaults](#12-compile-time-limits-and-defaults)
13. [Test requirements](#13-test-requirements)
14. [Usage examples](#14-usage-examples)
15. [Integration with microdb](#15-integration-with-microdb)
16. [What microcodec is NOT](#16-what-microcodec-is-not)

---

## 1. Design philosophy

```
Zero allocations.  Every function works on caller-provided buffers.
Zero dependencies. No libc beyond memcpy/memset/sizeof.
One header.        #include "microcodec.h" — everything is there.
Five algorithms.   Each solves a specific embedded data pattern.
Composable.        Output of one can be input of another.
```

Each algorithm is independent — you can compile only what you use
via `#define MICROCODEC_ENABLE_*` guards.

No algorithm allocates heap. No algorithm uses global state.
Every function is re-entrant and thread-safe by construction.

---

## 2. Repository layout

```
microcodec/
├── include/
│   └── microcodec.h          ← single public header
├── src/
│   ├── microcodec.c          ← dispatch API + algorithm registry
│   ├── mc_rle.c              ← Run-Length Encoding
│   ├── mc_varint.c           ← Zig-zag + varint encoding
│   ├── mc_delta.c            ← Delta + delta-of-delta
│   ├── mc_lzss.c             ← LZSS with fixed window
│   └── mc_huff.c             ← Static Huffman coding
├── tests/
│   ├── test_rle.c
│   ├── test_varint.c
│   ├── test_delta.c
│   ├── test_lzss.c
│   ├── test_huff.c
│   └── test_integration.c
├── CMakeLists.txt
└── README.md
```

---

## 3. Configuration reference

All configuration via `#define` before `#include "microcodec.h"`.

```c
/* Enable/disable individual algorithms. Default: all enabled. */
#define MICROCODEC_ENABLE_RLE    1
#define MICROCODEC_ENABLE_VARINT 1
#define MICROCODEC_ENABLE_DELTA  1
#define MICROCODEC_ENABLE_LZSS   1
#define MICROCODEC_ENABLE_HUFF   1

/* LZSS window size in bytes. Must be power of 2. Default: 256.
 * Larger = better compression, more RAM for the window buffer.
 * Window buffer is caller-provided — see mc_lzss_ctx_t. */
#define MICROCODEC_LZSS_WINDOW_SIZE   256

/* LZSS minimum match length in bytes. Default: 3. */
#define MICROCODEC_LZSS_MIN_MATCH     3

/* LZSS maximum match length in bytes. Default: 18. */
#define MICROCODEC_LZSS_MAX_MATCH     18

/* Huffman symbol count. Default: 256 (full byte alphabet). */
#define MICROCODEC_HUFF_SYMBOLS       256

/* Maximum Huffman code length in bits. Default: 15. */
#define MICROCODEC_HUFF_MAX_CODE_LEN  15
```

---

## 4. Error codes

```c
typedef enum {
    MC_OK              =  0,   /* Success */
    MC_ERR_INVALID     = -1,   /* NULL pointer or bad argument */
    MC_ERR_OVERFLOW    = -2,   /* Output buffer too small */
    MC_ERR_CORRUPT     = -3,   /* Input data is malformed / corrupt */
    MC_ERR_DISABLED    = -4,   /* Algorithm compiled out */
    MC_ERR_INCOMPRESS  = -5,   /* Data not compressible (output > input) */
} mc_err_t;
```

All functions return `mc_err_t`.
Output size is always written via an output pointer argument.
No function returns a pointer the caller must free.

---

## 5. Common types

```c
/* Byte slice — non-owning view into a buffer. */
typedef struct {
    const uint8_t *ptr;   /* Pointer to data */
    size_t         len;   /* Length in bytes */
} mc_slice_t;

/* Mutable output buffer. */
typedef struct {
    uint8_t *ptr;         /* Pointer to output buffer */
    size_t   cap;         /* Capacity in bytes */
    size_t   len;         /* Bytes written (set by encode/decode) */
} mc_buf_t;

/* Algorithm identifier for dispatch API. */
typedef enum {
    MC_ALG_RLE    = 0,
    MC_ALG_VARINT = 1,
    MC_ALG_DELTA  = 2,
    MC_ALG_LZSS   = 3,
    MC_ALG_HUFF   = 4,
} mc_alg_t;

/* Convenience macros */
#define MC_SLICE(ptr, len)   ((mc_slice_t){ (const uint8_t *)(ptr), (len) })
#define MC_BUF(ptr, cap)     ((mc_buf_t){ (uint8_t *)(ptr), (cap), 0 })
```

---

## 6. Algorithm — RLE

### 6.1 What it solves

Repeated byte sequences — common in binary sensor frames, padding,
zero-filled buffers, and raw ADC samples with long flat regions.

### 6.2 Format

```
Encoded stream is a sequence of tokens:
  Literal run:  [ 0x00 | count-1 (7 bits) ] [ count bytes of literal data ]
                count range: 1..128
  Repeat run:   [ 0x80 | count-1 (7 bits) ] [ 1 byte to repeat ]
                count range: 1..128

Maximum expansion: 1 byte of input → 2 bytes of output (literal token + data).
Worst case output size: 2 × input_len bytes.
```

### 6.3 API

```c
/* Encode src into dst using RLE.
 *
 * src     — input slice
 * dst     — output buffer (caller-allocated)
 *           safe minimum: 2 × src.len bytes
 *
 * Returns MC_OK on success, dst.len set to encoded size.
 * Returns MC_ERR_INVALID if src.ptr or dst.ptr is NULL.
 * Returns MC_ERR_OVERFLOW if dst.cap < encoded size. */
mc_err_t mc_rle_encode(mc_slice_t src, mc_buf_t *dst);


/* Decode RLE-encoded src into dst.
 *
 * dst.cap must be >= original uncompressed size.
 * Caller must know the original size (store it separately).
 *
 * Returns MC_OK on success, dst.len set to decoded size.
 * Returns MC_ERR_CORRUPT if stream is malformed.
 * Returns MC_ERR_OVERFLOW if dst.cap is insufficient. */
mc_err_t mc_rle_decode(mc_slice_t src, mc_buf_t *dst);


/* Compute worst-case encoded size for input of len bytes.
 * Use to size the output buffer before encoding. */
static inline size_t mc_rle_max_encoded_size(size_t len) {
    return len * 2u;
}
```

---

## 7. Algorithm — Varint

### 7.1 What it solves

Small integers that rarely reach their full range — register values,
sensor IDs, error counts, status flags. Zig-zag encoding also handles
signed integers efficiently when values cluster near zero.

### 7.2 Format

**Unsigned varint (LEB128):**
```
Each byte uses 7 bits of data and 1 continuation bit (MSB).
MSB=1: more bytes follow. MSB=0: last byte.

Value 0..127:       1 byte
Value 128..16383:   2 bytes
Value 16384..2097151: 3 bytes
Value 2097152..268435455: 4 bytes
Value ≥ 268435456:  5 bytes (uint32_t max)
```

**Zig-zag encoding for signed integers:**
```
zigzag(n) = (n << 1) ^ (n >> 31)   for int32_t
zigzag(n) = (n << 1) ^ (n >> 63)   for int64_t

0  → 0
-1 → 1
1  → 2
-2 → 3
2  → 4
```

### 7.3 API

```c
/* Encode a single uint32_t as varint into dst.
 * Writes 1–5 bytes. dst must have at least 5 bytes capacity.
 *
 * Returns MC_OK, dst.len set to bytes written.
 * Returns MC_ERR_OVERFLOW if dst.cap < 5. */
mc_err_t mc_varint_encode_u32(uint32_t value, mc_buf_t *dst);


/* Decode a varint from src into value.
 * Advances src.ptr and decrements src.len by bytes consumed.
 *
 * Returns MC_OK, *value set to decoded integer.
 * Returns MC_ERR_CORRUPT if stream ends before varint is complete.
 * Returns MC_ERR_INVALID if value is NULL. */
mc_err_t mc_varint_decode_u32(mc_slice_t *src, uint32_t *value);


/* Encode int32_t using zig-zag then varint. */
mc_err_t mc_varint_encode_i32(int32_t value, mc_buf_t *dst);


/* Decode zig-zag varint into int32_t. */
mc_err_t mc_varint_decode_i32(mc_slice_t *src, int32_t *value);


/* Encode an array of uint32_t values as varints.
 * Useful for encoding arrays of sensor readings or timestamps.
 *
 * count   — number of values in src_arr
 * dst     — output buffer
 *
 * Returns MC_OK, dst.len set to total bytes written.
 * Returns MC_ERR_OVERFLOW if dst.cap is insufficient.
 *
 * Safe output size: count × 5 bytes. */
mc_err_t mc_varint_encode_u32_array(const uint32_t *src_arr,
                                     size_t          count,
                                     mc_buf_t       *dst);


/* Decode an array of varints into dst_arr.
 * count — expected number of values to decode.
 *
 * Returns MC_OK, *bytes_consumed set to bytes read from src.
 * Returns MC_ERR_CORRUPT if stream ends early. */
mc_err_t mc_varint_decode_u32_array(mc_slice_t  src,
                                     uint32_t   *dst_arr,
                                     size_t      count,
                                     size_t     *bytes_consumed);


/* Encode int32_t array with zig-zag + varint. */
mc_err_t mc_varint_encode_i32_array(const int32_t *src_arr,
                                     size_t         count,
                                     mc_buf_t      *dst);


/* Decode int32_t array. */
mc_err_t mc_varint_decode_i32_array(mc_slice_t  src,
                                     int32_t    *dst_arr,
                                     size_t      count,
                                     size_t     *bytes_consumed);


/* Compute worst-case encoded size for count uint32_t values. */
static inline size_t mc_varint_max_encoded_size(size_t count) {
    return count * 5u;
}
```

---

## 8. Algorithm — Delta

### 8.1 What it solves

**Delta encoding** — slowly-changing sensor values where consecutive
readings differ by small amounts. Temperature, pressure, humidity.

**Delta-of-delta encoding** — linearly trending data or monotonic
timestamps where the difference between differences is near zero.
Extremely effective for timestamp arrays.

### 8.2 Supported value types

```c
typedef enum {
    MC_DELTA_I32 = 0,   /* int32_t array — general purpose */
    MC_DELTA_U32 = 1,   /* uint32_t array — timestamps, counters */
    MC_DELTA_F32 = 2,   /* float array — sensor readings
                           Internally quantized: float → int32_t
                           via scale factor before delta coding. */
} mc_delta_type_t;
```

### 8.3 Delta context for F32

```c
/* F32 quantization: value_quantized = round(value * scale)
 * scale is stored in the encoded header.
 * Default scale: 100.0f (2 decimal places of precision).
 * Override with MICROCODEC_DELTA_F32_SCALE. */
#ifndef MICROCODEC_DELTA_F32_SCALE
#define MICROCODEC_DELTA_F32_SCALE  100.0f
#endif
```

### 8.4 Encoded format

```
Delta header (8 bytes):
  [ uint8_t  type        ]   mc_delta_type_t
  [ uint8_t  order       ]   1 = delta, 2 = delta-of-delta
  [ uint16_t count       ]   number of values
  [ float    f32_scale   ]   quantization scale (only meaningful for F32)

Followed by:
  [ first value as raw int32_t/uint32_t (4 bytes) ]
  [ remaining (count-1) deltas as zig-zag varints  ]

For order=2 (delta-of-delta):
  [ first value as raw    (4 bytes) ]
  [ first delta as raw    (4 bytes) ]
  [ remaining (count-2) delta-of-deltas as zig-zag varints ]
```

### 8.5 API

```c
/* Encode an int32_t array using delta or delta-of-delta coding.
 *
 * src_arr — input array of count int32_t values
 * count   — number of values (must be >= 1)
 * order   — 1 for delta, 2 for delta-of-delta
 * dst     — output buffer
 *
 * Returns MC_OK, dst.len set to encoded size.
 * Returns MC_ERR_INVALID if count == 0 or order not in {1,2}.
 * Returns MC_ERR_OVERFLOW if dst.cap insufficient.
 *
 * Safe output size: 8 + 4 + count * 5 bytes. */
mc_err_t mc_delta_encode_i32(const int32_t *src_arr,
                               size_t         count,
                               uint8_t        order,
                               mc_buf_t      *dst);


/* Decode delta-encoded int32_t array.
 *
 * dst_arr — caller-allocated array of count int32_t
 * count   — must match count stored in encoded header
 *
 * Returns MC_OK.
 * Returns MC_ERR_CORRUPT if header or stream is malformed.
 * Returns MC_ERR_INVALID if count mismatch. */
mc_err_t mc_delta_decode_i32(mc_slice_t  src,
                               int32_t    *dst_arr,
                               size_t      count);


/* Encode uint32_t array (e.g. timestamps). */
mc_err_t mc_delta_encode_u32(const uint32_t *src_arr,
                               size_t          count,
                               uint8_t         order,
                               mc_buf_t       *dst);


/* Decode uint32_t array. */
mc_err_t mc_delta_decode_u32(mc_slice_t  src,
                               uint32_t   *dst_arr,
                               size_t      count);


/* Encode float array.
 * Values are quantized to int32_t using MICROCODEC_DELTA_F32_SCALE,
 * then delta-coded. Scale stored in header — no external config needed.
 *
 * Precision loss: 1/scale (default: 0.01 for scale=100). */
mc_err_t mc_delta_encode_f32(const float *src_arr,
                               size_t       count,
                               uint8_t      order,
                               mc_buf_t    *dst);


/* Decode float array. Dequantizes using scale from header. */
mc_err_t mc_delta_decode_f32(mc_slice_t  src,
                               float      *dst_arr,
                               size_t      count);


/* Compute worst-case encoded size for count values. */
static inline size_t mc_delta_max_encoded_size(size_t count) {
    return 8u + 4u + count * 5u;
}
```

---

## 9. Algorithm — LZSS

### 9.1 What it solves

General-purpose binary compression for firmware payloads, config blobs,
and any data with repeated byte sequences longer than 3 bytes.
Better compression ratio than RLE for complex data.

### 9.2 Context struct (caller provides window buffer)

```c
/* Caller provides the window buffer — no internal allocation.
 * Window size must equal MICROCODEC_LZSS_WINDOW_SIZE. */
typedef struct {
    uint8_t window[MICROCODEC_LZSS_WINDOW_SIZE];
    size_t  window_pos;   /* Current write position in window (circular) */
} mc_lzss_ctx_t;
```

### 9.3 Encoded format

```
Bitstream of tokens:
  Flag bit = 0: literal byte follows (8 bits)
  Flag bit = 1: back-reference follows:
    offset: log2(WINDOW_SIZE) bits
    length: 4 bits (value + MIN_MATCH = actual length)

Flags are packed 8 per flag byte:
  [ flag_byte ] [ 8 tokens (literals or references) ] [ flag_byte ] ...

Header (4 bytes):
  [ uint32_t original_size ]   uncompressed size in bytes
```

### 9.4 API

```c
/* Initialize LZSS context. Must be called before encode/decode.
 * ctx — caller-allocated, zeroed by this function. */
void mc_lzss_ctx_init(mc_lzss_ctx_t *ctx);


/* Encode src into dst using LZSS.
 *
 * ctx must be initialized with mc_lzss_ctx_init().
 * Do NOT reuse ctx between unrelated encode calls without reinit.
 *
 * dst safe size: mc_lzss_max_encoded_size(src.len)
 *
 * Returns MC_OK, dst.len set to encoded size.
 * Returns MC_ERR_OVERFLOW if dst.cap insufficient.
 * Returns MC_ERR_INCOMPRESS if encoded size >= src.len
 *   (caller should store raw data instead). */
mc_err_t mc_lzss_encode(mc_lzss_ctx_t *ctx,
                          mc_slice_t     src,
                          mc_buf_t      *dst);


/* Decode LZSS-encoded src into dst.
 *
 * ctx must be initialized with mc_lzss_ctx_init().
 * dst.cap must be >= original_size (read from header).
 *
 * Returns MC_OK, dst.len set to decoded size.
 * Returns MC_ERR_CORRUPT if stream is malformed.
 * Returns MC_ERR_OVERFLOW if dst.cap < original_size. */
mc_err_t mc_lzss_decode(mc_lzss_ctx_t *ctx,
                          mc_slice_t     src,
                          mc_buf_t      *dst);


/* Compute worst-case encoded size.
 * In the absolute worst case: every byte becomes a literal with its flag bit.
 * Adds header (4 bytes) + flag bytes overhead. */
static inline size_t mc_lzss_max_encoded_size(size_t input_len) {
    /* flag bytes: ceil(input_len / 8), literals: input_len, header: 4 */
    return 4u + ((input_len + 7u) / 8u) + input_len;
}
```

---

## 10. Algorithm — Static Huffman

### 10.1 What it solves

Text-like or structured binary data where byte frequency distribution
is non-uniform and known in advance. Pre-built Huffman table for the
typical embedded sensor data byte distribution — no dynamic tree building.

Two modes:
- **Static table** — pre-built frequency table baked into the library.
  Zero overhead, works best for typical sensor/config data.
- **Adaptive table** — caller provides frequency counts, library builds
  canonical Huffman codes at encode time and embeds the table in the output.

### 10.2 Static table

The static table is built for this byte frequency distribution
(typical embedded sensor data):

```
Bytes 0x00..0x0F:   high frequency (zeros, small integers)
Bytes 0x10..0x7F:   medium frequency (ASCII range, register values)
Bytes 0x80..0xFF:   low frequency (high bytes, rarely used)
```

The static table is a `const` array in `mc_huff.c` — generated once,
never changes, zero runtime overhead.

### 10.3 Encoded format

**Static mode:**
```
Header (1 byte):
  [ uint8_t mode ]   0x00 = static table

Followed by: bitstream of Huffman codes, LSB first.
Padded to byte boundary with zero bits.
Trailer (4 bytes):
  [ uint32_t original_size ]
```

**Adaptive mode:**
```
Header (1 byte):
  [ uint8_t mode ]   0x01 = adaptive table

Table (variable):
  [ uint16_t symbol_count ]
  For each symbol with non-zero code:
    [ uint8_t symbol ]
    [ uint8_t code_len ]
    [ uint8_t code_bytes[] ]   ceil(code_len / 8) bytes, LSB first

Followed by: bitstream + trailer same as static mode.
```

### 10.4 API

```c
/* Encode src using the pre-built static Huffman table.
 *
 * dst safe size: mc_huff_max_encoded_size(src.len)
 *
 * Returns MC_OK, dst.len set to encoded size.
 * Returns MC_ERR_OVERFLOW if dst.cap insufficient.
 * Returns MC_ERR_INCOMPRESS if encoded size >= src.len. */
mc_err_t mc_huff_encode_static(mc_slice_t src, mc_buf_t *dst);


/* Decode static-Huffman-encoded data.
 *
 * Returns MC_OK, dst.len set to decoded size.
 * Returns MC_ERR_CORRUPT if header mode != 0x00 or stream malformed.
 * Returns MC_ERR_OVERFLOW if dst.cap < original_size. */
mc_err_t mc_huff_decode_static(mc_slice_t src, mc_buf_t *dst);


/* Build a Huffman table from observed byte frequencies.
 *
 * freq[256] — frequency count for each byte value (0 = symbol not present)
 * table_buf — caller-provided buffer for the canonical code table
 *             safe size: MICROCODEC_HUFF_SYMBOLS * 4 bytes
 *
 * Returns MC_OK, table filled with canonical codes.
 * Returns MC_ERR_INVALID if all frequencies are zero. */
typedef struct {
    uint16_t code;       /* Canonical Huffman code, LSB first */
    uint8_t  code_len;   /* Code length in bits (0 = symbol not present) */
} mc_huff_entry_t;

mc_err_t mc_huff_build_table(const uint32_t   freq[MICROCODEC_HUFF_SYMBOLS],
                               mc_huff_entry_t  table[MICROCODEC_HUFF_SYMBOLS]);


/* Encode using caller-provided (adaptive) table.
 *
 * Embeds the table in the output (adaptive mode header). */
mc_err_t mc_huff_encode_adaptive(mc_slice_t             src,
                                   const mc_huff_entry_t  table[MICROCODEC_HUFF_SYMBOLS],
                                   mc_buf_t              *dst);


/* Decode adaptive-Huffman-encoded data.
 * Reads the embedded table from the stream header. */
mc_err_t mc_huff_decode_adaptive(mc_slice_t src, mc_buf_t *dst);


/* Compute worst-case encoded size. */
static inline size_t mc_huff_max_encoded_size(size_t input_len) {
    /* Worst case: all symbols use max code length (15 bits) → ~2× expansion */
    /* Plus header (1 byte) + trailer (4 bytes) + table overhead */
    return 5u + (input_len * MICROCODEC_HUFF_MAX_CODE_LEN + 7u) / 8u
           + MICROCODEC_HUFF_SYMBOLS * 3u;   /* table overhead for adaptive */
}
```

---

## 11. Dispatch API

Single entry point for encoding/decoding with any algorithm.
Useful when the algorithm is selected at runtime.

```c
/* Encode src using the specified algorithm.
 *
 * alg     — algorithm to use
 * src     — input slice
 * dst     — output buffer
 * ctx     — algorithm context:
 *             MC_ALG_LZSS:   mc_lzss_ctx_t* (must be initialized)
 *             all others:    NULL
 *
 * Returns MC_OK or algorithm-specific error.
 * Returns MC_ERR_DISABLED if algorithm was compiled out. */
mc_err_t mc_encode(mc_alg_t   alg,
                    mc_slice_t src,
                    mc_buf_t  *dst,
                    void      *ctx);


/* Decode encoded src using the specified algorithm.
 *
 * Same ctx rules as mc_encode. */
mc_err_t mc_decode(mc_alg_t   alg,
                    mc_slice_t src,
                    mc_buf_t  *dst,
                    void      *ctx);


/* Compute worst-case encoded size for the given algorithm and input length. */
size_t mc_max_encoded_size(mc_alg_t alg, size_t input_len);


/* Return algorithm name as a string (for logging/debug). */
const char *mc_alg_name(mc_alg_t alg);
```

---

## 12. Compile-time limits and defaults

| Define | Default | Notes |
|--------|---------|-------|
| `MICROCODEC_ENABLE_RLE` | 1 | |
| `MICROCODEC_ENABLE_VARINT` | 1 | |
| `MICROCODEC_ENABLE_DELTA` | 1 | |
| `MICROCODEC_ENABLE_LZSS` | 1 | |
| `MICROCODEC_ENABLE_HUFF` | 1 | |
| `MICROCODEC_LZSS_WINDOW_SIZE` | 256 | Must be power of 2, 64–4096 |
| `MICROCODEC_LZSS_MIN_MATCH` | 3 | 2–8 |
| `MICROCODEC_LZSS_MAX_MATCH` | 18 | MIN_MATCH..255 |
| `MICROCODEC_HUFF_SYMBOLS` | 256 | Fixed — full byte alphabet |
| `MICROCODEC_HUFF_MAX_CODE_LEN` | 15 | 8–16 |
| `MICROCODEC_DELTA_F32_SCALE` | 100.0f | Quantization scale for F32 |

---

## 13. Test requirements

Use microtest (`github.com/Vanderhell/microtest`):
```c
#define MTEST_IMPLEMENTATION
#include "mtest.h"
```

### 13.1 RLE tests — target 20 tests

- Encode empty input → MC_OK, dst.len == 0
- Encode single byte → 2 bytes output (token + byte)
- Encode 128 identical bytes → 2 bytes (repeat token)
- Encode 129 identical bytes → 4 bytes (two tokens)
- Encode all-different bytes → 2 × N bytes (all literals)
- Encode mixed literal and repeat runs
- Decode round-trip: encode then decode → identical to original
- Decode empty input → MC_OK, dst.len == 0
- Decode corrupt token (bad count) → MC_ERR_CORRUPT
- Decode overflow (dst too small) → MC_ERR_OVERFLOW
- Encode overflow (dst too small) → MC_ERR_OVERFLOW
- NULL src.ptr → MC_ERR_INVALID
- NULL dst → MC_ERR_INVALID
- 1 byte of repeated data → correct 2-byte output
- Binary data (0x00, 0xFF mixed) → round-trip correct
- mc_rle_max_encoded_size(n) >= actual encoded size always
- Worst case: alternating 0x00/0xFF → encoded > input (check MC_ERR_INCOMPRESS NOT returned — RLE always encodes)
- Decode with exactly correct dst.cap → MC_OK
- Decode with dst.cap = original_size - 1 → MC_ERR_OVERFLOW
- Stress: 1024 bytes all zeros → 2 bytes encoded

### 13.2 Varint tests — target 25 tests

- Encode 0 → 1 byte (0x00)
- Encode 127 → 1 byte (0x7F)
- Encode 128 → 2 bytes (0x80, 0x01)
- Encode UINT32_MAX → 5 bytes
- Decode round-trip for 0, 1, 127, 128, 255, 256, 16383, 16384, UINT32_MAX
- Zig-zag encode 0 → 0
- Zig-zag encode -1 → 1
- Zig-zag encode 1 → 2
- Zig-zag encode INT32_MIN → UINT32_MAX (correct mapping)
- Zig-zag round-trip: encode i32 then decode → same value
- Array encode/decode round-trip: 10 uint32_t values
- Array encode/decode round-trip: 10 int32_t values (signed)
- Array with mixed values (0, UINT32_MAX, 128, 1) → correct
- Decode truncated stream → MC_ERR_CORRUPT
- Decode empty stream → MC_ERR_CORRUPT
- NULL pointer args → MC_ERR_INVALID
- Overflow: dst.cap = 0 → MC_ERR_OVERFLOW for encode
- mc_varint_max_encoded_size(n) >= actual encoded size always
- Timestamps array (monotonic uint32_t) → encodes efficiently
- Negative values round-trip correctly via zig-zag
- Large array: 1000 uint32_t → decode matches encode
- decode_u32_array bytes_consumed is correct
- Consecutive encode calls to same dst (manual append) → correct
- Single value array → same as single encode
- mc_varint_decode_u32 advances src correctly

### 13.3 Delta tests — target 25 tests

- Encode constant array (all same) → minimal output (all deltas = 0)
- Encode monotonically increasing by 1 → order=1 efficient
- Encode linearly increasing → order=2 extremely efficient (all dod=0)
- Decode round-trip i32 order=1: 10 values
- Decode round-trip i32 order=2: 10 values
- Decode round-trip u32 order=1: timestamps
- Decode round-trip u32 order=2: timestamps with constant rate
- Decode round-trip f32 order=1: temperature readings
- Decode round-trip f32 order=2: linearly trending temperature
- F32 precision: decoded values within 1/scale of original
- Single value array → MC_OK (no deltas needed)
- Two value array order=1 → correct
- Two value array order=2 → correct (uses one delta, zero dod)
- Invalid order (0 or 3) → MC_ERR_INVALID
- Count=0 → MC_ERR_INVALID
- Overflow in output buffer → MC_ERR_OVERFLOW
- Corrupt header (bad type) → MC_ERR_CORRUPT
- Count mismatch on decode → MC_ERR_INVALID
- NULL pointers → MC_ERR_INVALID
- Large negative deltas (INT32_MIN diffs) → correct zig-zag
- f32 scale stored and recovered from header correctly
- mc_delta_max_encoded_size(n) >= actual always
- Timestamps 0..999 step 100 → order=2 gives near-zero bytes
- Mixed positive/negative deltas round-trip
- 1000-element f32 array round-trip within precision

### 13.4 LZSS tests — target 25 tests

- Encode non-compressible data → MC_ERR_INCOMPRESS
- Encode highly repetitive data → significant compression
- Decode round-trip: encode then decode → identical to original
- Empty input → MC_OK, dst.len == header size
- Single byte → literal (no back-reference possible)
- Input shorter than MIN_MATCH → all literals
- Back-reference at window boundary → correct
- Back-reference to position 0 → correct
- Multiple back-references → all decoded correctly
- Window wraps around (circular) → correct
- Input larger than window → correct (old data evicted)
- Reinit ctx between calls → clean state
- Decode corrupt flag byte → MC_ERR_CORRUPT
- Decode back-reference past available data → MC_ERR_CORRUPT
- Decode overflow → MC_ERR_OVERFLOW
- Encode overflow → MC_ERR_OVERFLOW
- NULL ctx → MC_ERR_INVALID
- NULL src.ptr → MC_ERR_INVALID
- 256 bytes all identical → high compression ratio
- 1024 bytes repeating pattern → compression ratio > 2:1
- mc_lzss_max_encoded_size(n) >= actual always
- Decode with exactly original_size dst.cap → MC_OK
- Header original_size matches decoded output
- LZSS_WINDOW_SIZE=64 variant → compiles and correct
- Stress: 4096 bytes random-ish → decode matches encode

### 13.5 Huffman tests — target 25 tests

- Static encode empty → MC_OK
- Static encode single byte → correct
- Static encode all 256 possible byte values → round-trip
- Static decode round-trip: 100 bytes → identical
- Static encode non-compressible → MC_ERR_INCOMPRESS
- Static decode wrong mode byte → MC_ERR_CORRUPT
- mc_huff_build_table all zeros freq → MC_ERR_INVALID
- mc_huff_build_table single symbol → code_len=1
- mc_huff_build_table two symbols equal freq → both len=1
- Adaptive encode round-trip: build table, encode, decode
- Adaptive decode reads embedded table correctly
- Adaptive encode uniform distribution → near 8 bits/symbol
- Adaptive encode skewed distribution → compression
- Adaptive decode corrupt table → MC_ERR_CORRUPT
- Encode overflow → MC_ERR_OVERFLOW
- Decode overflow → MC_ERR_OVERFLOW
- NULL pointers → MC_ERR_INVALID
- mc_huff_max_encoded_size(n) >= actual always
- Static and adaptive round-trip identical data → same output
- Code lengths ≤ MICROCODEC_HUFF_MAX_CODE_LEN always
- Codes are prefix-free (no code is prefix of another)
- Decode padded bits ignored correctly
- Large input: 4096 bytes → round-trip correct
- Frequency-0 symbols never appear in encoded output
- Static table is valid canonical Huffman (compile-time verifiable)

### 13.6 Integration tests — target 20 tests

- RLE + Varint pipeline: encode with RLE then varint-encode the size
- Delta + LZSS pipeline: delta-encode float array, then LZSS the result
- Delta + Huff pipeline: delta-encode, then Huffman the result
- All algorithms via dispatch API mc_encode/mc_decode
- mc_alg_name returns non-NULL for all algorithms
- mc_max_encoded_size returns > 0 for all algorithms, input_len=100
- Disabled algorithm (MICROCODEC_ENABLE_RLE=0) → MC_ERR_DISABLED from dispatch
- Round-trip 1000 float sensor readings through delta+lzss pipeline
- Round-trip 1000 timestamps through delta (order=2) + varint
- Compression ratio test: delta+lzss on temperature data > 3:1
- Compression ratio test: rle on zero-padded firmware frame > 10:1
- mc_slice_t and mc_buf_t macros work correctly
- mc_encode with NULL ctx for LZSS → MC_ERR_INVALID
- mc_encode with initialized ctx for LZSS → MC_OK
- All algorithms handle zero-length input gracefully
- All algorithms handle 1-byte input gracefully
- All algorithms handle max realistic input (4096 bytes)
- Encode then decode never reads past src bounds
- Encode never writes past dst.cap
- No global state: two concurrent encode calls on different buffers → both correct

**Total target: ≥ 120 tests**

---

## 14. Usage examples

### 14.1 Temperature array compression

```c
#include "microcodec.h"

float temperatures[100] = { 23.1f, 23.2f, 23.1f, 23.3f, /* ... */ };

uint8_t encoded[512];
mc_buf_t dst = MC_BUF(encoded, sizeof(encoded));

/* Delta encoding — ideal for slowly changing sensor data */
mc_err_t err = mc_delta_encode_f32(temperatures, 100, 1, &dst);
if (err == MC_OK) {
    printf("Compressed %zu floats from %zu to %zu bytes\n",
           100 * sizeof(float), (size_t)(100 * 4), dst.len);
}

/* Decode back */
float recovered[100];
mc_slice_t src = MC_SLICE(encoded, dst.len);
mc_delta_decode_f32(src, recovered, 100);
```

### 14.2 Timestamp array compression

```c
uint32_t timestamps[200];   /* Monotonically increasing, 1s intervals */
for (int i = 0; i < 200; i++) timestamps[i] = 1700000000u + i;

uint8_t encoded[256];
mc_buf_t dst = MC_BUF(encoded, sizeof(encoded));

/* Delta-of-delta order=2 — near-perfect for constant-rate timestamps */
mc_delta_encode_u32(timestamps, 200, 2, &dst);
/* Result: ~12 bytes for 200 timestamps (vs 800 bytes raw) */
```

### 14.3 Firmware blob compression

```c
const uint8_t *firmware = /* ... */;
size_t firmware_len = 4096;

mc_lzss_ctx_t ctx;
mc_lzss_ctx_init(&ctx);

uint8_t compressed[mc_lzss_max_encoded_size(4096)];
mc_buf_t dst = MC_BUF(compressed, sizeof(compressed));

mc_err_t err = mc_lzss_encode(&ctx, MC_SLICE(firmware, firmware_len), &dst);
if (err == MC_ERR_INCOMPRESS) {
    /* Store raw — LZSS couldn't help */
}
```

### 14.4 Via dispatch API

```c
mc_lzss_ctx_t lzss_ctx;
mc_lzss_ctx_init(&lzss_ctx);

uint8_t out[1024];
mc_buf_t dst = MC_BUF(out, sizeof(out));

mc_encode(MC_ALG_LZSS, MC_SLICE(data, data_len), &dst, &lzss_ctx);
```

---

## 15. Integration with microdb

microcodec is intentionally separate from microdb.
Compress before storing, decompress after loading:

```c
/* Store compressed sensor burst */
float readings[50];
uint8_t compressed[256];
mc_buf_t cbuf = MC_BUF(compressed, sizeof(compressed));
mc_delta_encode_f32(readings, 50, 1, &cbuf);
microdb_kv_put(&db, "sensor_burst", compressed, cbuf.len);

/* Load and decompress */
uint8_t raw[256];
size_t raw_len;
microdb_kv_get(&db, "sensor_burst", raw, sizeof(raw), &raw_len);
float recovered[50];
mc_delta_decode_f32(MC_SLICE(raw, raw_len), recovered, 50);
```

---

## 16. What microcodec is NOT

microcodec is not a general-purpose compression library.
It does not implement zlib, LZ4, Brotli, or zstd.
It is tuned for embedded sensor data patterns — small arrays,
slowly-changing values, monotonic timestamps.

microcodec does not encrypt. Use `microcrypt` for encryption.

microcodec does not allocate. All buffers are caller-provided.

microcodec does not detect the best algorithm automatically.
The caller chooses based on data characteristics.

---

*microcodec v1.0 spec — C99 · Zero allocations · Five algorithms*
*Part of the micro-toolkit ecosystem — github.com/Vanderhell*
