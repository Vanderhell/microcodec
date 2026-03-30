#define MTEST_IMPLEMENTATION
#include "mtest.h"
#include "microcodec.h"

#include <string.h>

static uint8_t g_src[4096];
static uint8_t g_dst[8192];
static uint8_t g_dec[4096];

static void setup(void) {
    memset(g_src, 0, sizeof(g_src));
    memset(g_dst, 0, sizeof(g_dst));
    memset(g_dec, 0, sizeof(g_dec));
}

static void teardown(void) {
}

MTEST(rle_encode_empty) {
    mc_buf_t dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 0u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 0u);
}

MTEST(rle_encode_single_byte) {
    mc_buf_t dst;

    g_src[0] = 0xABu;
    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 1u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 2u);
    MTEST_ASSERT_EQ(g_dst[0], 0u);
    MTEST_ASSERT_EQ(g_dst[1], 0xABu);
}

MTEST(rle_encode_128_identical_bytes) {
    mc_buf_t dst;

    memset(g_src, 0x11, 128u);
    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 128u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 2u);
    MTEST_ASSERT_EQ(g_dst[0], 0xFFu);
    MTEST_ASSERT_EQ(g_dst[1], 0x11u);
}

MTEST(rle_encode_129_identical_bytes) {
    mc_buf_t dst;

    memset(g_src, 0x22, 129u);
    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 129u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 4u);
}

MTEST(rle_encode_all_different_bytes) {
    mc_buf_t dst;
    size_t i;

    for (i = 0; i < 8u; ++i) {
        g_src[i] = (uint8_t)i;
    }

    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 8u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 9u);
    MTEST_ASSERT_EQ(g_dst[0], 7u);
    MTEST_ASSERT_MEM_EQ(g_dst + 1, g_src, 8u);
}

MTEST(rle_encode_mixed_runs) {
    static const uint8_t expected[] = { 1u, 0x10u, 0x11u, 0x82u, 0x22u, 0u, 0x33u };
    mc_buf_t dst;

    g_src[0] = 0x10u;
    g_src[1] = 0x11u;
    g_src[2] = 0x22u;
    g_src[3] = 0x22u;
    g_src[4] = 0x22u;
    g_src[5] = 0x33u;

    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 6u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, sizeof(expected));
    MTEST_ASSERT_MEM_EQ(g_dst, expected, sizeof(expected));
}

MTEST(rle_roundtrip_mixed_data) {
    mc_buf_t dst;
    mc_buf_t dec;
    size_t i;

    for (i = 0; i < 256u; ++i) {
        g_src[i] = (uint8_t)((i % 5u) == 0u ? 0xAAu : (i & 0xFFu));
    }
    memset(g_src + 100u, 0x44, 40u);

    dst = MC_BUF(g_dst, sizeof(g_dst));
    dec = MC_BUF(g_dec, sizeof(g_dec));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 256u), &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, dst.len), &dec), MC_OK);
    MTEST_ASSERT_EQ(dec.len, 256u);
    MTEST_ASSERT_MEM_EQ(g_dec, g_src, 256u);
}

MTEST(rle_decode_empty_input) {
    mc_buf_t dst = MC_BUF(g_dec, sizeof(g_dec));
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, 0u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 0u);
}

MTEST(rle_decode_corrupt_missing_repeat_byte) {
    mc_buf_t dst = MC_BUF(g_dec, sizeof(g_dec));
    g_dst[0] = 0x80u;
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, 1u), &dst), MC_ERR_CORRUPT);
}

MTEST(rle_decode_overflow) {
    uint8_t tiny[3];
    mc_buf_t dst;

    g_dst[0] = 4u;
    memcpy(g_dst + 1, "hello", 5u);
    dst = MC_BUF(tiny, sizeof(tiny));
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, 6u), &dst), MC_ERR_OVERFLOW);
}

MTEST(rle_encode_overflow) {
    uint8_t tiny[1];
    mc_buf_t dst;

    g_src[0] = 0xAAu;
    dst = MC_BUF(tiny, sizeof(tiny));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 1u), &dst), MC_ERR_OVERFLOW);
}

MTEST(rle_null_src_ptr) {
    mc_buf_t dst = MC_BUF(g_dst, sizeof(g_dst));
    mc_slice_t bad = { NULL, 3u };
    MTEST_ASSERT_EQ(mc_rle_encode(bad, &dst), MC_ERR_INVALID);
}

MTEST(rle_null_dst) {
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 1u), NULL), MC_ERR_INVALID);
}

