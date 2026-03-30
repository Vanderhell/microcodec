#include "microcodec.h"

#include <string.h>

#define MC_HUFF_MODE_STATIC 0x00u
#define MC_HUFF_MODE_ADAPTIVE 0x01u
#define MC_HUFF_MAX_NODES (MICROCODEC_HUFF_SYMBOLS * 2u)

typedef struct {
    uint32_t freq;
    int parent;
    int left;
    int right;
    int symbol;
    int active;
} mc_huff_node_t;

typedef struct {
    int child[2];
    int symbol;
} mc_huff_decode_node_t;

typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t *len;
    uint8_t accum;
    uint8_t bits;
} mc_huff_bitwriter_t;

typedef struct {
    const uint8_t *buf;
    size_t len;
    size_t pos;
    uint8_t accum;
    uint8_t bits_left;
} mc_huff_bitreader_t;

typedef struct {
    uint8_t symbol;
    uint8_t code_len;
} mc_huff_sort_entry_t;

static void mc_huff_fill_static_freq(uint32_t freq[MICROCODEC_HUFF_SYMBOLS]) {
    size_t i;

    freq[0] = 10000u;
    freq[1] = 5000u;
    freq[2] = 4000u;
    freq[3] = 3500u;
    for (i = 4u; i < 16u; ++i) {
        freq[i] = 3000u - (uint32_t)((i - 4u) * 150u);
    }
    for (i = 16u; i < 128u; ++i) {
        freq[i] = 800u - (uint32_t)(((i - 16u) * 700u) / 111u);
    }
    for (i = 128u; i < 256u; ++i) {
        freq[i] = 50u - (uint32_t)(((i - 128u) * 40u) / 127u);
    }
}

static uint16_t mc_huff_reverse_bits(uint16_t code, uint8_t code_len) {
    uint16_t reversed = 0u;
    uint8_t i;

    for (i = 0u; i < code_len; ++i) {
        reversed = (uint16_t)((reversed << 1u) | (uint16_t)(code & 1u));
        code >>= 1u;
    }

    return reversed;
}

static void mc_huff_bw_init(mc_huff_bitwriter_t *bw, mc_buf_t *dst) {
    bw->buf = dst->ptr;
    bw->cap = dst->cap;
    bw->len = &dst->len;
    bw->accum = 0u;
    bw->bits = 0u;
}

static mc_err_t mc_huff_bw_write_bits(mc_huff_bitwriter_t *bw, uint16_t code, uint8_t code_len) {
    uint8_t i;

    for (i = 0u; i < code_len; ++i) {
        bw->accum |= (uint8_t)(((code >> i) & 1u) << bw->bits);
        bw->bits++;
        if (bw->bits == 8u) {
            if (*bw->len >= bw->cap) {
                return MC_ERR_OVERFLOW;
            }
            bw->buf[(*bw->len)++] = bw->accum;
            bw->accum = 0u;
            bw->bits = 0u;
        }
    }

    return MC_OK;
}

static mc_err_t mc_huff_bw_flush(mc_huff_bitwriter_t *bw) {
    if (bw->bits > 0u) {
        if (*bw->len >= bw->cap) {
            return MC_ERR_OVERFLOW;
        }
        bw->buf[(*bw->len)++] = bw->accum;
    }
    return MC_OK;
}

static void mc_huff_br_init(mc_huff_bitreader_t *br, const uint8_t *buf, size_t len) {
    br->buf = buf;
    br->len = len;
    br->pos = 0u;
    br->accum = 0u;
    br->bits_left = 0u;
}

static mc_err_t mc_huff_br_read_bit(mc_huff_bitreader_t *br, uint8_t *bit) {
    if ((br == NULL) || (bit == NULL)) {
        return MC_ERR_INVALID;
    }
    if (br->bits_left == 0u) {
        if (br->pos >= br->len) {
            return MC_ERR_CORRUPT;
        }
        br->accum = br->buf[br->pos++];
        br->bits_left = 8u;
    }

    *bit = (uint8_t)(br->accum & 1u);
    br->accum >>= 1u;
    br->bits_left--;
    return MC_OK;
}

static int mc_huff_pick_min(const mc_huff_node_t *nodes, int count) {
    int best = -1;
    int i;

    for (i = 0; i < count; ++i) {
        if (nodes[i].active == 0) {
            continue;
        }
        if ((best < 0) ||
            (nodes[i].freq < nodes[best].freq) ||
            ((nodes[i].freq == nodes[best].freq) && (nodes[i].symbol < nodes[best].symbol))) {
            best = i;
        }
    }

    return best;
}

