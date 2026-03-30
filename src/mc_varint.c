#include "microcodec.h"

#include <string.h>

static uint32_t mc_zigzag_encode_i32_impl(int32_t value) {
    const uint32_t uvalue = (uint32_t)value;
    const uint32_t sign = (uint32_t)-(int32_t)(uvalue >> 31u);
    return (uvalue << 1u) ^ sign;
}

static int32_t mc_zigzag_decode_i32_impl(uint32_t value) {
    return (int32_t)((value >> 1u) ^ (uint32_t)-(int32_t)(value & 1u));
}

#if MICROCODEC_ENABLE_VARINT
mc_err_t mc_varint_encode_u32(uint32_t value, mc_buf_t *dst) {
    uint8_t buf[5];
    size_t n;

    if ((dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }

    n = 0u;
    do {
        buf[n++] = (uint8_t)((value & 0x7Fu) | ((value > 0x7Fu) ? 0x80u : 0u));
        value >>= 7u;
    } while (value != 0u);

    if ((dst->len + n) > dst->cap) {
        return MC_ERR_OVERFLOW;
    }

    memcpy(dst->ptr + dst->len, buf, n);
    dst->len += n;
    return MC_OK;
}

mc_err_t mc_varint_decode_u32(mc_slice_t *src, uint32_t *value) {
    uint32_t result;
    uint8_t shift;

    if ((src == NULL) || (src->ptr == NULL) || (value == NULL)) {
        return MC_ERR_INVALID;
    }

    result = 0u;
    shift = 0u;
    while (src->len > 0u) {
        const uint8_t byte = src->ptr[0];
        src->ptr++;
        src->len--;

        result |= (uint32_t)(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0u) {
            *value = result;
            return MC_OK;
        }

        shift = (uint8_t)(shift + 7u);
        if (shift >= 35u) {
            return MC_ERR_CORRUPT;
        }
    }

    return MC_ERR_CORRUPT;
}

mc_err_t mc_varint_encode_i32(int32_t value, mc_buf_t *dst) {
    return mc_varint_encode_u32(mc_zigzag_encode_i32_impl(value), dst);
}

mc_err_t mc_varint_decode_i32(mc_slice_t *src, int32_t *value) {
    uint32_t encoded;
    mc_err_t err;

    if (value == NULL) {
        return MC_ERR_INVALID;
    }

    err = mc_varint_decode_u32(src, &encoded);
    if (err != MC_OK) {
        return err;
    }

    *value = mc_zigzag_decode_i32_impl(encoded);
    return MC_OK;
}

mc_err_t mc_varint_encode_u32_array(const uint32_t *src_arr,
                                    size_t count,
                                    mc_buf_t *dst) {
    size_t i;

    if ((dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if ((count > 0u) && (src_arr == NULL)) {
        return MC_ERR_INVALID;
    }

    for (i = 0u; i < count; ++i) {
        const mc_err_t err = mc_varint_encode_u32(src_arr[i], dst);
        if (err != MC_OK) {
            return err;
        }
    }

    return MC_OK;
}

mc_err_t mc_varint_decode_u32_array(mc_slice_t src,
                                    uint32_t *dst_arr,
                                    size_t count,
                                    size_t *bytes_consumed) {
    mc_slice_t cursor;
    size_t i;

    if ((dst_arr == NULL) || (bytes_consumed == NULL) ||
        ((count > 0u) && (src.ptr == NULL))) {
        return MC_ERR_INVALID;
    }

    cursor = src;
    for (i = 0u; i < count; ++i) {
        const mc_err_t err = mc_varint_decode_u32(&cursor, &dst_arr[i]);
        if (err != MC_OK) {
            return err;
        }
    }

    *bytes_consumed = src.len - cursor.len;
    return MC_OK;
}

mc_err_t mc_varint_encode_i32_array(const int32_t *src_arr,
                                    size_t count,
                                    mc_buf_t *dst) {
    size_t i;

    if ((dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if ((count > 0u) && (src_arr == NULL)) {
        return MC_ERR_INVALID;
    }

    for (i = 0u; i < count; ++i) {
        const mc_err_t err = mc_varint_encode_i32(src_arr[i], dst);
        if (err != MC_OK) {
            return err;
        }
    }

    return MC_OK;
}

mc_err_t mc_varint_decode_i32_array(mc_slice_t src,
                                    int32_t *dst_arr,
                                    size_t count,
                                    size_t *bytes_consumed) {
    mc_slice_t cursor;
    size_t i;

    if ((dst_arr == NULL) || (bytes_consumed == NULL) ||
        ((count > 0u) && (src.ptr == NULL))) {
        return MC_ERR_INVALID;
    }

    cursor = src;
    for (i = 0u; i < count; ++i) {
        const mc_err_t err = mc_varint_decode_i32(&cursor, &dst_arr[i]);
        if (err != MC_OK) {
            return err;
        }
    }

    *bytes_consumed = src.len - cursor.len;
    return MC_OK;
}
#endif
