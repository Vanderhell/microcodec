/*
 * microcodec.h - Compression library for embedded sensor data
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef MICROCODEC_H
#define MICROCODEC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef MICROCODEC_ENABLE_RLE
#define MICROCODEC_ENABLE_RLE 1
#endif

#ifndef MICROCODEC_ENABLE_VARINT
#define MICROCODEC_ENABLE_VARINT 1
#endif

#ifndef MICROCODEC_ENABLE_DELTA
#define MICROCODEC_ENABLE_DELTA 1
#endif

#ifndef MICROCODEC_ENABLE_LZSS
#define MICROCODEC_ENABLE_LZSS 1
#endif

#ifndef MICROCODEC_ENABLE_HUFF
#define MICROCODEC_ENABLE_HUFF 1
#endif

#ifndef MICROCODEC_LZSS_WINDOW_SIZE
#define MICROCODEC_LZSS_WINDOW_SIZE 256
#endif

#ifndef MICROCODEC_LZSS_MIN_MATCH
#define MICROCODEC_LZSS_MIN_MATCH 3
#endif

#ifndef MICROCODEC_LZSS_MAX_MATCH
#define MICROCODEC_LZSS_MAX_MATCH 18
#endif

#ifndef MICROCODEC_HUFF_SYMBOLS
#define MICROCODEC_HUFF_SYMBOLS 256
#endif

#ifndef MICROCODEC_HUFF_MAX_CODE_LEN
#define MICROCODEC_HUFF_MAX_CODE_LEN 15
#endif

#ifndef MICROCODEC_DELTA_F32_SCALE
#define MICROCODEC_DELTA_F32_SCALE 100.0f
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define MC_STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)
#else
#define MC_STATIC_ASSERT_GLUE_(a, b) a##b
#define MC_STATIC_ASSERT_GLUE(a, b) MC_STATIC_ASSERT_GLUE_(a, b)
#define MC_STATIC_ASSERT(expr, msg) \
    typedef char MC_STATIC_ASSERT_GLUE(mc_static_assert_, __LINE__)[(expr) ? 1 : -1]
#endif

MC_STATIC_ASSERT((MICROCODEC_LZSS_WINDOW_SIZE &
                  (MICROCODEC_LZSS_WINDOW_SIZE - 1)) == 0,
                 "MICROCODEC_LZSS_WINDOW_SIZE must be a power of 2");
MC_STATIC_ASSERT(MICROCODEC_LZSS_WINDOW_SIZE >= 64,
                 "MICROCODEC_LZSS_WINDOW_SIZE must be >= 64");
MC_STATIC_ASSERT(MICROCODEC_LZSS_MIN_MATCH >= 2,
                 "MICROCODEC_LZSS_MIN_MATCH must be >= 2");
MC_STATIC_ASSERT(MICROCODEC_LZSS_MAX_MATCH > MICROCODEC_LZSS_MIN_MATCH,
                 "MICROCODEC_LZSS_MAX_MATCH must be > MIN_MATCH");

typedef enum {
    MC_OK = 0,
    MC_ERR_INVALID = -1,
    MC_ERR_OVERFLOW = -2,
    MC_ERR_CORRUPT = -3,
    MC_ERR_DISABLED = -4,
    MC_ERR_INCOMPRESS = -5
} mc_err_t;

typedef struct {
    const uint8_t *ptr;
    size_t len;
} mc_slice_t;

typedef struct {
    uint8_t *ptr;
    size_t cap;
    size_t len;
} mc_buf_t;

typedef enum {
    MC_ALG_RLE = 0,
    MC_ALG_VARINT = 1,
    MC_ALG_DELTA = 2,
    MC_ALG_LZSS = 3,
    MC_ALG_HUFF = 4
} mc_alg_t;

#define MC_SLICE(ptr_, len_) ((mc_slice_t){ (const uint8_t *)(ptr_), (len_) })
#define MC_BUF(ptr_, cap_) ((mc_buf_t){ (uint8_t *)(ptr_), (cap_), 0u })

typedef enum {
    MC_DELTA_I32 = 0,
    MC_DELTA_U32 = 1,
    MC_DELTA_F32 = 2
} mc_delta_type_t;

typedef struct {
    uint8_t window[MICROCODEC_LZSS_WINDOW_SIZE];
    size_t window_pos;
} mc_lzss_ctx_t;

typedef struct {
    uint16_t code;
    uint8_t code_len;
} mc_huff_entry_t;

#if MICROCODEC_ENABLE_RLE
/* Encode src into dst using RLE. */
mc_err_t mc_rle_encode(mc_slice_t src, mc_buf_t *dst);

/* Decode RLE-encoded src into dst. */
mc_err_t mc_rle_decode(mc_slice_t src, mc_buf_t *dst);

/* Compute worst-case encoded size for input of len bytes. */
static inline size_t mc_rle_max_encoded_size(size_t len) {
    return len * 2u;
}
#endif

#if MICROCODEC_ENABLE_VARINT
/* Encode a single uint32_t as varint into dst. */
mc_err_t mc_varint_encode_u32(uint32_t value, mc_buf_t *dst);

/* Decode a varint from src into value. */
mc_err_t mc_varint_decode_u32(mc_slice_t *src, uint32_t *value);

