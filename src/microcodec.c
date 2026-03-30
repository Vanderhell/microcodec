#include "microcodec.h"

static mc_err_t mc_decode_varint_dispatch(mc_slice_t src, mc_buf_t *dst) {
    size_t count = 0u;
    mc_slice_t cursor = src;

    if ((dst == NULL) || (dst->ptr == NULL) || (src.ptr == NULL)) {
        return MC_ERR_INVALID;
    }

    while (cursor.len > 0u) {
        uint32_t value = 0u;
        mc_err_t err = mc_varint_decode_u32(&cursor, &value);
        if (err != MC_OK) {
            dst->len = 0u;
            return err;
        }
        if (((count + 1u) * sizeof(uint32_t)) > dst->cap) {
            dst->len = 0u;
            return MC_ERR_OVERFLOW;
        }
        ((uint32_t *)dst->ptr)[count++] = value;
    }

    dst->len = count * sizeof(uint32_t);
    return MC_OK;
}

static mc_err_t mc_decode_delta_dispatch(mc_slice_t src, mc_buf_t *dst) {
    size_t count;

    if ((dst == NULL) || (dst->ptr == NULL) || (src.ptr == NULL) || (src.len < 4u)) {
        return MC_ERR_INVALID;
    }
    if (src.len < 8u) {
        return MC_ERR_CORRUPT;
    }

    count = (size_t)((uint16_t)src.ptr[2] | ((uint16_t)src.ptr[3] << 8u));
    if ((count * sizeof(float)) > dst->cap) {
        return MC_ERR_OVERFLOW;
    }

    return mc_delta_decode_f32(src, (float *)dst->ptr, count);
}

mc_err_t mc_encode(mc_alg_t alg, mc_slice_t src, mc_buf_t *dst, void *ctx) {
    switch (alg) {
        case MC_ALG_RLE:
#if MICROCODEC_ENABLE_RLE
            return mc_rle_encode(src, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        case MC_ALG_VARINT:
#if MICROCODEC_ENABLE_VARINT
            if ((src.len % sizeof(uint32_t)) != 0u) {
                return MC_ERR_INVALID;
            }
            return mc_varint_encode_u32_array((const uint32_t *)src.ptr,
                                              src.len / sizeof(uint32_t), dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        case MC_ALG_DELTA:
#if MICROCODEC_ENABLE_DELTA
            if ((src.len % sizeof(float)) != 0u) {
                return MC_ERR_INVALID;
            }
            return mc_delta_encode_f32((const float *)src.ptr,
                                       src.len / sizeof(float), 1u, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        case MC_ALG_LZSS:
#if MICROCODEC_ENABLE_LZSS
            if (ctx == NULL) {
                return MC_ERR_INVALID;
            }
            return mc_lzss_encode((mc_lzss_ctx_t *)ctx, src, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        case MC_ALG_HUFF:
#if MICROCODEC_ENABLE_HUFF
            return mc_huff_encode_static(src, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        default:
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
    }
}

mc_err_t mc_decode(mc_alg_t alg, mc_slice_t src, mc_buf_t *dst, void *ctx) {
    switch (alg) {
        case MC_ALG_RLE:
#if MICROCODEC_ENABLE_RLE
            return mc_rle_decode(src, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        case MC_ALG_VARINT:
#if MICROCODEC_ENABLE_VARINT
            return mc_decode_varint_dispatch(src, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        case MC_ALG_DELTA:
#if MICROCODEC_ENABLE_DELTA
            return mc_decode_delta_dispatch(src, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        case MC_ALG_LZSS:
#if MICROCODEC_ENABLE_LZSS
            if (ctx == NULL) {
                return MC_ERR_INVALID;
            }
            return mc_lzss_decode((mc_lzss_ctx_t *)ctx, src, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        case MC_ALG_HUFF:
#if MICROCODEC_ENABLE_HUFF
            return mc_huff_decode_static(src, dst);
#else
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
#endif
        default:
            (void)src; (void)dst; (void)ctx;
            return MC_ERR_DISABLED;
    }
}

size_t mc_max_encoded_size(mc_alg_t alg, size_t input_len) {
    switch (alg) {
        case MC_ALG_RLE:
#if MICROCODEC_ENABLE_RLE
            return mc_rle_max_encoded_size(input_len);
#else
            return 0u;
#endif
        case MC_ALG_VARINT:
#if MICROCODEC_ENABLE_VARINT
            return mc_varint_max_encoded_size(input_len / sizeof(uint32_t));
#else
            return 0u;
#endif
        case MC_ALG_DELTA:
#if MICROCODEC_ENABLE_DELTA
            return mc_delta_max_encoded_size(input_len / sizeof(float));
#else
            return 0u;
#endif
        case MC_ALG_LZSS:
#if MICROCODEC_ENABLE_LZSS
            return mc_lzss_max_encoded_size(input_len);
#else
            return 0u;
#endif
        case MC_ALG_HUFF:
#if MICROCODEC_ENABLE_HUFF
            return mc_huff_max_encoded_size(input_len);
#else
            return 0u;
#endif
        default:
            return 0u;
    }
}

const char *mc_alg_name(mc_alg_t alg) {
    switch (alg) {
        case MC_ALG_RLE:    return "rle";
        case MC_ALG_VARINT: return "varint";
        case MC_ALG_DELTA:  return "delta";
        case MC_ALG_LZSS:   return "lzss";
        case MC_ALG_HUFF:   return "huff";
        default:            return "unknown";
    }
}
