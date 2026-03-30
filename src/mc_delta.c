#include "microcodec.h"

#include <limits.h>
#include <string.h>

#define MC_DELTA_HEADER_SIZE 8u
#define MC_DELTA_F32_MAX_COUNT 4096u

typedef struct {
    mc_delta_type_t type;
    uint8_t order;
    uint16_t count;
    float scale;
} mc_delta_header_t;

static mc_err_t mc_delta_write_header(mc_buf_t *dst,
                                      mc_delta_type_t type,
                                      uint8_t order,
                                      uint16_t count,
                                      float scale) {
    if ((dst->len + MC_DELTA_HEADER_SIZE) > dst->cap) {
        return MC_ERR_OVERFLOW;
    }

    dst->ptr[dst->len++] = (uint8_t)type;
    dst->ptr[dst->len++] = order;
    dst->ptr[dst->len++] = (uint8_t)(count & 0xFFu);
    dst->ptr[dst->len++] = (uint8_t)((count >> 8u) & 0xFFu);
    memcpy(dst->ptr + dst->len, &scale, sizeof(scale));
    dst->len += sizeof(scale);
    return MC_OK;
}

static mc_err_t mc_delta_read_header(mc_slice_t src, mc_delta_header_t *header) {
    if ((header == NULL) || (src.ptr == NULL) || (src.len < MC_DELTA_HEADER_SIZE)) {
        return MC_ERR_CORRUPT;
    }

    header->type = (mc_delta_type_t)src.ptr[0];
    header->order = src.ptr[1];
    header->count = (uint16_t)((uint16_t)src.ptr[2] | ((uint16_t)src.ptr[3] << 8u));
    memcpy(&header->scale, src.ptr + 4u, sizeof(header->scale));

    if ((header->type != MC_DELTA_I32) &&
        (header->type != MC_DELTA_U32) &&
        (header->type != MC_DELTA_F32)) {
        return MC_ERR_CORRUPT;
    }
    if ((header->order != 1u) && (header->order != 2u)) {
        return MC_ERR_CORRUPT;
    }

    return MC_OK;
}

