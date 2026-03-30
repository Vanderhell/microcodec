#define MTEST_IMPLEMENTATION
#include "mtest.h"
#include "microcodec.h"

#include <string.h>

static uint8_t g_a[16384];
static uint8_t g_b[16384];
static uint8_t g_c[16384];
static float g_f32[1024];
static uint32_t g_u32[1024];

static void setup(void) {
    memset(g_a, 0, sizeof(g_a));
    memset(g_b, 0, sizeof(g_b));
    memset(g_c, 0, sizeof(g_c));
    memset(g_f32, 0, sizeof(g_f32));
    memset(g_u32, 0, sizeof(g_u32));
}

static void teardown(void) {
}

MTEST(integration_rle_varint_pipeline) {
    mc_buf_t rle_dst = MC_BUF(g_b, sizeof(g_b));
    mc_buf_t var_dst = MC_BUF(g_c, sizeof(g_c));
    uint32_t size_value = 0u;

    memset(g_a, 0x00, 128u);
    MTEST_ASSERT_EQ(MC_OK, mc_rle_encode(MC_SLICE(g_a, 128u), &rle_dst));
    MTEST_ASSERT_EQ(MC_OK, mc_varint_encode_u32((uint32_t)rle_dst.len, &var_dst));

    memcpy(&size_value, g_c, var_dst.len);
    MTEST_ASSERT_TRUE(rle_dst.len <= 2u);
}

MTEST(integration_delta_lzss_pipeline_roundtrip) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t delta_dst = MC_BUF(g_a, sizeof(g_a));
    mc_buf_t lzss_dst = MC_BUF(g_b, sizeof(g_b));
    mc_buf_t lzss_dec = MC_BUF(g_c, sizeof(g_c));
    float out[256];
    size_t i;

    for (i = 0u; i < 256u; ++i) {
        g_f32[i] = 22.0f + ((float)i * 0.01f);
    }

    MTEST_ASSERT_EQ(MC_OK, mc_delta_encode_f32(g_f32, 256u, 2u, &delta_dst));
    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&enc_ctx, MC_SLICE(g_a, delta_dst.len), &lzss_dst));
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_decode(&dec_ctx, MC_SLICE(g_b, lzss_dst.len), &lzss_dec));
    MTEST_ASSERT_EQ(MC_OK, mc_delta_decode_f32(MC_SLICE(g_c, lzss_dec.len), out, 256u));
    for (i = 0u; i < 256u; ++i) {
        MTEST_ASSERT_TRUE((out[i] - g_f32[i] < 0.011f) && (g_f32[i] - out[i] < 0.011f));
    }
}

MTEST(integration_delta_huff_pipeline_roundtrip) {
    mc_buf_t delta_dst = MC_BUF(g_a, sizeof(g_a));
    mc_buf_t huff_dst = MC_BUF(g_b, sizeof(g_b));
    mc_buf_t huff_dec = MC_BUF(g_c, sizeof(g_c));
    float out[128];
    size_t i;

    for (i = 0u; i < 128u; ++i) {
        g_f32[i] = 18.0f + ((float)i * 0.02f);
    }

    MTEST_ASSERT_EQ(MC_OK, mc_delta_encode_f32(g_f32, 128u, 1u, &delta_dst));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_encode_static(MC_SLICE(g_a, delta_dst.len), &huff_dst));
    MTEST_ASSERT_EQ(MC_OK, mc_huff_decode_static(MC_SLICE(g_b, huff_dst.len), &huff_dec));
    MTEST_ASSERT_EQ(MC_OK, mc_delta_decode_f32(MC_SLICE(g_c, huff_dec.len), out, 128u));
    for (i = 0u; i < 128u; ++i) {
        MTEST_ASSERT_TRUE((out[i] - g_f32[i] < 0.011f) && (g_f32[i] - out[i] < 0.011f));
    }
}

