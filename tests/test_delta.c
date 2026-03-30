#define MTEST_IMPLEMENTATION
#include "mtest.h"
#include "microcodec.h"

#include <string.h>

static uint8_t g_buf[8192];
static int32_t g_i32[2048];
static uint32_t g_u32[2048];
static float g_f32[2048];

static void setup(void) {
    memset(g_buf, 0, sizeof(g_buf));
    memset(g_i32, 0, sizeof(g_i32));
    memset(g_u32, 0, sizeof(g_u32));
    memset(g_f32, 0, sizeof(g_f32));
}

static void teardown(void) {
}

static float absf_(float value) {
    return (value < 0.0f) ? -value : value;
}

MTEST(delta_encode_constant_i32_minimal) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_i32[i] = 42;
    }

    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 10u, 1u, &dst), MC_OK);
    MTEST_ASSERT_TRUE(dst.len <= 21u);
}

MTEST(delta_encode_monotonic_order1_efficient) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_i32[i] = (int32_t)i;
    }

    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 10u, 1u, &dst), MC_OK);
    MTEST_ASSERT_TRUE(dst.len < (10u * sizeof(int32_t)));
}

MTEST(delta_encode_linear_order2_efficient) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_i32[i] = (int32_t)(100 + (int32_t)(i * 7u));
    }

    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 10u, 2u, &dst), MC_OK);
    MTEST_ASSERT_TRUE(dst.len <= 24u);
}

MTEST(delta_roundtrip_i32_order1) {
    int32_t out[10];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_i32[i] = (int32_t)(i * i) - 20;
    }

    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 10u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len), out, 10u), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, g_i32, sizeof(out));
}

MTEST(delta_roundtrip_i32_order2) {
    int32_t out[10];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_i32[i] = (int32_t)(5 + (int32_t)(i * 10u));
    }

    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 10u, 2u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len), out, 10u), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, g_i32, sizeof(out));
}

MTEST(delta_roundtrip_u32_order1) {
    uint32_t out[10];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_u32[i] = 1000u + (uint32_t)(i * 17u);
    }

    MTEST_ASSERT_EQ(mc_delta_encode_u32(g_u32, 10u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_u32(MC_SLICE(g_buf, dst.len), out, 10u), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, g_u32, sizeof(out));
}

MTEST(delta_roundtrip_u32_order2) {
    uint32_t out[10];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_u32[i] = 1000u + (uint32_t)(i * 100u);
    }

    MTEST_ASSERT_EQ(mc_delta_encode_u32(g_u32, 10u, 2u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_u32(MC_SLICE(g_buf, dst.len), out, 10u), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, g_u32, sizeof(out));
}

MTEST(delta_roundtrip_f32_order1) {
    float out[10];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_f32[i] = 23.10f + ((float)i * 0.13f);
    }

    MTEST_ASSERT_EQ(mc_delta_encode_f32(g_f32, 10u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_f32(MC_SLICE(g_buf, dst.len), out, 10u), MC_OK);
    for (i = 0u; i < 10u; ++i) {
        MTEST_ASSERT_TRUE(absf_(out[i] - g_f32[i]) <= 0.011f);
    }
}

MTEST(delta_roundtrip_f32_order2) {
    float out[10];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_f32[i] = 20.0f + ((float)i * 0.25f);
    }

    MTEST_ASSERT_EQ(mc_delta_encode_f32(g_f32, 10u, 2u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_f32(MC_SLICE(g_buf, dst.len), out, 10u), MC_OK);
    for (i = 0u; i < 10u; ++i) {
        MTEST_ASSERT_TRUE(absf_(out[i] - g_f32[i]) <= 0.011f);
    }
}

MTEST(delta_f32_precision_within_scale) {
    float out[3];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));

    g_f32[0] = 1.234f;
    g_f32[1] = 1.235f;
    g_f32[2] = 1.236f;

    MTEST_ASSERT_EQ(mc_delta_encode_f32(g_f32, 3u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_f32(MC_SLICE(g_buf, dst.len), out, 3u), MC_OK);
    MTEST_ASSERT_TRUE(absf_(out[0] - g_f32[0]) <= 0.011f);
    MTEST_ASSERT_TRUE(absf_(out[1] - g_f32[1]) <= 0.011f);
    MTEST_ASSERT_TRUE(absf_(out[2] - g_f32[2]) <= 0.011f);
}

