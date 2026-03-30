#define MTEST_IMPLEMENTATION
#include "mtest.h"
#include "microcodec.h"

#include <limits.h>
#include <string.h>

static uint8_t g_buf[8192];
static uint32_t g_u32[1000];
static int32_t g_i32[1000];

static void setup(void) {
    memset(g_buf, 0, sizeof(g_buf));
    memset(g_u32, 0, sizeof(g_u32));
    memset(g_i32, 0, sizeof(g_i32));
}

static void teardown(void) {
}

MTEST(varint_encode_0) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_varint_encode_u32(0u, &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 1u);
    MTEST_ASSERT_EQ(g_buf[0], 0u);
}

MTEST(varint_encode_127) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_varint_encode_u32(127u, &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 1u);
    MTEST_ASSERT_EQ(g_buf[0], 0x7Fu);
}

MTEST(varint_encode_128) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_varint_encode_u32(128u, &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 2u);
    MTEST_ASSERT_EQ(g_buf[0], 0x80u);
    MTEST_ASSERT_EQ(g_buf[1], 0x01u);
}

MTEST(varint_encode_uint32_max) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_varint_encode_u32(UINT_MAX, &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 5u);
}

MTEST(varint_decode_roundtrip_set) {
    const uint32_t values[] = { 0u, 1u, 127u, 128u, 255u, 256u, 16383u, 16384u, UINT_MAX };
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    mc_slice_t src;
    size_t i;

    for (i = 0u; i < (sizeof(values) / sizeof(values[0])); ++i) {
        MTEST_ASSERT_EQ(mc_varint_encode_u32(values[i], &dst), MC_OK);
    }

    src = MC_SLICE(g_buf, dst.len);
    for (i = 0u; i < (sizeof(values) / sizeof(values[0])); ++i) {
        uint32_t decoded = 0u;
        MTEST_ASSERT_EQ(mc_varint_decode_u32(&src, &decoded), MC_OK);
        MTEST_ASSERT_EQ(decoded, values[i]);
    }
    MTEST_ASSERT_EQ(src.len, 0u);
}

MTEST(varint_zigzag_encode_0) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_varint_encode_i32(0, &dst), MC_OK);
    MTEST_ASSERT_EQ(g_buf[0], 0u);
}

MTEST(varint_zigzag_encode_minus_1) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_varint_encode_i32(-1, &dst), MC_OK);
    MTEST_ASSERT_EQ(g_buf[0], 1u);
}

MTEST(varint_zigzag_encode_plus_1) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_varint_encode_i32(1, &dst), MC_OK);
    MTEST_ASSERT_EQ(g_buf[0], 2u);
}

MTEST(varint_zigzag_encode_int32_min) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    MTEST_ASSERT_EQ(mc_varint_encode_i32(INT_MIN, &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 5u);
    MTEST_ASSERT_EQ(g_buf[4], 0x0Fu);
}

MTEST(varint_zigzag_roundtrip_single) {
    const int32_t values[] = { INT_MIN, -1000, -2, -1, 0, 1, 2, 1000, INT_MAX };
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    mc_slice_t src;
    size_t i;

    for (i = 0u; i < (sizeof(values) / sizeof(values[0])); ++i) {
        MTEST_ASSERT_EQ(mc_varint_encode_i32(values[i], &dst), MC_OK);
    }

    src = MC_SLICE(g_buf, dst.len);
    for (i = 0u; i < (sizeof(values) / sizeof(values[0])); ++i) {
        int32_t decoded = 0;
        MTEST_ASSERT_EQ(mc_varint_decode_i32(&src, &decoded), MC_OK);
        MTEST_ASSERT_EQ(decoded, values[i]);
    }
}

MTEST(varint_u32_array_roundtrip_10) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    uint32_t out[10];
    size_t consumed = 0u;
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_u32[i] = (uint32_t)(i * i * 17u);
    }

    MTEST_ASSERT_EQ(mc_varint_encode_u32_array(g_u32, 10u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_decode_u32_array(MC_SLICE(g_buf, dst.len), out, 10u, &consumed), MC_OK);
    MTEST_ASSERT_EQ(consumed, dst.len);
    MTEST_ASSERT_MEM_EQ(out, g_u32, sizeof(out));
}

MTEST(varint_i32_array_roundtrip_10) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    int32_t out[10];
    size_t consumed = 0u;
    size_t i;

    for (i = 0u; i < 10u; ++i) {
        g_i32[i] = (int32_t)i - 5;
    }

    MTEST_ASSERT_EQ(mc_varint_encode_i32_array(g_i32, 10u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_decode_i32_array(MC_SLICE(g_buf, dst.len), out, 10u, &consumed), MC_OK);
    MTEST_ASSERT_EQ(consumed, dst.len);
    MTEST_ASSERT_MEM_EQ(out, g_i32, sizeof(out));
}