MTEST(integration_dispatch_all_algorithms) {
    mc_lzss_ctx_t lzss_ctx_enc;
    mc_lzss_ctx_t lzss_ctx_dec;
    mc_buf_t dst = MC_BUF(g_b, sizeof(g_b));
    mc_buf_t dec = MC_BUF(g_c, sizeof(g_c));
    uint32_t nums[4] = { 1u, 2u, 128u, 1024u };
    float vals[4] = { 1.0f, 1.1f, 1.2f, 1.3f };

    memset(g_a, 0x00, 64u);
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_RLE, MC_SLICE(g_a, 64u), &dst, NULL));
    MTEST_ASSERT_EQ(MC_OK, mc_decode(MC_ALG_RLE, MC_SLICE(g_b, dst.len), &dec, NULL));

    dst = MC_BUF(g_b, sizeof(g_b));
    dec = MC_BUF(g_c, sizeof(g_c));
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_VARINT, MC_SLICE(nums, sizeof(nums)), &dst, NULL));
    MTEST_ASSERT_EQ(MC_OK, mc_decode(MC_ALG_VARINT, MC_SLICE(g_b, dst.len), &dec, NULL));

    dst = MC_BUF(g_b, sizeof(g_b));
    dec = MC_BUF(g_c, sizeof(g_c));
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_DELTA, MC_SLICE(vals, sizeof(vals)), &dst, NULL));
    MTEST_ASSERT_EQ(MC_OK, mc_decode(MC_ALG_DELTA, MC_SLICE(g_b, dst.len), &dec, NULL));

    memset(g_a, 'A', 128u);
    dst = MC_BUF(g_b, sizeof(g_b));
    dec = MC_BUF(g_c, sizeof(g_c));
    mc_lzss_ctx_init(&lzss_ctx_enc);
    mc_lzss_ctx_init(&lzss_ctx_dec);
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_LZSS, MC_SLICE(g_a, 128u), &dst, &lzss_ctx_enc));
    MTEST_ASSERT_EQ(MC_OK, mc_decode(MC_ALG_LZSS, MC_SLICE(g_b, dst.len), &dec, &lzss_ctx_dec));

    memset(g_a, 0x00, 128u);
    dst = MC_BUF(g_b, sizeof(g_b));
    dec = MC_BUF(g_c, sizeof(g_c));
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_HUFF, MC_SLICE(g_a, 128u), &dst, NULL));
    MTEST_ASSERT_EQ(MC_OK, mc_decode(MC_ALG_HUFF, MC_SLICE(g_b, dst.len), &dec, NULL));
}

MTEST(integration_alg_name_non_null) {
    MTEST_ASSERT_TRUE(mc_alg_name(MC_ALG_RLE) != NULL);
    MTEST_ASSERT_TRUE(mc_alg_name(MC_ALG_VARINT) != NULL);
    MTEST_ASSERT_TRUE(mc_alg_name(MC_ALG_DELTA) != NULL);
    MTEST_ASSERT_TRUE(mc_alg_name(MC_ALG_LZSS) != NULL);
    MTEST_ASSERT_TRUE(mc_alg_name(MC_ALG_HUFF) != NULL);
}

MTEST(integration_max_encoded_size_positive) {
    MTEST_ASSERT_TRUE(mc_max_encoded_size(MC_ALG_RLE, 100u) > 0u);
    MTEST_ASSERT_TRUE(mc_max_encoded_size(MC_ALG_VARINT, 100u) > 0u);
    MTEST_ASSERT_TRUE(mc_max_encoded_size(MC_ALG_DELTA, 100u) > 0u);
    MTEST_ASSERT_TRUE(mc_max_encoded_size(MC_ALG_LZSS, 100u) > 0u);
    MTEST_ASSERT_TRUE(mc_max_encoded_size(MC_ALG_HUFF, 100u) > 0u);
}