MTEST(delta_single_value_ok) {
    int32_t out[1];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));

    g_i32[0] = 77;
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 1u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len), out, 1u), MC_OK);
    MTEST_ASSERT_EQ(out[0], 77);
}

MTEST(delta_two_values_order1_ok) {
    int32_t out[2];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));

    g_i32[0] = 10;
    g_i32[1] = 15;
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 2u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len), out, 2u), MC_OK);
    MTEST_ASSERT_EQ(out[0], 10);
    MTEST_ASSERT_EQ(out[1], 15);
}

MTEST(delta_two_values_order2_ok) {
    int32_t out[2];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));

    g_i32[0] = 10;
    g_i32[1] = 15;
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 2u, 2u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len), out, 2u), MC_OK);
    MTEST_ASSERT_EQ(out[0], 10);
    MTEST_ASSERT_EQ(out[1], 15);
}

MTEST(delta_invalid_order) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 1u, 0u, &dst), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 1u, 3u, &dst), MC_ERR_INVALID);
}

MTEST(delta_count_zero_invalid) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 0u, 1u, &dst), MC_ERR_INVALID);
}

MTEST(delta_output_overflow) {
    uint8_t tiny[8];
    mc_buf_t dst = MC_BUF(tiny, sizeof(tiny));
    g_i32[0] = 1;
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 1u, 1u, &dst), MC_ERR_OVERFLOW);
}

MTEST(delta_corrupt_header_bad_type) {
    int32_t out[1];
    memset(g_buf, 0, 12u);
    g_buf[0] = 9u;
    g_buf[1] = 1u;
    g_buf[2] = 1u;
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, 12u), out, 1u), MC_ERR_CORRUPT);
}

MTEST(delta_count_mismatch_invalid) {
    int32_t out[2];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    g_i32[0] = 5;
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 1u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len), out, 2u), MC_ERR_INVALID);
}

MTEST(delta_null_pointers) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_delta_encode_i32(NULL, 1u, 1u, &dst), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 1u, 1u, NULL), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, 1u), NULL, 1u), MC_ERR_INVALID);
}

MTEST(delta_large_negative_deltas_roundtrip) {
    int32_t out[4];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));

    g_i32[0] = 1000;
    g_i32[1] = -1000;
    g_i32[2] = -3000;
    g_i32[3] = -6000;

    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 4u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len), out, 4u), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, g_i32, sizeof(out));
}

MTEST(delta_f32_scale_stored_in_header) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    float scale = 0.0f;

    g_f32[0] = 1.0f;
    MTEST_ASSERT_EQ(mc_delta_encode_f32(g_f32, 1u, 1u, &dst), MC_OK);
    memcpy(&scale, g_buf + 4u, sizeof(scale));
    MTEST_ASSERT_TRUE(scale > 99.0f);
    MTEST_ASSERT_TRUE(scale < 101.0f);
}

MTEST(delta_max_encoded_size_covers_actual) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 32u; ++i) {
        g_i32[i] = (int32_t)i;
    }

    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 32u, 1u, &dst), MC_OK);
    MTEST_ASSERT_TRUE(mc_delta_max_encoded_size(32u) >= dst.len);
}

MTEST(delta_timestamps_step100_order2_small) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 100u; ++i) {
        g_u32[i] = (uint32_t)(i * 100u);
    }

    MTEST_ASSERT_EQ(mc_delta_encode_u32(g_u32, 100u, 2u, &dst), MC_OK);
    MTEST_ASSERT_TRUE(dst.len < 120u);
}

MTEST(delta_mixed_positive_negative_roundtrip) {
    int32_t out[6];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    static const int32_t values[] = { 0, 10, -10, 20, -20, 0 };

    memcpy(g_i32, values, sizeof(values));
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 6u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len), out, 6u), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, values, sizeof(values));
}