static mc_err_t mc_delta_write_u32_raw(mc_buf_t *dst, uint32_t value) {
    if ((dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if ((dst->len + sizeof(value)) > dst->cap) {
        return MC_ERR_OVERFLOW;
    }

    memcpy(dst->ptr + dst->len, &value, sizeof(value));
    dst->len += sizeof(value);
    return MC_OK;
}

static mc_err_t mc_delta_read_u32_raw(mc_slice_t *src, uint32_t *value) {
    if ((src == NULL) || (src->ptr == NULL) || (value == NULL)) {
        return MC_ERR_INVALID;
    }
    if (src->len < sizeof(*value)) {
        return MC_ERR_CORRUPT;
    }

    memcpy(value, src->ptr, sizeof(*value));
    src->ptr += sizeof(*value);
    src->len -= sizeof(*value);
    return MC_OK;
}

static mc_err_t mc_delta_check_encode_args(const void *src_arr,
                                           size_t count,
                                           uint8_t order,
                                           const mc_buf_t *dst) {
    if ((src_arr == NULL) || (dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if ((count == 0u) || (count > UINT16_MAX) || ((order != 1u) && (order != 2u))) {
        return MC_ERR_INVALID;
    }
    return MC_OK;
}

static mc_err_t mc_delta_check_decode_args(mc_slice_t src, const void *dst_arr, size_t count) {
    if ((dst_arr == NULL) || (count == 0u) || (src.ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    return MC_OK;
}

static mc_err_t mc_delta_encode_i32_like(const int32_t *src_arr,
                                         size_t count,
                                         uint8_t order,
                                         mc_buf_t *dst,
                                         mc_delta_type_t type,
                                         float scale) {
    mc_err_t err;
    size_t i;

    err = mc_delta_check_encode_args(src_arr, count, order, dst);
    if (err != MC_OK) {
        return err;
    }

    err = mc_delta_write_header(dst, type, order, (uint16_t)count, scale);
    if (err != MC_OK) {
        return err;
    }

    err = mc_delta_write_u32_raw(dst, (uint32_t)src_arr[0]);
    if (err != MC_OK) {
        return err;
    }

    if (count == 1u) {
        return MC_OK;
    }

    if (order == 1u) {
        for (i = 1u; i < count; ++i) {
            const int64_t delta64 = (int64_t)src_arr[i] - (int64_t)src_arr[i - 1u];
            const int32_t delta = (int32_t)delta64;
            err = mc_varint_encode_i32(delta, dst);
            if (err != MC_OK) {
                return err;
            }
        }
    } else {
        const int64_t first_delta64 = (int64_t)src_arr[1] - (int64_t)src_arr[0];
        const int32_t first_delta = (int32_t)first_delta64;

        err = mc_delta_write_u32_raw(dst, (uint32_t)first_delta);
        if (err != MC_OK) {
            return err;
        }

        for (i = 2u; i < count; ++i) {
            const int64_t delta64 = (int64_t)src_arr[i] - (int64_t)src_arr[i - 1u];
            const int64_t prev64 = (int64_t)src_arr[i - 1u] - (int64_t)src_arr[i - 2u];
            const int32_t dod = (int32_t)(delta64 - prev64);
            err = mc_varint_encode_i32(dod, dst);
            if (err != MC_OK) {
                return err;
            }
        }
    }

    return MC_OK;
}

static mc_err_t mc_delta_decode_i32_like(mc_slice_t src,
                                         int32_t *dst_arr,
                                         size_t count,
                                         mc_delta_type_t expected_type,
                                         float *out_scale) {
    mc_delta_header_t header;
    mc_err_t err;
    uint32_t raw;
    size_t i;

    err = mc_delta_check_decode_args(src, dst_arr, count);
    if (err != MC_OK) {
        return err;
    }

    err = mc_delta_read_header(src, &header);
    if (err != MC_OK) {
        return err;
    }
    if (header.type != expected_type) {
        return MC_ERR_CORRUPT;
    }
    if ((size_t)header.count != count) {
        return MC_ERR_INVALID;
    }

    src.ptr += MC_DELTA_HEADER_SIZE;
    src.len -= MC_DELTA_HEADER_SIZE;

    err = mc_delta_read_u32_raw(&src, &raw);
    if (err != MC_OK) {
        return err;
    }
    dst_arr[0] = (int32_t)raw;

    if (out_scale != NULL) {
        *out_scale = header.scale;
    }

    if (count == 1u) {
        return MC_OK;
    }

    if (header.order == 1u) {
        for (i = 1u; i < count; ++i) {
            int32_t delta;
            err = mc_varint_decode_i32(&src, &delta);
            if (err != MC_OK) {
                return err;
            }
            dst_arr[i] = dst_arr[i - 1u] + delta;
        }
    } else {
        int32_t first_delta;

        err = mc_delta_read_u32_raw(&src, &raw);
        if (err != MC_OK) {
            return err;
        }
        first_delta = (int32_t)raw;
        dst_arr[1] = dst_arr[0] + first_delta;

        for (i = 2u; i < count; ++i) {
            const int32_t prev_delta = dst_arr[i - 1u] - dst_arr[i - 2u];
            int32_t dod;
            err = mc_varint_decode_i32(&src, &dod);
            if (err != MC_OK) {
                return err;
            }
            dst_arr[i] = dst_arr[i - 1u] + prev_delta + dod;
        }
    }

    return MC_OK;
}

static int32_t mc_delta_quantize_f32(float value, float scale) {
    const float scaled = value * scale;
    if (scaled >= 0.0f) {
        return (int32_t)(scaled + 0.5f);
    }
    return (int32_t)(scaled - 0.5f);
}

#if MICROCODEC_ENABLE_DELTA
mc_err_t mc_delta_encode_i32(const int32_t *src_arr,
                             size_t count,
                             uint8_t order,
                             mc_buf_t *dst) {
    return mc_delta_encode_i32_like(src_arr, count, order, dst, MC_DELTA_I32, 0.0f);
}

mc_err_t mc_delta_decode_i32(mc_slice_t src, int32_t *dst_arr, size_t count) {
    return mc_delta_decode_i32_like(src, dst_arr, count, MC_DELTA_I32, NULL);
}

mc_err_t mc_delta_encode_u32(const uint32_t *src_arr,
                             size_t count,
                             uint8_t order,
                             mc_buf_t *dst) {
    int32_t converted[UINT16_MAX / 16];
    size_t i;
    mc_err_t err;

    err = mc_delta_check_encode_args(src_arr, count, order, dst);
    if (err != MC_OK) {
        return err;
    }
    if (count > (sizeof(converted) / sizeof(converted[0]))) {
        return MC_ERR_INVALID;
    }

    for (i = 0u; i < count; ++i) {
        if (src_arr[i] > (uint32_t)INT_MAX) {
            return MC_ERR_INVALID;
        }
        converted[i] = (int32_t)src_arr[i];
    }

    return mc_delta_encode_i32_like(converted, count, order, dst, MC_DELTA_U32, 0.0f);
}

mc_err_t mc_delta_decode_u32(mc_slice_t src, uint32_t *dst_arr, size_t count) {
    int32_t decoded[UINT16_MAX / 16];
    mc_err_t err;
    size_t i;

    if ((dst_arr == NULL) || (count == 0u) || (src.ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if (count > (sizeof(decoded) / sizeof(decoded[0]))) {
        return MC_ERR_INVALID;
    }

    err = mc_delta_decode_i32_like(src, decoded, count, MC_DELTA_U32, NULL);
    if (err != MC_OK) {
        return err;
    }

    for (i = 0u; i < count; ++i) {
        if (decoded[i] < 0) {
            return MC_ERR_CORRUPT;
        }
        dst_arr[i] = (uint32_t)decoded[i];
    }

    return MC_OK;
}

mc_err_t mc_delta_encode_f32(const float *src_arr,
                             size_t count,
                             uint8_t order,
                             mc_buf_t *dst) {
    int32_t quantized[MC_DELTA_F32_MAX_COUNT];
    size_t i;
    mc_err_t err;

    err = mc_delta_check_encode_args(src_arr, count, order, dst);
    if (err != MC_OK) {
        return err;
    }
    if (count > MC_DELTA_F32_MAX_COUNT) {
        return MC_ERR_INVALID;
    }

    for (i = 0u; i < count; ++i) {
        quantized[i] = mc_delta_quantize_f32(src_arr[i], MICROCODEC_DELTA_F32_SCALE);
    }

    return mc_delta_encode_i32_like(quantized, count, order, dst,
                                    MC_DELTA_F32, MICROCODEC_DELTA_F32_SCALE);
}

mc_err_t mc_delta_decode_f32(mc_slice_t src, float *dst_arr, size_t count) {
    int32_t quantized[MC_DELTA_F32_MAX_COUNT];
    float scale;
    mc_err_t err;
    size_t i;

    if ((dst_arr == NULL) || (count == 0u) || (src.ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if (count > MC_DELTA_F32_MAX_COUNT) {
        return MC_ERR_INVALID;
    }

    err = mc_delta_decode_i32_like(src, quantized, count, MC_DELTA_F32, &scale);
    if (err != MC_OK) {
        return err;
    }
    if (scale == 0.0f) {
        return MC_ERR_CORRUPT;
    }

    for (i = 0u; i < count; ++i) {
        dst_arr[i] = (float)quantized[i] / scale;
    }

    return MC_OK;
}
#endif
