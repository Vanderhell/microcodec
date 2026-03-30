#define MTEST_IMPLEMENTATION
#include "mtest.h"
#include "microcodec.h"

#include <string.h>

static uint8_t g_src[4096];
static uint8_t g_enc[16384];
static uint8_t g_dec[4096];
static uint32_t g_freq[256];
static mc_huff_entry_t g_table[256];

static void setup(void) {
    memset(g_src, 0, sizeof(g_src));
    memset(g_enc, 0, sizeof(g_enc));
    memset(g_dec, 0, sizeof(g_dec));
    memset(g_freq, 0, sizeof(g_freq));
    memset(g_table, 0, sizeof(g_table));
}

static void teardown(void) {
}

static void build_freq_from_src(size_t len) {
    size_t i;
    for (i = 0u; i < len; ++i) {
        g_freq[g_src[i]]++;
    }
}

static int prefix_free_(const mc_huff_entry_t *table) {
    size_t i;
    size_t j;

    for (i = 0u; i < 256u; ++i) {
        if (table[i].code_len == 0u) {
            continue;
        }
        for (j = 0u; j < 256u; ++j) {
            uint16_t short_code;
            uint16_t long_code;
            uint8_t short_len;
            uint8_t long_len;

            if ((i == j) || (table[j].code_len == 0u)) {
                continue;
            }
            if (table[i].code_len <= table[j].code_len) {
                short_code = table[i].code;
                short_len = table[i].code_len;
                long_code = table[j].code;
                long_len = table[j].code_len;
            } else {
                short_code = table[j].code;
                short_len = table[j].code_len;
                long_code = table[i].code;
                long_len = table[i].code_len;
            }
            if ((short_len < long_len) &&
                ((uint16_t)(long_code & (uint16_t)((1u << short_len) - 1u)) == short_code)) {
                return 0;
            }
        }
    }

    return 1;
}

MTEST(huff_static_encode_empty_ok) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_static(MC_SLICE(g_src, 0u), &dst));
}

MTEST(huff_static_roundtrip_compressible) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    memset(g_src, 0x00, 128u);
    g_src[32] = 0x01u;
    g_src[96] = 0x02u;
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_static(MC_SLICE(g_src, 128u), &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_decode_static(MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 128u);
}

MTEST(huff_static_noncompressible_incompress) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    size_t i;
    for (i = 0u; i < 128u; ++i) {
        g_src[i] = (uint8_t)i;
    }
    MTEST_ASSERT_EQ(MC_ERR_INCOMPRESS, mc_huff_encode_static(MC_SLICE(g_src, 128u), &dst));
}

MTEST(huff_static_wrong_mode_corrupt) {
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    memset(g_enc, 0, 8u);
    g_enc[0] = 0x01u;
    MTEST_ASSERT_EQ(MC_ERR_CORRUPT, mc_huff_decode_static(MC_SLICE(g_enc, 8u), &dec));
}

MTEST(huff_build_table_all_zero_invalid) {
    MTEST_ASSERT_EQ(MC_ERR_INVALID, mc_huff_build_table(g_freq, g_table));
}

MTEST(huff_build_table_single_symbol_len1) {
    g_freq[7] = 10u;
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(1, (int)g_table[7].code_len);
}

MTEST(huff_build_table_two_equal_symbols_len1) {
    g_freq[10] = 5u;
    g_freq[20] = 5u;
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(1, (int)g_table[10].code_len);
    MTEST_ASSERT_EQ(1, (int)g_table[20].code_len);
}

MTEST(huff_adaptive_roundtrip) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    memset(g_src, 0xAA, 64u);
    memset(g_src + 40u, 0xBB, 8u);
    build_freq_from_src(64u);
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_adaptive(MC_SLICE(g_src, 64u), g_table, &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_decode_adaptive(MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 64u);
}

MTEST(huff_adaptive_reads_embedded_table) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    memset(g_src, 0x11, 32u);
    memset(g_src + 24u, 0x22, 8u);
    build_freq_from_src(32u);
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_adaptive(MC_SLICE(g_src, 32u), g_table, &dst));
    MTEST_ASSERT_TRUE(g_enc[0] == 0x01u);
    MTEST_ASSERT_EQ(MC_OK, mc_huff_decode_adaptive(MC_SLICE(g_enc, dst.len), &dec));
}

MTEST(huff_adaptive_uniform_near_8_bits) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    size_t i;
    for (i = 0u; i < 256u; ++i) {
        g_src[i] = (uint8_t)i;
        g_freq[i] = 1u;
    }
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_adaptive(MC_SLICE(g_src, 256u), g_table, &dst));
    MTEST_ASSERT_TRUE(dst.len > 200u);
}

MTEST(huff_adaptive_skewed_compresses) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    memset(g_src, 0x00, 256u);
    memset(g_src + 200u, 0x01, 32u);
    build_freq_from_src(256u);
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_adaptive(MC_SLICE(g_src, 256u), g_table, &dst));
    MTEST_ASSERT_TRUE(dst.len < 256u);
}

MTEST(huff_adaptive_corrupt_table) {
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    memset(g_enc, 0, 8u);
    g_enc[0] = 0x01u;
    g_enc[1] = 0x01u;
    g_enc[2] = 0x00u;
    g_enc[3] = 0x44u;
    g_enc[4] = 0x00u;
    MTEST_ASSERT_EQ(MC_ERR_CORRUPT, mc_huff_decode_adaptive(MC_SLICE(g_enc, 8u), &dec));
}