static void mc_huff_compute_lengths(const mc_huff_node_t *nodes,
                                    int node,
                                    uint8_t depth,
                                    uint8_t lengths[MICROCODEC_HUFF_SYMBOLS]) {
    if ((nodes[node].left < 0) && (nodes[node].right < 0)) {
        lengths[(size_t)nodes[node].symbol] = (depth == 0u) ? 1u : depth;
        return;
    }
    if (nodes[node].left >= 0) {
        mc_huff_compute_lengths(nodes, nodes[node].left, (uint8_t)(depth + 1u), lengths);
    }
    if (nodes[node].right >= 0) {
        mc_huff_compute_lengths(nodes, nodes[node].right, (uint8_t)(depth + 1u), lengths);
    }
}

static void mc_huff_sort_entries(mc_huff_sort_entry_t *entries, size_t count) {
    size_t i;
    size_t j;

    for (i = 0u; i < count; ++i) {
        for (j = i + 1u; j < count; ++j) {
            if ((entries[j].code_len < entries[i].code_len) ||
                ((entries[j].code_len == entries[i].code_len) &&
                 (entries[j].symbol < entries[i].symbol))) {
                const mc_huff_sort_entry_t tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
}

static mc_err_t mc_huff_build_decode_tree(const mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS],
                                          mc_huff_decode_node_t nodes[MC_HUFF_MAX_NODES]) {
    size_t i;
    int next_node = 1;

    for (i = 0u; i < MC_HUFF_MAX_NODES; ++i) {
        nodes[i].child[0] = -1;
        nodes[i].child[1] = -1;
        nodes[i].symbol = -1;
    }

    for (i = 0u; i < MICROCODEC_HUFF_SYMBOLS; ++i) {
        uint8_t bit_index;
        int cursor = 0;

        if (table[i].code_len == 0u) {
            continue;
        }

        for (bit_index = 0u; bit_index < table[i].code_len; ++bit_index) {
            const int bit = (int)((table[i].code >> bit_index) & 1u);
            if (nodes[cursor].child[bit] < 0) {
                if (next_node >= (int)MC_HUFF_MAX_NODES) {
                    return MC_ERR_CORRUPT;
                }
                nodes[cursor].child[bit] = next_node++;
            }
            cursor = nodes[cursor].child[bit];
        }

        if (nodes[cursor].symbol >= 0) {
            return MC_ERR_CORRUPT;
        }
        nodes[cursor].symbol = (int)i;
    }

    return MC_OK;
}

static mc_err_t mc_huff_encode_payload(mc_slice_t src,
                                       const mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS],
                                       mc_buf_t *dst) {
    mc_huff_bitwriter_t bw;
    size_t i;
    mc_err_t err;

    mc_huff_bw_init(&bw, dst);
    for (i = 0u; i < src.len; ++i) {
        const mc_huff_entry_t entry = table[src.ptr[i]];
        if (entry.code_len == 0u) {
            return MC_ERR_CORRUPT;
        }
        err = mc_huff_bw_write_bits(&bw, entry.code, entry.code_len);
        if (err != MC_OK) {
            return err;
        }
    }

    return mc_huff_bw_flush(&bw);
}

static mc_err_t mc_huff_decode_payload(const uint8_t *payload,
                                       size_t payload_len,
                                       const mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS],
                                       mc_buf_t *dst,
                                       size_t original_size) {
    mc_huff_decode_node_t nodes[MC_HUFF_MAX_NODES];
    mc_huff_bitreader_t br;
    size_t out_pos = 0u;
    mc_err_t err;

    if (dst->cap < original_size) {
        return MC_ERR_OVERFLOW;
    }

    err = mc_huff_build_decode_tree(table, nodes);
    if (err != MC_OK) {
        return err;
    }

    mc_huff_br_init(&br, payload, payload_len);
    while (out_pos < original_size) {
        int cursor = 0;
        while (nodes[cursor].symbol < 0) {
            uint8_t bit = 0u;
            err = mc_huff_br_read_bit(&br, &bit);
            if (err != MC_OK) {
                return err;
            }
            cursor = nodes[cursor].child[bit];
            if (cursor < 0) {
                return MC_ERR_CORRUPT;
            }
        }
        dst->ptr[out_pos++] = (uint8_t)nodes[cursor].symbol;
    }

    dst->len = original_size;
    return MC_OK;
}

static mc_err_t mc_huff_prepare_static_table(mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS]) {
    uint32_t freq[MICROCODEC_HUFF_SYMBOLS];
    mc_huff_fill_static_freq(freq);
    return mc_huff_build_table(freq, table);
}