MTEST(integration_timestamps_delta_varint_roundtrip) {
    mc_buf_t delta_dst = MC_BUF(g_a, sizeof(g_a));
    mc_buf_t var_dst = MC_BUF(g_b, sizeof(g_b));
    uint32_t decoded[100];
    size_t bytes = 0u;
    size_t i;

    for (i = 0u; i < 100u; ++i) {
        g_u32[i] = (uint32_t)(1000u + i * 100u);
    }

    MTEST_ASSERT_EQ(MC_OK, mc_delta_encode_u32(g_u32, 100u, 2u, &delta_dst));
    MTEST_ASSERT_EQ(MC_OK, mc_varint_encode_u32_array(g_u32, 100u, &var_dst));
    MTEST_ASSERT_EQ(MC_OK, mc_varint_decode_u32_array(MC_SLICE(g_b, var_dst.len), decoded, 100u, &bytes));
    MTEST_ASSERT_MEM_EQ(g_u32, decoded, 100u * sizeof(uint32_t));
}

MTEST(integration_delta_lzss_ratio_gt_three) {
    mc_lzss_ctx_t ctx;
    mc_buf_t delta_dst = MC_BUF(g_a, sizeof(g_a));
    mc_buf_t lzss_dst = MC_BUF(g_b, sizeof(g_b));
    size_t i;

    for (i = 0u; i < 512u; ++i) {
        g_f32[i] = 20.0f + ((float)i * 0.01f);
    }

    MTEST_ASSERT_EQ(MC_OK, mc_delta_encode_f32(g_f32, 512u, 2u, &delta_dst));
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_lzss_encode(&ctx, MC_SLICE(g_a, delta_dst.len), &lzss_dst));
    MTEST_ASSERT_TRUE((512u * sizeof(float)) > (lzss_dst.len * 3u));
}

MTEST(integration_rle_ratio_gt_ten) {
    mc_buf_t dst = MC_BUF(g_b, sizeof(g_b));
    memset(g_a, 0x00, 1024u);
    MTEST_ASSERT_EQ(MC_OK, mc_rle_encode(MC_SLICE(g_a, 1024u), &dst));
    MTEST_ASSERT_TRUE(1024u > (dst.len * 10u));
}

MTEST(integration_macros_work) {
    mc_slice_t slice = MC_SLICE(g_a, 10u);
    mc_buf_t buf = MC_BUF(g_b, 20u);
    MTEST_ASSERT_TRUE(slice.ptr == g_a);
    MTEST_ASSERT_EQ(10, (int)slice.len);
    MTEST_ASSERT_TRUE(buf.ptr == g_b);
    MTEST_ASSERT_EQ(20, (int)buf.cap);
    MTEST_ASSERT_EQ(0, (int)buf.len);
}

MTEST(integration_lzss_null_ctx_invalid) {
    mc_buf_t dst = MC_BUF(g_b, sizeof(g_b));
    memset(g_a, 'A', 64u);
    MTEST_ASSERT_EQ(MC_ERR_INVALID, mc_encode(MC_ALG_LZSS, MC_SLICE(g_a, 64u), &dst, NULL));
}

MTEST(integration_lzss_initialized_ctx_ok) {
    mc_lzss_ctx_t ctx;
    mc_buf_t dst = MC_BUF(g_b, sizeof(g_b));
    memset(g_a, 'A', 64u);
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_LZSS, MC_SLICE(g_a, 64u), &dst, &ctx));
}

MTEST(integration_zero_length_inputs) {
    mc_buf_t dst = MC_BUF(g_b, sizeof(g_b));
    mc_lzss_ctx_t ctx;
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_RLE, MC_SLICE(g_a, 0u), &dst, NULL));
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_LZSS, MC_SLICE(g_a, 0u), &dst, &ctx));
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_HUFF, MC_SLICE(g_a, 0u), &dst, NULL));
}