MTEST(huff_encode_overflow) {
    uint8_t tiny[2];
    mc_buf_t dst = MC_BUF(tiny, sizeof(tiny));
    memset(g_src, 0x00, 10u);
    MTEST_ASSERT_EQ(MC_ERR_OVERFLOW, mc_huff_encode_static(MC_SLICE(g_src, 10u), &dst));
}

MTEST(huff_decode_overflow) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    uint8_t tiny[4];
    mc_buf_t dec = MC_BUF(tiny, sizeof(tiny));
    memset(g_src, 0x00, 64u);
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_static(MC_SLICE(g_src, 64u), &dst));
    MTEST_ASSERT_EQ(MC_ERR_OVERFLOW, mc_huff_decode_static(MC_SLICE(g_enc, dst.len), &dec));
}

MTEST(huff_null_pointers) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    MTEST_ASSERT_EQ(MC_ERR_INVALID, mc_huff_encode_static(MC_SLICE(g_src, 1u), NULL));
    MTEST_ASSERT_EQ(MC_ERR_INVALID, mc_huff_decode_static(MC_SLICE(g_enc, 1u), NULL));
    MTEST_ASSERT_EQ(MC_ERR_INVALID, mc_huff_build_table(NULL, g_table));
    MTEST_ASSERT_EQ(MC_ERR_INVALID, mc_huff_encode_adaptive(MC_SLICE(g_src, 1u), NULL, &dst));
}

MTEST(huff_max_encoded_size_covers_actual) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    memset(g_src, 0x00, 128u);
    build_freq_from_src(128u);
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_adaptive(MC_SLICE(g_src, 128u), g_table, &dst));
    MTEST_ASSERT_TRUE(mc_huff_max_encoded_size(128u) >= dst.len);
}

MTEST(huff_code_lengths_within_limit) {
    size_t i;
    for (i = 0u; i < 256u; ++i) {
        g_freq[i] = (uint32_t)(i + 1u);
    }
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    for (i = 0u; i < 256u; ++i) {
        MTEST_ASSERT_TRUE(g_table[i].code_len <= MICROCODEC_HUFF_MAX_CODE_LEN);
    }
}

MTEST(huff_codes_prefix_free) {
    size_t i;
    for (i = 0u; i < 32u; ++i) {
        g_freq[i] = (uint32_t)(32u - i);
    }
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_TRUE(prefix_free_(g_table) == 1);
}

MTEST(huff_decode_padded_bits_ignored) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    memset(g_src, 0x00, 9u);
    build_freq_from_src(9u);
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_adaptive(MC_SLICE(g_src, 9u), g_table, &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_decode_adaptive(MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 9u);
}

MTEST(huff_large_input_roundtrip) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    size_t i;
    for (i = 0u; i < 4096u; ++i) {
        g_src[i] = (uint8_t)((i % 64u) == 0u ? 0x00u : (i % 4u));
    }
    build_freq_from_src(4096u);
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_adaptive(MC_SLICE(g_src, 4096u), g_table, &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_decode_adaptive(MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 4096u);
}

MTEST(huff_zero_freq_symbols_absent) {
    size_t i;
    g_freq[0xAA] = 10u;
    g_freq[0xBB] = 1u;
    MTEST_ASSERT_EQ(MC_OK, mc_huff_build_table(g_freq, g_table));
    for (i = 0u; i < 256u; ++i) {
        if ((i != 0xAAu) && (i != 0xBBu)) {
            MTEST_ASSERT_EQ(0, (int)g_table[i].code_len);
        }
    }
}

MTEST_SUITE(huff) {
    MTEST_RUN_F(huff_static_encode_empty_ok, setup, teardown);
    MTEST_RUN_F(huff_static_roundtrip_compressible, setup, teardown);
    MTEST_RUN_F(huff_static_noncompressible_incompress, setup, teardown);
    MTEST_RUN_F(huff_static_wrong_mode_corrupt, setup, teardown);
    MTEST_RUN_F(huff_build_table_all_zero_invalid, setup, teardown);
    MTEST_RUN_F(huff_build_table_single_symbol_len1, setup, teardown);
    MTEST_RUN_F(huff_build_table_two_equal_symbols_len1, setup, teardown);
    MTEST_RUN_F(huff_adaptive_roundtrip, setup, teardown);
    MTEST_RUN_F(huff_adaptive_reads_embedded_table, setup, teardown);
    MTEST_RUN_F(huff_adaptive_uniform_near_8_bits, setup, teardown);
    MTEST_RUN_F(huff_adaptive_skewed_compresses, setup, teardown);
    MTEST_RUN_F(huff_adaptive_corrupt_table, setup, teardown);
    MTEST_RUN_F(huff_encode_overflow, setup, teardown);
    MTEST_RUN_F(huff_decode_overflow, setup, teardown);
    MTEST_RUN_F(huff_null_pointers, setup, teardown);
    MTEST_RUN_F(huff_max_encoded_size_covers_actual, setup, teardown);
    MTEST_RUN_F(huff_code_lengths_within_limit, setup, teardown);
    MTEST_RUN_F(huff_codes_prefix_free, setup, teardown);
    MTEST_RUN_F(huff_decode_padded_bits_ignored, setup, teardown);
    MTEST_RUN_F(huff_large_input_roundtrip, setup, teardown);
    MTEST_RUN_F(huff_zero_freq_symbols_absent, setup, teardown);
}

int main(int argc, char **argv) {
    MTEST_BEGIN(argc, argv);
    MTEST_SUITE_RUN(huff);
    return MTEST_END();
}
