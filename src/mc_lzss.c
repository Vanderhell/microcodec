#include "microcodec.h"

#include <string.h>

#define MC_LZSS_HEADER_SIZE 4u

typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t *len;
    size_t flag_pos;
    uint8_t flag;
    uint8_t flag_bit;
} mc_lzss_writer_t;

static void mc_lzss_update_window(mc_lzss_ctx_t *ctx, uint8_t byte) {
    ctx->window[ctx->window_pos] = byte;
    ctx->window_pos = (ctx->window_pos + 1u) & (MICROCODEC_LZSS_WINDOW_SIZE - 1u);
}

static void mc_lzss_writer_init(mc_lzss_writer_t *writer, mc_buf_t *dst) {
    writer->buf = dst->ptr;
    writer->cap = dst->cap;
    writer->len = &dst->len;
    writer->flag_pos = dst->len;
    writer->flag = 0u;
    writer->flag_bit = 0u;
    (*writer->len)++;
}

static mc_err_t mc_lzss_writer_next_flag(mc_lzss_writer_t *writer) {
    writer->buf[writer->flag_pos] = writer->flag;
    writer->flag_pos = *writer->len;
    if (*writer->len >= writer->cap) {
        return MC_ERR_OVERFLOW;
    }
    (*writer->len)++;
    writer->flag = 0u;
    writer->flag_bit = 0u;
    return MC_OK;
}

static mc_err_t mc_lzss_writer_literal(mc_lzss_writer_t *writer, uint8_t byte) {
    if (*writer->len >= writer->cap) {
        return MC_ERR_OVERFLOW;
    }

    writer->buf[(*writer->len)++] = byte;
    writer->flag_bit++;
    if (writer->flag_bit == 8u) {
        return mc_lzss_writer_next_flag(writer);
    }
    return MC_OK;
}

static mc_err_t mc_lzss_writer_backref(mc_lzss_writer_t *writer,
                                       size_t offset,
                                       size_t length) {
    const uint8_t len_enc = (uint8_t)(length - MICROCODEC_LZSS_MIN_MATCH);

    if ((*writer->len + 2u) > writer->cap) {
        return MC_ERR_OVERFLOW;
    }

    writer->flag |= (uint8_t)(1u << writer->flag_bit);
    writer->buf[(*writer->len)++] = (uint8_t)(offset & 0xFFu);
    writer->buf[(*writer->len)++] = (uint8_t)((len_enc & 0x0Fu) |
                                              (uint8_t)(((uint16_t)offset >> 4u) & 0xF0u));
    writer->flag_bit++;
    if (writer->flag_bit == 8u) {
        return mc_lzss_writer_next_flag(writer);
    }
    return MC_OK;
}

static void mc_lzss_writer_flush(mc_lzss_writer_t *writer) {
    writer->buf[writer->flag_pos] = writer->flag;
}

static size_t mc_lzss_find_match(const uint8_t *src,
                                 size_t src_len,
                                 size_t pos,
                                 size_t *out_offset) {
    size_t best_len = 0u;
    size_t best_offset = 0u;
    size_t max_window = (pos < MICROCODEC_LZSS_WINDOW_SIZE) ? pos : MICROCODEC_LZSS_WINDOW_SIZE;
    size_t offset;

    for (offset = 1u; offset <= max_window; ++offset) {
        size_t match_len = 0u;

        while ((match_len < MICROCODEC_LZSS_MAX_MATCH) &&
               ((pos + match_len) < src_len) &&
               (src[pos - offset + match_len] == src[pos + match_len])) {
            match_len++;
        }

        if (match_len > best_len) {
            best_len = match_len;
            best_offset = offset;
            if (best_len == MICROCODEC_LZSS_MAX_MATCH) {
                break;
            }
        }
    }

    *out_offset = best_offset;
    return best_len;
}

static void mc_lzss_write_u32(uint8_t *dst, uint32_t value) {
    memcpy(dst, &value, sizeof(value));
}

static mc_err_t mc_lzss_read_u32(mc_slice_t src, uint32_t *value) {
    if ((src.ptr == NULL) || (value == NULL) || (src.len < sizeof(*value))) {
        return MC_ERR_CORRUPT;
    }
    memcpy(value, src.ptr, sizeof(*value));
    return MC_OK;
}

#if MICROCODEC_ENABLE_LZSS
void mc_lzss_ctx_init(mc_lzss_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    memset(ctx->window, 0, sizeof(ctx->window));
    ctx->window_pos = 0u;
}