MTEST(integration_one_byte_inputs) {
    mc_buf_t dst = MC_BUF(g_b, sizeof(g_b));
    mc_lzss_ctx_t ctx;
    g_a[0] = 0x42u;
    mc_lzss_ctx_init(&ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_RLE, MC_SLICE(g_a, 1u), &dst, NULL));
    MTEST_ASSERT_EQ(MC_ERR_INCOMPRESS, mc_encode(MC_ALG_LZSS, MC_SLICE(g_a, 1u), &dst, &ctx));
}

MTEST(integration_max_realistic_input) {
    mc_lzss_ctx_t enc_ctx;
    mc_lzss_ctx_t dec_ctx;
    mc_buf_t dst = MC_BUF(g_b, sizeof(g_b));
    mc_buf_t dec = MC_BUF(g_c, sizeof(g_c));
    size_t i;

    for (i = 0u; i < 4096u; ++i) {
        g_a[i] = (uint8_t)("ABCD"[i % 4u]);
    }
    mc_lzss_ctx_init(&enc_ctx);
    mc_lzss_ctx_init(&dec_ctx);
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_LZSS, MC_SLICE(g_a, 4096u), &dst, &enc_ctx));
    MTEST_ASSERT_EQ(MC_OK, mc_decode(MC_ALG_LZSS, MC_SLICE(g_b, dst.len), &dec, &dec_ctx));
    MTEST_ASSERT_MEM_EQ(g_a, g_c, 4096u);
}

MTEST(integration_two_independent_encode_calls) {
    mc_lzss_ctx_t ctx1;
    mc_lzss_ctx_t ctx2;
    mc_buf_t dst1 = MC_BUF(g_b, sizeof(g_b) / 2u);
    mc_buf_t dst2 = MC_BUF(g_b + (sizeof(g_b) / 2u), sizeof(g_b) / 2u);

    memset(g_a, 'X', 256u);
    memset(g_c, 'Y', 256u);
    mc_lzss_ctx_init(&ctx1);
    mc_lzss_ctx_init(&ctx2);
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_LZSS, MC_SLICE(g_a, 256u), &dst1, &ctx1));
    MTEST_ASSERT_EQ(MC_OK, mc_encode(MC_ALG_LZSS, MC_SLICE(g_c, 256u), &dst2, &ctx2));
    MTEST_ASSERT_TRUE(dst1.len > 0u);
    MTEST_ASSERT_TRUE(dst2.len > 0u);
}

MTEST_SUITE(integration) {
    MTEST_RUN_F(integration_rle_varint_pipeline, setup, teardown);
    MTEST_RUN_F(integration_delta_lzss_pipeline_roundtrip, setup, teardown);
    MTEST_RUN_F(integration_delta_huff_pipeline_roundtrip, setup, teardown);
    MTEST_RUN_F(integration_dispatch_all_algorithms, setup, teardown);
    MTEST_RUN_F(integration_alg_name_non_null, setup, teardown);
    MTEST_RUN_F(integration_max_encoded_size_positive, setup, teardown);
    MTEST_RUN_F(integration_timestamps_delta_varint_roundtrip, setup, teardown);
    MTEST_RUN_F(integration_delta_lzss_ratio_gt_three, setup, teardown);
    MTEST_RUN_F(integration_rle_ratio_gt_ten, setup, teardown);
    MTEST_RUN_F(integration_macros_work, setup, teardown);
    MTEST_RUN_F(integration_lzss_null_ctx_invalid, setup, teardown);
    MTEST_RUN_F(integration_lzss_initialized_ctx_ok, setup, teardown);
    MTEST_RUN_F(integration_zero_length_inputs, setup, teardown);
    MTEST_RUN_F(integration_one_byte_inputs, setup, teardown);
    MTEST_RUN_F(integration_max_realistic_input, setup, teardown);
    MTEST_RUN_F(integration_two_independent_encode_calls, setup, teardown);
}

int main(int argc, char **argv) {
    MTEST_BEGIN(argc, argv);
    MTEST_SUITE_RUN(integration);
    return MTEST_END();
}
