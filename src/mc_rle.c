#include "microcodec.h"

#include <string.h>

#define RLE_REPEAT_FLAG 0x80u
#define RLE_COUNT_MASK 0x7Fu
#define RLE_MAX_COUNT 128u

static size_t mc_rle_repeat_run_length(mc_slice_t src, size_t start) {
    size_t run = 1u;

    while ((run < RLE_MAX_COUNT) &&
           ((start + run) < src.len) &&
           (src.ptr[start + run] == src.ptr[start])) {
        run++;
    }

    return run;
}

#if MICROCODEC_ENABLE_RLE
mc_err_t mc_rle_encode(mc_slice_t src, mc_buf_t *dst) {
    size_t i;

    if ((src.ptr == NULL) || (dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }

    dst->len = 0u;
    i = 0u;
    while (i < src.len) {
        const size_t run = mc_rle_repeat_run_length(src, i);

        if (run >= 2u) {
            if ((dst->len + 2u) > dst->cap) {
                dst->len = 0u;
                return MC_ERR_OVERFLOW;
            }

            dst->ptr[dst->len++] = (uint8_t)(RLE_REPEAT_FLAG | (uint8_t)(run - 1u));
            dst->ptr[dst->len++] = src.ptr[i];
            i += run;
            continue;
        }

        {
            const size_t literal_start = i;
            size_t literal_count = 0u;

            while ((literal_count < RLE_MAX_COUNT) && (i < src.len)) {
                if (((i + 1u) < src.len) && (src.ptr[i] == src.ptr[i + 1u])) {
                    break;
                }

                literal_count++;
                i++;
            }

            if (literal_count == 0u) {
                literal_count = 1u;
                i++;
            }

            if ((dst->len + 1u + literal_count) > dst->cap) {
                dst->len = 0u;
                return MC_ERR_OVERFLOW;
            }

            dst->ptr[dst->len++] = (uint8_t)(literal_count - 1u);
            memcpy(dst->ptr + dst->len, src.ptr + literal_start, literal_count);
            dst->len += literal_count;
        }
    }

    return MC_OK;
}

mc_err_t mc_rle_decode(mc_slice_t src, mc_buf_t *dst) {
    size_t i;

    if ((src.ptr == NULL) || (dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }

    dst->len = 0u;
    i = 0u;
    while (i < src.len) {
        const uint8_t token = src.ptr[i++];
        const size_t count = (size_t)(token & RLE_COUNT_MASK) + 1u;

        if ((token & RLE_REPEAT_FLAG) != 0u) {
            if (i >= src.len) {
                dst->len = 0u;
                return MC_ERR_CORRUPT;
            }

            if ((dst->len + count) > dst->cap) {
                dst->len = 0u;
                return MC_ERR_OVERFLOW;
            }

            memset(dst->ptr + dst->len, src.ptr[i++], count);
            dst->len += count;
            continue;
        }

        if ((i + count) > src.len) {
            dst->len = 0u;
            return MC_ERR_CORRUPT;
        }

        if ((dst->len + count) > dst->cap) {
            dst->len = 0u;
            return MC_ERR_OVERFLOW;
        }

        memcpy(dst->ptr + dst->len, src.ptr + i, count);
        dst->len += count;
        i += count;
    }

    return MC_OK;
}
#endif
