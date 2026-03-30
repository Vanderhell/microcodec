# Architecture

`microcodec` is organized as a small set of focused C modules:

- `include/microcodec.h`: public API, compile-time configuration, shared types
- `src/mc_rle.c`: run-length encoding
- `src/mc_varint.c`: varint and zig-zag encoding
- `src/mc_delta.c`: delta and delta-of-delta encoding for `i32`, `u32`, `f32`
- `src/mc_lzss.c`: fixed-window LZSS using caller-owned context
- `src/mc_huff.c`: static and adaptive Huffman coding
- `src/microcodec.c`: dispatch API

## Design constraints

- zero heap allocation
- zero hidden ownership
- caller-provided buffers
- compile-time feature flags
- no global mutable runtime state

## Data model

The shared types are:

- `mc_slice_t`: non-owning input view
- `mc_buf_t`: caller-owned output buffer with capacity and written length
- `mc_err_t`: explicit error-code contract
- `mc_alg_t`: runtime dispatch selector