mc_err_t mc_huff_build_table(const uint32_t freq[MICROCODEC_HUFF_SYMBOLS],
                             mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS]) {
    mc_huff_node_t nodes[MC_HUFF_MAX_NODES];
    mc_huff_sort_entry_t entries[MICROCODEC_HUFF_SYMBOLS];
    uint8_t lengths[MICROCODEC_HUFF_SYMBOLS];
    size_t entry_count = 0u;
    int node_count = 0;
    int root;
    uint16_t code = 0u;
    uint8_t prev_len = 0u;
    size_t i;

    if ((freq == NULL) || (table == NULL)) {
        return MC_ERR_INVALID;
    }

    memset(table, 0, sizeof(mc_huff_entry_t) * MICROCODEC_HUFF_SYMBOLS);
    memset(lengths, 0, sizeof(lengths));

    for (i = 0u; i < MICROCODEC_HUFF_SYMBOLS; ++i) {
        if (freq[i] > 0u) {
            nodes[node_count].freq = freq[i];
            nodes[node_count].parent = -1;
            nodes[node_count].left = -1;
            nodes[node_count].right = -1;
            nodes[node_count].symbol = (int)i;
            nodes[node_count].active = 1;
            node_count++;
        }
    }

    if (node_count == 0) {
        return MC_ERR_INVALID;
    }
    if (node_count == 1) {
        for (i = 0u; i < MICROCODEC_HUFF_SYMBOLS; ++i) {
            if (freq[i] > 0u) {
                table[i].code = 0u;
                table[i].code_len = 1u;
                return MC_OK;
            }
        }
    }

    while (1) {
        const int first = mc_huff_pick_min(nodes, node_count);
        int second;

        nodes[first].active = 0;
        second = mc_huff_pick_min(nodes, node_count);
        if (second < 0) {
            root = first;
            break;
        }
        nodes[second].active = 0;

        nodes[node_count].freq = nodes[first].freq + nodes[second].freq;
        nodes[node_count].parent = -1;
        nodes[node_count].left = first;
        nodes[node_count].right = second;
        nodes[node_count].symbol = (nodes[first].symbol < nodes[second].symbol) ?
            nodes[first].symbol : nodes[second].symbol;
        nodes[node_count].active = 1;
        nodes[first].parent = node_count;
        nodes[second].parent = node_count;
        node_count++;
    }

    mc_huff_compute_lengths(nodes, root, 0u, lengths);

    for (i = 0u; i < MICROCODEC_HUFF_SYMBOLS; ++i) {
        if (lengths[i] > 0u) {
            if (lengths[i] > MICROCODEC_HUFF_MAX_CODE_LEN) {
                return MC_ERR_CORRUPT;
            }
            entries[entry_count].symbol = (uint8_t)i;
            entries[entry_count].code_len = lengths[i];
            entry_count++;
        }
    }

    mc_huff_sort_entries(entries, entry_count);

    for (i = 0u; i < entry_count; ++i) {
        if (i == 0u) {
            code = 0u;
            prev_len = entries[i].code_len;
        } else {
            code = (uint16_t)(code + 1u);
            if (entries[i].code_len > prev_len) {
                code = (uint16_t)(code << (entries[i].code_len - prev_len));
                prev_len = entries[i].code_len;
            }
        }
        table[entries[i].symbol].code = mc_huff_reverse_bits(code, entries[i].code_len);
        table[entries[i].symbol].code_len = entries[i].code_len;
    }

    return MC_OK;
}

mc_err_t mc_huff_encode_static(mc_slice_t src, mc_buf_t *dst) {
    mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS];
    mc_err_t err;

    if ((dst == NULL) || (dst->ptr == NULL) || ((src.ptr == NULL) && (src.len > 0u))) {
        return MC_ERR_INVALID;
    }

    err = mc_huff_prepare_static_table(table);
    if (err != MC_OK) {
        return err;
    }

    dst->len = 0u;
    if (dst->cap < 5u) {
        return MC_ERR_OVERFLOW;
    }
    dst->ptr[dst->len++] = MC_HUFF_MODE_STATIC;
    err = mc_huff_encode_payload(src, table, dst);
    if (err != MC_OK) {
        dst->len = 0u;
        return err;
    }
    if ((dst->len + 4u) > dst->cap) {
        dst->len = 0u;
        return MC_ERR_OVERFLOW;
    }
    memcpy(dst->ptr + dst->len, &src.len, sizeof(uint32_t));
    dst->len += 4u;

    if ((src.len > 0u) && (dst->len >= src.len)) {
        dst->len = 0u;
        return MC_ERR_INCOMPRESS;
    }

    return MC_OK;
}

