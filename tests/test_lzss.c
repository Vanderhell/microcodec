#define MTEST_IMPLEMENTATION
#include "mtest.h"
#include "microcodec.h"

#include <string.h>

static uint8_t g_src[4096];
static uint8_t g_enc[8192];
static uint8_t g_dec[4096];

static void setup(void) {
    memset(g_src, 0, sizeof(g_src));
    memset(g_enc, 0, sizeof(g_enc));
    memset(g_dec, 0, sizeof(g_dec));
}

static void teardown(void) {
}

MTEST(lzss_encode_noncompressible_returns_incompress) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    size_t i;

    mc_lzss_ctx_init(&ctx);
    for (i = 0u; i < 64u; ++i) {
        g_src[i] = (uint8_t)i;
    }

    MTEST_ASSERT_EQ(MC_ERR_INCOMPRESS, mc_lzss_encode(&ctx, MC_SLICE(g_src, 64u), &dst));
}

MTEST(lzss_encode_repetitive_compresses) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));

    mc_lzss_ctx_init(&ctx);
    memset(g_src, 'A', 128u);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_src, 128u), &dst));
    MTEST_ASSERT_TRUE(dst.len < 128u);
}

MTEST(lzss_roundtrip_encode_decode) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    const char *pattern = "abcabcabcabcabcabcabcabc";

    memcpy(g_src, pattern, 24u);
    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_src, 24u), &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_EQ(24, (int)dec.len);
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 24u);
}

MTEST(lzss_empty_input_header_only) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));

    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_src, 0u), &dst));
    MTEST_ASSERT_EQ(4, (int)dst.len);
}

MTEST(lzss_single_byte_incompress) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));

    g_src[0] = 0x42u;
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_ERR_INCOMPRESS, mc_lzss_encode(&ctx, MC_SLICE(g_src, 1u), &dst));
}

MTEST(lzss_shorter_than_min_match_incompress) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));

    g_src[0] = 'x';
    g_src[1] = 'y';
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_ERR_INCOMPRESS, mc_lzss_encode(&ctx, MC_SLICE(g_src, 2u), &dst));
}

MTEST(lzss_backref_to_position_zero_roundtrip) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));

    memcpy(g_src, "aaaaabaaaaab", 12u);
    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_src, 12u), &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 12u);
}

MTEST(lzss_multiple_backrefs_roundtrip) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));

    memcpy(g_src, "abcdabcdxxxxabcdabcd", 20u);
    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_src, 20u), &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 20u);
}

MTEST(lzss_window_wrap_roundtrip) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    size_t i;

    for (i = 0u; i < 600u; ++i) {
        g_src[i] = (uint8_t)("abcd"[i % 4u]);
    }

    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_src, 600u), &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 600u);
}

MTEST(lzss_input_larger_than_window_roundtrip) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    size_t i;

    for (i = 0u; i < 512u; ++i) {
        g_src[i] = (uint8_t)("sensor-packet-"[i % 14u]);
    }

    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_src, 512u), &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 512u);
}

MTEST(lzss_reinit_ctx_clean_state) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst1 = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dst2 = MC_BUF(g_enc + 1024, sizeof(g_enc) - 1024u);

    memset(g_src, 'Q', 64u);
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_src, 64u), &dst1));
    mc_lzss_ctx_init(&ctx);
    memset(g_src, 'Q', 64u);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_src, 64u), &dst2));
    MTEST_ASSERT_EQ((int)dst1.len, (int)dst2.len);
}

MTEST(lzss_decode_corrupt_missing_literal) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));

    mc_lzss_ctx_init(&ctx);
    memset(g_enc, 0, 5u);
    g_enc[0] = 1u;
    MTEST_ASSERT_EQ(MC_ERR_CORRUPT, mc_lzss_decode(&ctx, MC_SLICE(g_enc, 5u), &dec));
}

MTEST(lzss_decode_backref_past_available_data) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));

    mc_lzss_ctx_init(&ctx);
    memset(g_enc, 0, 7u);
    g_enc[0] = 3u;
    g_enc[4] = 1u;
    g_enc[5] = 5u;
    g_enc[6] = 0u;
    MTEST_ASSERT_EQ(MC_ERR_CORRUPT, mc_lzss_decode(&ctx, MC_SLICE(g_enc, 7u), &dec));
}

MTEST(lzss_decode_overflow) {
    mc_lzss_ctx_t ctx;
    uint8_t small[4];
    mc_buf_t dec = MC_BUF(small, sizeof(small));

    mc_lzss_ctx_init(&ctx);
    memset(g_enc, 0, 5u);
    g_enc[0] = 5u;
    MTEST_ASSERT_EQ(MC_ERR_OVERFLOW, mc_lzss_decode(&ctx, MC_SLICE(g_enc, 5u), &dec));
}

MTEST(lzss_encode_overflow) {
    mc_lzss_ctx_t ctx;
    uint8_t tiny[8];
    mc_buf_t dst = MC_BUF(tiny, sizeof(tiny));

    memset(g_src, 'A', 64u);
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_ERR_OVERFLOW, mc_lzss_encode(&ctx, MC_SLICE(g_src, 64u), &dst));
}

MTEST(lzss_null_ctx_invalid) {
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    MTEST_ASSERT_EQ(MC_ERR_INVALID, mc_lzss_encode(NULL, MC_SLICE(g_src, 1u), &dst));
}

MTEST(lzss_null_src_invalid) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_slice_t bad = { NULL, 4u };

    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_ERR_INVALID, mc_lzss_encode(&ctx, bad, &dst));
}

MTEST(lzss_256_identical_high_compression) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));

    memset(g_src, 0xAA, 256u);
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_src, 256u), &dst));
    MTEST_ASSERT_TRUE(dst.len < 64u);
}