MTEST(varint_array_mixed_values) {
    const uint32_t values[] = { 0u, UINT_MAX, 128u, 1u };
    uint32_t out[4];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t consumed = 0u;

    MTEST_ASSERT_EQ(mc_varint_encode_u32_array(values, 4u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_decode_u32_array(MC_SLICE(g_buf, dst.len), out, 4u, &consumed), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, values, sizeof(values));
}

MTEST(varint_decode_truncated_stream) {
    mc_slice_t src;
    uint32_t value = 0u;

    g_buf[0] = 0x80u;
    src = MC_SLICE(g_buf, 1u);
    MTEST_ASSERT_EQ(mc_varint_decode_u32(&src, &value), MC_ERR_CORRUPT);
}

MTEST(varint_decode_empty_stream) {
    mc_slice_t src = MC_SLICE(g_buf, 0u);
    uint32_t value = 0u;
    MTEST_ASSERT_EQ(mc_varint_decode_u32(&src, &value), MC_ERR_CORRUPT);
}

MTEST(varint_null_pointer_args) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    mc_slice_t src = MC_SLICE(g_buf, 1u);
    uint32_t uval = 0u;
    int32_t ival = 0;

    MTEST_ASSERT_EQ(mc_varint_encode_u32(1u, NULL), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_varint_decode_u32(NULL, &uval), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_varint_decode_u32(&src, NULL), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_varint_encode_u32_array(NULL, 1u, &dst), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_varint_decode_i32(NULL, &ival), MC_ERR_INVALID);
}

MTEST(varint_overflow_dst_cap_zero) {
    uint8_t tiny[1];
    mc_buf_t dst = MC_BUF(tiny, 0u);
    MTEST_ASSERT_EQ(mc_varint_encode_u32(1u, &dst), MC_ERR_OVERFLOW);
}

MTEST(varint_max_encoded_size_covers_actual) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 25u; ++i) {
        g_u32[i] = (uint32_t)(i * 1000u);
    }

    MTEST_ASSERT_EQ(mc_varint_encode_u32_array(g_u32, 25u, &dst), MC_OK);
    MTEST_ASSERT_TRUE(mc_varint_max_encoded_size(25u) >= dst.len);
}

MTEST(varint_timestamps_encode_efficiently) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t i;

    for (i = 0u; i < 32u; ++i) {
        g_u32[i] = (uint32_t)(1000u + i);
    }

    MTEST_ASSERT_EQ(mc_varint_encode_u32_array(g_u32, 32u, &dst), MC_OK);
    MTEST_ASSERT_TRUE(dst.len < (32u * sizeof(uint32_t)));
}

MTEST(varint_negative_values_roundtrip) {
    const int32_t values[] = { -100, -50, -1, -2000000000 };
    int32_t out[4];
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t consumed = 0u;

    MTEST_ASSERT_EQ(mc_varint_encode_i32_array(values, 4u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_decode_i32_array(MC_SLICE(g_buf, dst.len), out, 4u, &consumed), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, values, sizeof(values));
}

MTEST(varint_large_array_1000_u32) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    uint32_t out[1000];
    size_t consumed = 0u;
    size_t i;

    for (i = 0u; i < 1000u; ++i) {
        g_u32[i] = (uint32_t)i;
    }

    MTEST_ASSERT_EQ(mc_varint_encode_u32_array(g_u32, 1000u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_decode_u32_array(MC_SLICE(g_buf, dst.len), out, 1000u, &consumed), MC_OK);
    MTEST_ASSERT_MEM_EQ(out, g_u32, sizeof(out));
}

MTEST(varint_decode_u32_array_bytes_consumed) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    uint32_t out[3];
    size_t consumed = 0u;

    MTEST_ASSERT_EQ(mc_varint_encode_u32(1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_encode_u32(128u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_encode_u32(16384u, &dst), MC_OK);
    g_buf[dst.len++] = 0xFFu;

    MTEST_ASSERT_EQ(mc_varint_decode_u32_array(MC_SLICE(g_buf, dst.len), out, 3u, &consumed), MC_OK);
    MTEST_ASSERT_EQ(consumed, 6u);
}

MTEST(varint_consecutive_encode_calls_append) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));

    MTEST_ASSERT_EQ(mc_varint_encode_u32(1u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_encode_u32(128u, &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 3u);
    MTEST_ASSERT_EQ(g_buf[0], 1u);
    MTEST_ASSERT_EQ(g_buf[1], 0x80u);
    MTEST_ASSERT_EQ(g_buf[2], 0x01u);
}

MTEST(varint_single_value_array_same_as_single_encode) {
    uint8_t single[8];
    mc_buf_t dst_array = MC_BUF(g_buf, sizeof(g_buf));
    mc_buf_t dst_single = MC_BUF(single, sizeof(single));
    const uint32_t value = 300u;

    MTEST_ASSERT_EQ(mc_varint_encode_u32_array(&value, 1u, &dst_array), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_encode_u32(value, &dst_single), MC_OK);
    MTEST_ASSERT_EQ(dst_array.len, dst_single.len);
    MTEST_ASSERT_MEM_EQ(g_buf, single, dst_single.len);
}

MTEST(varint_decode_u32_advances_src) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    mc_slice_t src;
    uint32_t value = 0u;

    MTEST_ASSERT_EQ(mc_varint_encode_u32(128u, &dst), MC_OK);
    MTEST_ASSERT_EQ(mc_varint_encode_u32(1u, &dst), MC_OK);
    src = MC_SLICE(g_buf, dst.len);
    MTEST_ASSERT_EQ(mc_varint_decode_u32(&src, &value), MC_OK);
    MTEST_ASSERT_EQ(value, 128u);
    MTEST_ASSERT_EQ(src.len, 1u);
    MTEST_ASSERT_EQ(src.ptr[0], 1u);
}