mc_err_t mc_huff_decode_static(mc_slice_t src, mc_buf_t *dst) {
    mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS];
    uint32_t original_size_u32;
    mc_err_t err;

    if ((src.ptr == NULL) || (dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if ((src.len < 5u) || (src.ptr[0] != MC_HUFF_MODE_STATIC)) {
        return MC_ERR_CORRUPT;
    }

    err = mc_huff_prepare_static_table(table);
    if (err != MC_OK) {
        return err;
    }
    memcpy(&original_size_u32, src.ptr + src.len - 4u, sizeof(original_size_u32));
    return mc_huff_decode_payload(src.ptr + 1u, src.len - 5u, table, dst, (size_t)original_size_u32);
}

mc_err_t mc_huff_encode_adaptive(mc_slice_t src,
                                 const mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS],
                                 mc_buf_t *dst) {
    uint16_t symbol_count = 0u;
    size_t i;
    mc_err_t err;

    if ((table == NULL) || (dst == NULL) || (dst->ptr == NULL) || ((src.ptr == NULL) && (src.len > 0u))) {
        return MC_ERR_INVALID;
    }

    dst->len = 0u;
    if (dst->cap < 7u) {
        return MC_ERR_OVERFLOW;
    }
    dst->ptr[dst->len++] = MC_HUFF_MODE_ADAPTIVE;

    for (i = 0u; i < MICROCODEC_HUFF_SYMBOLS; ++i) {
        if (table[i].code_len > 0u) {
            symbol_count++;
        }
    }
    memcpy(dst->ptr + dst->len, &symbol_count, sizeof(symbol_count));
    dst->len += sizeof(symbol_count);

    for (i = 0u; i < MICROCODEC_HUFF_SYMBOLS; ++i) {
        const size_t code_bytes = (size_t)((table[i].code_len + 7u) / 8u);
        if (table[i].code_len == 0u) {
            continue;
        }
        if ((dst->len + 2u + code_bytes) > dst->cap) {
            dst->len = 0u;
            return MC_ERR_OVERFLOW;
        }
        dst->ptr[dst->len++] = (uint8_t)i;
        dst->ptr[dst->len++] = table[i].code_len;
        memcpy(dst->ptr + dst->len, &table[i].code, code_bytes);
        dst->len += code_bytes;
    }

    err = mc_huff_encode_payload(src, table, dst);
    if (err != MC_OK) {
        dst->len = 0u;
        return err;
    }
    if ((dst->len + 4u) > dst->cap) {
        dst->len = 0u;
        return MC_ERR_OVERFLOW;
    }
    memcpy(dst->ptr + dst->len, &src.len, sizeof(uint32_t));
    dst->len += 4u;
    return MC_OK;
}

mc_err_t mc_huff_decode_adaptive(mc_slice_t src, mc_buf_t *dst) {
    mc_huff_entry_t table[MICROCODEC_HUFF_SYMBOLS];
    uint16_t symbol_count;
    uint32_t original_size_u32;
    size_t offset = 0u;
    size_t i;

    if ((src.ptr == NULL) || (dst == NULL) || (dst->ptr == NULL)) {
        return MC_ERR_INVALID;
    }
    if ((src.len < 7u) || (src.ptr[0] != MC_HUFF_MODE_ADAPTIVE)) {
        return MC_ERR_CORRUPT;
    }

    memset(table, 0, sizeof(table));
    offset = 1u;
    memcpy(&symbol_count, src.ptr + offset, sizeof(symbol_count));
    offset += sizeof(symbol_count);

    for (i = 0u; i < symbol_count; ++i) {
        const size_t code_bytes = (size_t)((src.ptr[offset + 1u] + 7u) / 8u);
        uint16_t code = 0u;
        uint8_t symbol;
        uint8_t code_len;

        if ((offset + 2u) > src.len) {
            return MC_ERR_CORRUPT;
        }
        symbol = src.ptr[offset++];
        code_len = src.ptr[offset++];
        if ((code_len == 0u) || (code_len > MICROCODEC_HUFF_MAX_CODE_LEN) ||
            ((offset + code_bytes) > src.len)) {
            return MC_ERR_CORRUPT;
        }
        memcpy(&code, src.ptr + offset, code_bytes);
        table[symbol].code = code;
        table[symbol].code_len = code_len;
        offset += code_bytes;
    }

    if ((src.len - offset) < 4u) {
        return MC_ERR_CORRUPT;
    }
    memcpy(&original_size_u32, src.ptr + src.len - 4u, sizeof(original_size_u32));
    return mc_huff_decode_payload(src.ptr + offset, src.len - offset - 4u,
                                  table, dst, (size_t)original_size_u32);
}