MTEST(delta_f32_1000_roundtrip_within_precision) {
    float out[1000];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 1000u; ++i) {
        g_f32[i] = 15.0f + ((float)i * 0.01f);
    }

    MTEST_ASSERT_EQ(mc_delta_encode_f32(g_f32, 1000u, 2u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_f32(MC_SLICE(g_buf, dst.len), out, 1000u), MC_OK);
    for (i = 0u; i < 1000u; ++i) {
        MTEST_ASSERT_TRUE(absf_(out[i] - g_f32[i]) <= 0.011f);
    }
}

MTEST(delta_decode_truncated_stream_corrupt) {
    int32_t out[2];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));

    g_i32[0] = 1;
    g_i32[1] = 2;
    MTEST_ASSERT_EQ(mc_delta_encode_i32(g_i32, 2u, 1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, dst.len - 1u), out, 2u), MC_ERR_CORRUPT);
}

MTEST(delta_decode_bad_order_corrupt) {
    int32_t out[1];
    memset(g_buf, 0, 12u);
    g_buf[0] = 0u;
    g_buf[1] = 5u;
    g_buf[2] = 1u;
    MTEST_ASSERT_EQ(mc_delta_decode_i32(MC_SLICE(g_buf, 12u), out, 1u), MC_ERR_CORRUPT);
}

MTEST(delta_u32_rejects_large_values_beyond_int32) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    g_u32[0] = 0xFFFFFFFFu;
    MTEST_ASSERT_EQ(mc_delta_encode_u32(g_u32, 1u, 1u, &dst), MC_ERR_INVALID);
}

MTEST_SUITE(delta) {
    MTEST_RUN_F(delta_encode_constant_i32_minimal, setup, teardown);
    MTEST_RUN_F(delta_encode_monotonic_order1_efficient, setup, teardown);
    MTEST_RUN_F(delta_encode_linear_order2_efficient, setup, teardown);
    MTEST_RUN_F(delta_roundtrip_i32_order1, setup, teardown);
    MTEST_RUN_F(delta_roundtrip_i32_order2, setup, teardown);
    MTEST_RUN_F(delta_roundtrip_u32_order1, setup, teardown);
    MTEST_RUN_F(delta_roundtrip_u32_order2, setup, teardown);
    MTEST_RUN_F(delta_roundtrip_f32_order1, setup, teardown);
    MTEST_RUN_F(delta_roundtrip_f32_order2, setup, teardown);
    MTEST_RUN_F(delta_f32_precision_within_scale, setup, teardown);
    MTEST_RUN_F(delta_single_value_ok, setup, teardown);
    MTEST_RUN_F(delta_two_values_order1_ok, setup, teardown);
    MTEST_RUN_F(delta_two_values_order2_ok, setup, teardown);
    MTEST_RUN_F(delta_invalid_order, setup, teardown);
    MTEST_RUN_F(delta_count_zero_invalid, setup, teardown);
    MTEST_RUN_F(delta_output_overflow, setup, teardown);
    MTEST_RUN_F(delta_corrupt_header_bad_type, setup, teardown);
    MTEST_RUN_F(delta_count_mismatch_invalid, setup, teardown);
    MTEST_RUN_F(delta_null_pointers, setup, teardown);
    MTEST_RUN_F(delta_large_negative_deltas_roundtrip, setup, teardown);
    MTEST_RUN_F(delta_f32_scale_stored_in_header, setup, teardown);
    MTEST_RUN_F(delta_max_encoded_size_covers_actual, setup, teardown);
    MTEST_RUN_F(delta_timestamps_step100_order2_small, setup, teardown);
    MTEST_RUN_F(delta_mixed_positive_negative_roundtrip, setup, teardown);
    MTEST_RUN_F(delta_f32_1000_roundtrip_within_precision, setup, teardown);
    MTEST_RUN_F(delta_decode_truncated_stream_corrupt, setup, teardown);
    MTEST_RUN_F(delta_decode_bad_order_corrupt, setup, teardown);
    MTEST_RUN_F(delta_u32_rejects_large_values_beyond_int32, setup, teardown);
}

int main(int argc, char **argv) {
    MTEST_BEGIN(argc, argv);
    MTEST_SUITE_RUN(delta);
    return MTEST_END();
}