MTEST(rle_repeated_single_byte_encodes_as_literal) {
    mc_buf_t dst;

    g_src[0] = 0x7Eu;
    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 1u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 2u);
}

MTEST(rle_binary_roundtrip) {
    static const uint8_t pattern[] = { 0x00u, 0x00u, 0xFFu, 0xFFu, 0x00u, 0xFFu, 0x00u, 0x00u };
    mc_buf_t dst;
    mc_buf_t dec;

    memcpy(g_src, pattern, sizeof(pattern));
    dst = MC_BUF(g_dst, sizeof(g_dst));
    dec = MC_BUF(g_dec, sizeof(g_dec));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, sizeof(pattern)), &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, dst.len), &dec), MC_OK);
    MTEST_ASSERT_MEM_EQ(g_dec, pattern, sizeof(pattern));
}

MTEST(rle_max_encoded_size_covers_actual) {
    mc_buf_t dst;
    size_t i;

    for (i = 0; i < 31u; ++i) {
        g_src[i] = (uint8_t)(i & 1u);
    }

    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 31u), &dst), MC_OK);
    MTEST_ASSERT_TRUE(mc_rle_max_encoded_size(31u) >= dst.len);
}

MTEST(rle_worst_case_still_encodes) {
    mc_buf_t dst;
    size_t i;

    for (i = 0; i < 32u; ++i) {
        g_src[i] = (uint8_t)((i & 1u) == 0u ? 0x00u : 0xFFu);
    }

    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 32u), &dst), MC_OK);
    MTEST_ASSERT_TRUE(dst.len > 32u);
}

MTEST(rle_decode_exact_capacity) {
    mc_buf_t dst;
    mc_buf_t dec;

    memset(g_src, 0x55, 17u);
    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 17u), &dst), MC_OK);
    dec = MC_BUF(g_dec, 17u);
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, dst.len), &dec), MC_OK);
    MTEST_ASSERT_EQ(dec.len, 17u);
}

MTEST(rle_decode_capacity_minus_one) {
    uint8_t small[16];
    mc_buf_t dst;
    mc_buf_t dec;

    memset(g_src, 0x33, 17u);
    dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 17u), &dst), MC_OK);
    dec = MC_BUF(small, sizeof(small));
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, dst.len), &dec), MC_ERR_OVERFLOW);
}

MTEST(rle_stress_all_zeros) {
    mc_buf_t dst;
    mc_buf_t dec;

    memset(g_src, 0x00, 1024u);
    dst = MC_BUF(g_dst, sizeof(g_dst));
    dec = MC_BUF(g_dec, sizeof(g_dec));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 1024u), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 16u);
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, dst.len), &dec), MC_OK);
    MTEST_ASSERT_MEM_EQ(g_dec, g_src, 1024u);
}

MTEST_SUITE(rle) {
    MTEST_RUN_F(rle_encode_empty, setup, teardown);
    MTEST_RUN_F(rle_encode_single_byte, setup, teardown);
    MTEST_RUN_F(rle_encode_128_identical_bytes, setup, teardown);
    MTEST_RUN_F(rle_encode_129_identical_bytes, setup, teardown);
    MTEST_RUN_F(rle_encode_all_different_bytes, setup, teardown);
    MTEST_RUN_F(rle_encode_mixed_runs, setup, teardown);
    MTEST_RUN_F(rle_roundtrip_mixed_data, setup, teardown);
    MTEST_RUN_F(rle_decode_empty_input, setup, teardown);
    MTEST_RUN_F(rle_decode_corrupt_missing_repeat_byte, setup, teardown);
    MTEST_RUN_F(rle_decode_overflow, setup, teardown);
    MTEST_RUN_F(rle_encode_overflow, setup, teardown);
    MTEST_RUN_F(rle_null_src_ptr, setup, teardown);
    MTEST_RUN_F(rle_null_dst, setup, teardown);
    MTEST_RUN_F(rle_repeated_single_byte_encodes_as_literal, setup, teardown);
    MTEST_RUN_F(rle_binary_roundtrip, setup, teardown);
    MTEST_RUN_F(rle_max_encoded_size_covers_actual, setup, teardown);
    MTEST_RUN_F(rle_worst_case_still_encodes, setup, teardown);
    MTEST_RUN_F(rle_decode_exact_capacity, setup, teardown);
    MTEST_RUN_F(rle_decode_capacity_minus_one, setup, teardown);
    MTEST_RUN_F(rle_stress_all_zeros, setup, teardown);
}

int main(int argc, char **argv) {
    MTEST_BEGIN(argc, argv);
    MTEST_SUITE_RUN(rle);
    return MTEST_END();
}