/* Encode int32_t using zig-zag then varint. */
mc_err_t mc_varint_encode_i32(int32_t value, mc_buf_t *dst);

/* Decode zig-zag varint into int32_t. */
mc_err_t mc_varint_decode_i32(mc_slice_t *src, int32_t *value);

/* Encode an array of uint32_t values as varints. */
mc_err_t mc_varint_encode_u32_array(const uint32_t *src_arr,
                                    size_t count,
                                    mc_buf_t *dst);

/* Decode an array of varints into dst_arr. */
mc_err_t mc_varint_decode_u32_array(mc_slice_t src,
                                    uint32_t *dst_arr,
                                    size_t count,
                                    size_t *bytes_consumed);

/* Encode int32_t array with zig-zag + varint. */
mc_err_t mc_varint_encode_i32_array(const int32_t *src_arr,
                                    size_t count,
                                    mc_buf_t *dst);

/* Decode int32_t array. */
mc_err_t mc_varint_decode_i32_array(mc_slice_t src,
                                    int32_t *dst_arr,
                                    size_t count,
                                    size_t *bytes_consumed);

/* Compute worst-case encoded size for count uint32_t values. */
static inline size_t mc_varint_max_encoded_size(size_t count) {
    return count * 5u;
}
#endif

#if MICROCODEC_ENABLE_DELTA
/* Encode an int32_t array using delta or delta-of-delta coding. */
mc_err_t mc_delta_encode_i32(const int32_t *src_arr,
                             size_t count,
                             uint8_t order,
                             mc_buf_t *dst);

/* Decode delta-encoded int32_t array. */
mc_err_t mc_delta_decode_i32(mc_slice_t src,
                             int32_t *dst_arr,
                             size_t count);

/* Encode uint32_t array. */
mc_err_t mc_delta_encode_u32(const uint32_t *src_arr,
                             size_t count,
                             uint8_t order,
                             mc_buf_t *dst);

/* Decode uint32_t array. */
mc_err_t mc_delta_decode_u32(mc_slice_t src,
                             uint32_t *dst_arr,
                             size_t count);

/* Encode float array. */
mc_err_t mc_delta_encode_f32(const float *src_arr,
                             size_t count,
                             uint8_t order,
                             mc_buf_t *dst);

/* Decode float array. */
mc_err_t mc_delta_decode_f32(mc_slice_t src,
                             float *dst_arr,
                             size_t count);

/* Compute worst-case encoded size for count values. */
static inline size_t mc_delta_max_encoded_size(size_t count) {
    return 8u + 4u + count * 5u;
}
#endif

#if MICROCODEC_ENABLE_LZSS
/* Initialize LZSS context. */
void mc_lzss_ctx_init(mc_lzss_ctx_t *ctx);

/* Encode src into dst using LZSS. */
mc_err_t mc_lzss_encode(mc_lzss_ctx_t *ctx,
                        mc_slice_t src,
                        mc_buf_t *dst);

/* Decode LZSS-encoded src into dst. */
mc_err_t mc_lzss_decode(mc_lzss_ctx_t *ctx,
                        mc_slice_t src,
                        mc_buf_t *dst);

/* Compute worst-case encoded size. */
static inline size_t mc_lzss_max_encoded_size(size_t input_len) {
    return 4u + ((input_len + 7u) / 8u) + input_len;
}
#endif

#if MICROCODEC_ENABLE_HUFF
/* Encode src using the pre-built static Huffman table. */
mc_err_t mc_huff_encode_static(mc_slice_t src, mc_buf_t *dst);

/* Decode static-Huffman-encoded data. */
mc_err_t mc_huff_decode_static(mc_slice_t src, mc_buf_t *dst);

/* Build a Huffman table from observed byte frequencies. */
mc_err_t mc_huff_build_table(const uint32_t freq[MICROCODEC_HUFF_SYMBOLS],
                             mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS]);

/* Encode using caller-provided adaptive table. */
mc_err_t mc_huff_encode_adaptive(mc_slice_t src,
                                 const mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS],
                                 mc_buf_t *dst);

/* Decode adaptive-Huffman-encoded data. */
mc_err_t mc_huff_decode_adaptive(mc_slice_t src, mc_buf_t *dst);

/* Compute worst-case encoded size. */
static inline size_t mc_huff_max_encoded_size(size_t input_len) {
    return 5u + ((input_len * MICROCODEC_HUFF_MAX_CODE_LEN + 7u) / 8u) +
           MICROCODEC_HUFF_SYMBOLS * 3u;
}
#endif

/* Encode src using the specified algorithm. */
mc_err_t mc_encode(mc_alg_t alg,
                   mc_slice_t src,
                   mc_buf_t *dst,
                   void *ctx);

/* Decode encoded src using the specified algorithm. */
mc_err_t mc_decode(mc_alg_t alg,
                   mc_slice_t src,
                   mc_buf_t *dst,
                   void *ctx);

/* Compute worst-case encoded size for the given algorithm and input length. */
size_t mc_max_encoded_size(mc_alg_t alg, size_t input_len);

/* Return algorithm name as a string. */
const char *mc_alg_name(mc_alg_t alg);

#endif /* MICROCODEC_H */