mc_err_t mc_lzss_encode(mc_lzss_ctx_t *ctx, mc_slice_t src, mc_buf_t *dst) {
    mc_lzss_writer_t writer;
    size_t pos;

    if ((ctx == NULL) || (dst == NULL) || (dst->ptr == NULL) || ((src.ptr == NULL) && (src.len > 0u))) {
        return MC_ERR_INVALID;
    }
    if (src.len > 0xFFFFFFFFu) {
        return MC_ERR_INVALID;
    }
    if (dst->cap < MC_LZSS_HEADER_SIZE) {
        return MC_ERR_OVERFLOW;
    }

    dst->len = 0u;
    mc_lzss_write_u32(dst->ptr, (uint32_t)src.len);
    dst->len = MC_LZSS_HEADER_SIZE;

    if (src.len == 0u) {
        return MC_OK;
    }

    mc_lzss_writer_init(&writer, dst);

    pos = 0u;
    while (pos < src.len) {
        size_t offset = 0u;
        const size_t match_len = mc_lzss_find_match(src.ptr, src.len, pos, &offset);

        if (match_len >= MICROCODEC_LZSS_MIN_MATCH) {
            size_t i;
            mc_err_t err = mc_lzss_writer_backref(&writer, offset, match_len);
            if (err != MC_OK) {
                dst->len = 0u;
                return err;
            }
            for (i = 0u; i < match_len; ++i) {
                mc_lzss_update_window(ctx, src.ptr[pos + i]);
            }
            pos += match_len;
        } else {
            const mc_err_t err = mc_lzss_writer_literal(&writer, src.ptr[pos]);
            if (err != MC_OK) {
                dst->len = 0u;
                return err;
            }
            mc_lzss_update_window(ctx, src.ptr[pos]);
            pos++;
        }
    }

    mc_lzss_writer_flush(&writer);
    if (dst->len >= src.len) {
        return MC_ERR_INCOMPRESS;
    }
    return MC_OK;
}

mc_err_t mc_lzss_decode(mc_lzss_ctx_t *ctx, mc_slice_t src, mc_buf_t *dst) {
    uint32_t original_size_u32;
    size_t original_size;
    size_t out_pos;
    size_t in_pos;

    if ((ctx == NULL) || (dst == NULL) || (dst->ptr == NULL) || (src.ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if (src.len < MC_LZSS_HEADER_SIZE) {
        return MC_ERR_CORRUPT;
    }

    if (mc_lzss_read_u32(src, &original_size_u32) != MC_OK) {
        return MC_ERR_CORRUPT;
    }
    original_size = (size_t)original_size_u32;
    if (dst->cap < original_size) {
        return MC_ERR_OVERFLOW;
    }

    dst->len = 0u;
    in_pos = MC_LZSS_HEADER_SIZE;
    out_pos = 0u;

    while (out_pos < original_size) {
        uint8_t flag_byte;
        uint8_t bit;

        if (in_pos >= src.len) {
            return MC_ERR_CORRUPT;
        }

        flag_byte = src.ptr[in_pos++];
        for (bit = 0u; (bit < 8u) && (out_pos < original_size); ++bit) {
            if ((flag_byte & (uint8_t)(1u << bit)) == 0u) {
                if (in_pos >= src.len) {
                    return MC_ERR_CORRUPT;
                }
                dst->ptr[out_pos] = src.ptr[in_pos++];
                mc_lzss_update_window(ctx, dst->ptr[out_pos]);
                out_pos++;
            } else {
                size_t offset;
                size_t length;
                size_t i;

                if ((in_pos + 1u) >= src.len) {
                    return MC_ERR_CORRUPT;
                }

                offset = (size_t)src.ptr[in_pos] |
                         (size_t)((src.ptr[in_pos + 1u] & 0xF0u) << 4u);
                length = (size_t)(src.ptr[in_pos + 1u] & 0x0Fu) + MICROCODEC_LZSS_MIN_MATCH;
                in_pos += 2u;

                if ((offset == 0u) || (offset > out_pos) || ((out_pos + length) > original_size)) {
                    return MC_ERR_CORRUPT;
                }

                for (i = 0u; i < length; ++i) {
                    dst->ptr[out_pos] = dst->ptr[out_pos - offset];
                    mc_lzss_update_window(ctx, dst->ptr[out_pos]);
                    out_pos++;
                }
            }
        }
    }

    dst->len = original_size;
    return MC_OK;
}
#endif
