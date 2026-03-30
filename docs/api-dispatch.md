# API and dispatch notes

The direct codec APIs expose the full algorithm-specific surface area.

The dispatch API is intentionally narrower and optimized for common usage.

## Dispatch behavior in this repository

- `MC_ALG_RLE`: raw byte encode/decode
- `MC_ALG_VARINT`: `uint32_t[]` payloads passed via `src.ptr`
- `MC_ALG_DELTA`: `float[]` payloads passed via `src.ptr`, default order `1`
- `MC_ALG_LZSS`: raw bytes with `mc_lzss_ctx_t *ctx`
- `MC_ALG_HUFF`: raw bytes through static Huffman mode

If you need richer runtime dispatch for delta order, delta type, or adaptive
Huffman tables, document and extend that contract explicitly before publishing a
compatible API revision.