MTEST(varint_decode_overlong_stream_corrupt) {
    mc_slice_t src;
    uint32_t value = 0u;

    memset(g_buf, 0x80, 6u);
    src = MC_SLICE(g_buf, 6u);
    MTEST_ASSERT_EQ(mc_varint_decode_u32(&src, &value), MC_ERR_CORRUPT);
}

MTEST(varint_zero_length_arrays_ok) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t consumed = 123u;

    MTEST_ASSERT_EQ(mc_varint_encode_u32_array(NULL, 0u, &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 0u);
    MTEST_ASSERT_EQ(mc_varint_decode_u32_array(MC_SLICE(g_buf, 0u), g_u32, 0u, &consumed), MC_OK);
    MTEST_ASSERT_EQ(consumed, 0u);
}

MTEST(varint_i32_array_invalid_args) {
    mc_buf_t dst = MC_BUF(g_buf, sizeof(g_buf));
    size_t consumed = 0u;

    MTEST_ASSERT_EQ(mc_varint_encode_i32_array(NULL, 2u, &dst), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_varint_decode_i32_array(MC_SLICE(g_buf, 0u), NULL, 1u, &consumed), MC_ERR_INVALID);
    MTEST_ASSERT_EQ(mc_varint_decode_u32_array(MC_SLICE(g_buf, 1u), g_u32, 1u, NULL), MC_ERR_INVALID);
}

MTEST_SUITE(varint) {
    MTEST_RUN_F(varint_encode_0, setup, teardown);
    MTEST_RUN_F(varint_encode_127, setup, teardown);
    MTEST_RUN_F(varint_encode_128, setup, teardown);
    MTEST_RUN_F(varint_encode_uint32_max, setup, teardown);
    MTEST_RUN_F(varint_decode_roundtrip_set, setup, teardown);
    MTEST_RUN_F(varint_zigzag_encode_0, setup, teardown);
    MTEST_RUN_F(varint_zigzag_encode_minus_1, setup, teardown);
    MTEST_RUN_F(varint_zigzag_encode_plus_1, setup, teardown);
    MTEST_RUN_F(varint_zigzag_encode_int32_min, setup, teardown);
    MTEST_RUN_F(varint_zigzag_roundtrip_single, setup, teardown);
    MTEST_RUN_F(varint_u32_array_roundtrip_10, setup, teardown);
    MTEST_RUN_F(varint_i32_array_roundtrip_10, setup, teardown);
    MTEST_RUN_F(varint_array_mixed_values, setup, teardown);
    MTEST_RUN_F(varint_decode_truncated_stream, setup, teardown);
    MTEST_RUN_F(varint_decode_empty_stream, setup, teardown);
    MTEST_RUN_F(varint_null_pointer_args, setup, teardown);
    MTEST_RUN_F(varint_overflow_dst_cap_zero, setup, teardown);
    MTEST_RUN_F(varint_max_encoded_size_covers_actual, setup, teardown);
    MTEST_RUN_F(varint_timestamps_encode_efficiently, setup, teardown);
    MTEST_RUN_F(varint_negative_values_roundtrip, setup, teardown);
    MTEST_RUN_F(varint_large_array_1000_u32, setup, teardown);
    MTEST_RUN_F(varint_decode_u32_array_bytes_consumed, setup, teardown);
    MTEST_RUN_F(varint_consecutive_encode_calls_append, setup, teardown);
    MTEST_RUN_F(varint_single_value_array_same_as_single_encode, setup, teardown);
    MTEST_RUN_F(varint_decode_u32_advances_src, setup, teardown);
    MTEST_RUN_F(varint_decode_overlong_stream_corrupt, setup, teardown);
    MTEST_RUN_F(varint_zero_length_arrays_ok, setup, teardown);
    MTEST_RUN_F(varint_i32_array_invalid_args, setup, teardown);
}

int main(int argc, char **argv) {
    MTEST_BEGIN(argc, argv);
    MTEST_SUITE_RUN(varint);
    return MTEST_END();
}