MTEST(lzss_1024_repeating_pattern_better_than_half) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    size_t i;

    for (i = 0u; i < 1024u; ++i) {
        g_src[i] = (uint8_t)("ABCD1234"[i % 8u]);
    }

    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_src, 1024u), &dst));
    MTEST_ASSERT_TRUE(dst.len < 512u);
}

MTEST(lzss_max_encoded_size_covers_actual) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));

    memset(g_src, 'Z', 128u);
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_src, 128u), &dst));
    MTEST_ASSERT_TRUE(mc_lzss_max_encoded_size(128u) >= dst.len);
}

MTEST(lzss_decode_exact_capacity_ok) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    uint8_t out[64];
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(out, sizeof(out));

    memset(g_src, 'R', 64u);
    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_src, 64u), &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_EQ(64, (int)dec.len);
}

MTEST(lzss_header_original_size_matches_output) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    uint32_t original_size = 0u;

    memset(g_src, 'M', 80u);
    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_src, 80u), &dst));
    memcpy(&original_size, g_enc, sizeof(original_size));
    MTEST_ASSERT_EQ(80, (int)original_size);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_EQ(80, (int)dec.len);
}

MTEST(lzss_stress_randomish_roundtrip) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t dst = MC_BUF(g_enc, sizeof(g_enc));
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    size_t i;

    for (i = 0u; i < 128u; ++i) {
        g_src[i] = (uint8_t)(((i * 17u) ^ (i >> 1u) ^ 0x5Au) & 0xFFu);
    }
    for (i = 128u; i < 4096u; i += 128u) {
        memcpy(g_src + i, g_src, 128u);
        g_src[i + ((i / 128u) % 64u)] ^= (uint8_t)(i & 0x1Fu);
    }

    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_src, 4096u), &dst));
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_enc, dst.len), &dec));
    MTEST_ASSERT_MEM_EQ(g_src, g_dec, 4096u);
}

MTEST(lzss_decode_truncated_backref_corrupt) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));

    mc_lzss_ctx_init(&ctx);
    memset(g_enc, 0, 6u);
    g_enc[0] = 6u;
    g_enc[4] = 1u;
    g_enc[5] = 1u;
    MTEST_ASSERT_EQ(MC_ERR_CORRUPT, mc_lzss_decode(&ctx, MC_SLICE(g_enc, 6u), &dec));
}

MTEST(lzss_decode_bad_header_corrupt) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));

    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_ERR_CORRUPT, mc_lzss_decode(&ctx, MC_SLICE(g_enc, 3u), &dec));
}

MTEST(lzss_encode_zero_length_with_small_dst) {
    mc_lzss_ctx_t ctx;
    uint8_t tiny[4];
    mc_buf_t dst = MC_BUF(tiny, sizeof(tiny));

    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_src, 0u), &dst));
    MTEST_ASSERT_EQ(4, (int)dst.len);
}

MTEST(lzss_decode_zero_length_stream) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));

    memset(g_enc, 0, 4u);
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&ctx, MC_SLICE(g_enc, 4u), &dec));
    MTEST_ASSERT_EQ(0, (int)dec.len);
}

MTEST_SUITE(lzss) {
    MTEST_RUN_F(lzss_encode_noncompressible_returns_incompress, setup, teardown);
    MTEST_RUN_F(lzss_encode_repetitive_compresses, setup, teardown);
    MTEST_RUN_F(lzss_roundtrip_encode_decode, setup, teardown);
    MTEST_RUN_F(lzss_empty_input_header_only, setup, teardown);
    MTEST_RUN_F(lzss_single_byte_incompress, setup, teardown);
    MTEST_RUN_F(lzss_shorter_than_min_match_incompress, setup, teardown);
    MTEST_RUN_F(lzss_backref_to_position_zero_roundtrip, setup, teardown);
    MTEST_RUN_F(lzss_multiple_backrefs_roundtrip, setup, teardown);
    MTEST_RUN_F(lzss_window_wrap_roundtrip, setup, teardown);
    MTEST_RUN_F(lzss_input_larger_than_window_roundtrip, setup, teardown);
    MTEST_RUN_F(lzss_reinit_ctx_clean_state, setup, teardown);
    MTEST_RUN_F(lzss_decode_corrupt_missing_literal, setup, teardown);
    MTEST_RUN_F(lzss_decode_backref_past_available_data, setup, teardown);
    MTEST_RUN_F(lzss_decode_overflow, setup, teardown);
    MTEST_RUN_F(lzss_encode_overflow, setup, teardown);
    MTEST_RUN_F(lzss_null_ctx_invalid, setup, teardown);
    MTEST_RUN_F(lzss_null_src_invalid, setup, teardown);
    MTEST_RUN_F(lzss_256_identical_high_compression, setup, teardown);
    MTEST_RUN_F(lzss_1024_repeating_pattern_better_than_half, setup, teardown);
    MTEST_RUN_F(lzss_max_encoded_size_covers_actual, setup, teardown);
    MTEST_RUN_F(lzss_decode_exact_capacity_ok, setup, teardown);
    MTEST_RUN_F(lzss_header_original_size_matches_output, setup, teardown);
    MTEST_RUN_F(lzss_stress_randomish_roundtrip, setup, teardown);
    MTEST_RUN_F(lzss_decode_truncated_backref_corrupt, setup, teardown);
    MTEST_RUN_F(lzss_decode_bad_header_corrupt, setup, teardown);
    MTEST_RUN_F(lzss_encode_zero_length_with_small_dst, setup, teardown);
    MTEST_RUN_F(lzss_decode_zero_length_stream, setup, teardown);
}

int main(int argc, char **argv) {
    MTEST_BEGIN(argc, argv);
    MTEST_SUITE_RUN(lzss);
    return MTEST_END();
}
