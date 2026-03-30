# microcodec — Agent Implementation Guide

> Doplnok k `microcodec_spec.md`.
> Presné interné štruktúry, algoritmy krok po kroku,
> build systém, test vzory a commit stratégia.

---

## 1. Implementačné poradie

```
Krok 1:  include/microcodec.h     — všetky typy, enums, API signatúry
Krok 2:  src/mc_rle.c             — RLE encode/decode
         tests/test_rle.c         — 20 testov (všetky musia prejsť)
         → tag v0.1.0
Krok 3:  src/mc_varint.c          — varint + zig-zag
         tests/test_varint.c      — 25 testov
         → tag v0.2.0
Krok 4:  src/mc_delta.c           — delta + delta-of-delta
         tests/test_delta.c       — 25 testov
         → tag v0.3.0
Krok 5:  src/mc_lzss.c            — LZSS
         tests/test_lzss.c        — 25 testov
         → tag v0.4.0
Krok 6:  src/mc_huff.c            — static + adaptive Huffman
         tests/test_huff.c        — 25 testov
         → tag v0.5.0
Krok 7:  src/microcodec.c         — dispatch API
         tests/test_integration.c — 20 testov
         → tag v1.0.0
Krok 8:  README.md
```

Pravidlo: každý krok musí mať čistý build a prejdené testy pred ďalším.

---

## 2. include/microcodec.h — presná štruktúra

```c
/*
 * microcodec.h — Compression library for embedded sensor data
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microcodec
 */

#ifndef MICROCODEC_H
#define MICROCODEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── 1. Defaults ─────────────────────────────────────────────────────────── */
#ifndef MICROCODEC_ENABLE_RLE
#  define MICROCODEC_ENABLE_RLE    1
#endif
#ifndef MICROCODEC_ENABLE_VARINT
#  define MICROCODEC_ENABLE_VARINT 1
#endif
#ifndef MICROCODEC_ENABLE_DELTA
#  define MICROCODEC_ENABLE_DELTA  1
#endif
#ifndef MICROCODEC_ENABLE_LZSS
#  define MICROCODEC_ENABLE_LZSS   1
#endif
#ifndef MICROCODEC_ENABLE_HUFF
#  define MICROCODEC_ENABLE_HUFF   1
#endif

#ifndef MICROCODEC_LZSS_WINDOW_SIZE
#  define MICROCODEC_LZSS_WINDOW_SIZE  256
#endif
#ifndef MICROCODEC_LZSS_MIN_MATCH
#  define MICROCODEC_LZSS_MIN_MATCH    3
#endif
#ifndef MICROCODEC_LZSS_MAX_MATCH
#  define MICROCODEC_LZSS_MAX_MATCH    18
#endif

#ifndef MICROCODEC_HUFF_SYMBOLS
#  define MICROCODEC_HUFF_SYMBOLS      256
#endif
#ifndef MICROCODEC_HUFF_MAX_CODE_LEN
#  define MICROCODEC_HUFF_MAX_CODE_LEN 15
#endif

#ifndef MICROCODEC_DELTA_F32_SCALE
#  define MICROCODEC_DELTA_F32_SCALE   100.0f
#endif

/* ── 2. Compile-time assertions ──────────────────────────────────────────── */
/* LZSS window must be power of 2 */
_Static_assert((MICROCODEC_LZSS_WINDOW_SIZE & (MICROCODEC_LZSS_WINDOW_SIZE - 1)) == 0,
               "MICROCODEC_LZSS_WINDOW_SIZE must be a power of 2");
_Static_assert(MICROCODEC_LZSS_WINDOW_SIZE >= 64,
               "MICROCODEC_LZSS_WINDOW_SIZE must be >= 64");
_Static_assert(MICROCODEC_LZSS_MIN_MATCH >= 2,
               "MICROCODEC_LZSS_MIN_MATCH must be >= 2");
_Static_assert(MICROCODEC_LZSS_MAX_MATCH > MICROCODEC_LZSS_MIN_MATCH,
               "MICROCODEC_LZSS_MAX_MATCH must be > MIN_MATCH");

/* ── 3. Error codes ──────────────────────────────────────────────────────── */
/* (copy from spec §4) */

/* ── 4. Common types ─────────────────────────────────────────────────────── */
/* mc_slice_t, mc_buf_t, mc_alg_t, mc_delta_type_t */
/* MC_SLICE, MC_BUF macros */

/* ── 5. RLE API ──────────────────────────────────────────────────────────── */
#if MICROCODEC_ENABLE_RLE
/* (functions from spec §6.3) */
#endif

/* ── 6. Varint API ───────────────────────────────────────────────────────── */
#if MICROCODEC_ENABLE_VARINT
/* (functions from spec §7.3) */
#endif

/* ── 7. Delta API ────────────────────────────────────────────────────────── */
#if MICROCODEC_ENABLE_DELTA
/* mc_delta_type_t enum */
/* (functions from spec §8.5) */
#endif

/* ── 8. LZSS API ─────────────────────────────────────────────────────────── */
#if MICROCODEC_ENABLE_LZSS
/* mc_lzss_ctx_t struct */
/* (functions from spec §9.4) */
#endif

/* ── 9. Huffman API ──────────────────────────────────────────────────────── */
#if MICROCODEC_ENABLE_HUFF
/* mc_huff_entry_t struct */
/* (functions from spec §10.4) */
#endif

/* ── 10. Dispatch API ────────────────────────────────────────────────────── */
/* mc_encode, mc_decode, mc_max_encoded_size, mc_alg_name */

#endif /* MICROCODEC_H */
```

---

## 3. RLE — presná implementácia

### 3.1 Token format

```c
/* Token byte:
 * bit 7 = 0: literal run — next (count) bytes are literals
 * bit 7 = 1: repeat run  — next byte repeated (count) times
 * bits 6..0 = count - 1  (range 0..127 → count 1..128) */

#define RLE_REPEAT_FLAG   0x80u
#define RLE_COUNT_MASK    0x7Fu
#define RLE_MAX_COUNT     128u
```

### 3.2 Encode algoritmus

```c
mc_err_t mc_rle_encode(mc_slice_t src, mc_buf_t *dst) {
    if (!src.ptr || !dst || !dst->ptr) return MC_ERR_INVALID;
    dst->len = 0;

    size_t i = 0;
    while (i < src.len) {
        /* Skontroluj repeat run od pozície i */
        size_t run = 1;
        while (run < RLE_MAX_COUNT
               && i + run < src.len
               && src.ptr[i + run] == src.ptr[i]) {
            run++;
        }

        if (run >= 2) {
            /* Repeat run */
            if (dst->len + 2 > dst->cap) return MC_ERR_OVERFLOW;
            dst->ptr[dst->len++] = (uint8_t)(RLE_REPEAT_FLAG | (run - 1));
            dst->ptr[dst->len++] = src.ptr[i];
            i += run;
        } else {
            /* Literal run — nájdi kde sa opakuje ďalší znak */
            size_t lit_start = i;
            size_t lit_count = 0;
            while (lit_count < RLE_MAX_COUNT && i < src.len) {
                /* Pozri dopredu — ak nasledujú 2+ rovnaké, zastav literal run */
                if (i + 1 < src.len && src.ptr[i] == src.ptr[i + 1]) break;
                lit_count++;
                i++;
            }
            if (lit_count == 0) lit_count = 1, i++;  /* Edge case */
            if (dst->len + 1 + lit_count > dst->cap) return MC_ERR_OVERFLOW;
            dst->ptr[dst->len++] = (uint8_t)(lit_count - 1);  /* no repeat flag */
            memcpy(dst->ptr + dst->len, src.ptr + lit_start, lit_count);
            dst->len += lit_count;
        }
    }
    return MC_OK;
}
```

### 3.3 Decode algoritmus

```c
mc_err_t mc_rle_decode(mc_slice_t src, mc_buf_t *dst) {
    if (!src.ptr || !dst || !dst->ptr) return MC_ERR_INVALID;
    dst->len = 0;

    size_t i = 0;
    while (i < src.len) {
        uint8_t token = src.ptr[i++];
        uint8_t count = (token & RLE_COUNT_MASK) + 1;

        if (token & RLE_REPEAT_FLAG) {
            /* Repeat run */
            if (i >= src.len) return MC_ERR_CORRUPT;
            uint8_t byte = src.ptr[i++];
            if (dst->len + count > dst->cap) return MC_ERR_OVERFLOW;
            memset(dst->ptr + dst->len, byte, count);
            dst->len += count;
        } else {
            /* Literal run */
            if (i + count > src.len) return MC_ERR_CORRUPT;
            if (dst->len + count > dst->cap) return MC_ERR_OVERFLOW;
            memcpy(dst->ptr + dst->len, src.ptr + i, count);
            dst->len += count;
            i += count;
        }
    }
    return MC_OK;
}
```

---

## 4. Varint — presná implementácia

### 4.1 Encode single u32

```c
mc_err_t mc_varint_encode_u32(uint32_t value, mc_buf_t *dst) {
    if (!dst || !dst->ptr) return MC_ERR_INVALID;

    uint8_t buf[5];
    size_t  n = 0;

    do {
        buf[n++] = (uint8_t)((value & 0x7Fu) | (value > 0x7Fu ? 0x80u : 0u));
        value >>= 7;
    } while (value != 0);

    if (dst->len + n > dst->cap) return MC_ERR_OVERFLOW;
    memcpy(dst->ptr + dst->len, buf, n);
    dst->len += n;
    return MC_OK;
}
```

### 4.2 Decode single u32

```c
mc_err_t mc_varint_decode_u32(mc_slice_t *src, uint32_t *value) {
    if (!src || !src->ptr || !value) return MC_ERR_INVALID;

    uint32_t result = 0;
    uint8_t  shift  = 0;

    while (src->len > 0) {
        uint8_t byte = src->ptr[0];
        src->ptr++;
        src->len--;

        result |= (uint32_t)(byte & 0x7Fu) << shift;
        if (!(byte & 0x80u)) {
            *value = result;
            return MC_OK;
        }
        shift += 7u;
        if (shift >= 35u) return MC_ERR_CORRUPT;   /* Overflow protection */
    }
    return MC_ERR_CORRUPT;   /* Truncated */
}
```

### 4.3 Zig-zag

```c
static inline uint32_t mc_zigzag_encode_i32(int32_t value) {
    return ((uint32_t)(value << 1)) ^ (uint32_t)(value >> 31);
}

static inline int32_t mc_zigzag_decode_i32(uint32_t value) {
    return (int32_t)((value >> 1) ^ (uint32_t)(-(int32_t)(value & 1u)));
}
```

---

## 5. Delta — presná implementácia

### 5.1 Header write helper

```c
/* Zapíše 8-bajtový delta header do dst.
 * Vracia MC_OK alebo MC_ERR_OVERFLOW. */
static mc_err_t delta_write_header(mc_buf_t         *dst,
                                    mc_delta_type_t   type,
                                    uint8_t           order,
                                    uint16_t          count,
                                    float             f32_scale) {
    if (dst->len + 8u > dst->cap) return MC_ERR_OVERFLOW;
    dst->ptr[dst->len++] = (uint8_t)type;
    dst->ptr[dst->len++] = order;
    dst->ptr[dst->len++] = (uint8_t)(count & 0xFFu);
    dst->ptr[dst->len++] = (uint8_t)(count >> 8u);
    memcpy(dst->ptr + dst->len, &f32_scale, 4u);
    dst->len += 4u;
    return MC_OK;
}
```

### 5.2 i32 delta encode

```c
mc_err_t mc_delta_encode_i32(const int32_t *src_arr,
                               size_t         count,
                               uint8_t        order,
                               mc_buf_t      *dst) {
    if (!src_arr || !dst || !dst->ptr) return MC_ERR_INVALID;
    if (count == 0 || (order != 1 && order != 2)) return MC_ERR_INVALID;

    mc_err_t err = delta_write_header(dst, MC_DELTA_I32, order,
                                       (uint16_t)count, 0.0f);
    if (err != MC_OK) return err;

    /* Prvá hodnota raw */
    if (dst->len + 4u > dst->cap) return MC_ERR_OVERFLOW;
    memcpy(dst->ptr + dst->len, &src_arr[0], 4u);
    dst->len += 4u;

    if (count == 1) return MC_OK;

    if (order == 1) {
        /* Delta order 1 */
        for (size_t i = 1; i < count; i++) {
            int32_t  delta = src_arr[i] - src_arr[i - 1];
            uint32_t zz    = mc_zigzag_encode_i32(delta);
            err = mc_varint_encode_u32(zz, dst);
            if (err != MC_OK) return err;
        }
    } else {
        /* Delta order 2 — prvý delta raw, potom dod */
        int32_t first_delta = src_arr[1] - src_arr[0];
        if (dst->len + 4u > dst->cap) return MC_ERR_OVERFLOW;
        memcpy(dst->ptr + dst->len, &first_delta, 4u);
        dst->len += 4u;

        for (size_t i = 2; i < count; i++) {
            int32_t  delta = src_arr[i] - src_arr[i - 1];
            int32_t  prev  = src_arr[i - 1] - src_arr[i - 2];
            int32_t  dod   = delta - prev;
            uint32_t zz    = mc_zigzag_encode_i32(dod);
            err = mc_varint_encode_u32(zz, dst);
            if (err != MC_OK) return err;
        }
    }
    return MC_OK;
}
```

### 5.3 f32 encode/decode

```c
/* F32: kvantizuj na int32_t, potom použijemc_delta_encode_i32 */
mc_err_t mc_delta_encode_f32(const float *src_arr,
                               size_t       count,
                               uint8_t      order,
                               mc_buf_t    *dst) {
    if (!src_arr || !dst || !dst->ptr) return MC_ERR_INVALID;
    if (count == 0 || (order != 1 && order != 2)) return MC_ERR_INVALID;

    /* Kvantizácia — stack buffer, max 256 hodnôt naraz pre MCU komfort */
    /* Pre väčšie polia: enkóduj po blokoch (nie je potrebné pre v1.0) */
    /* Zjednodušenie: použijemc VLA alebo pevný max 4096 */
    #define MC_DELTA_F32_MAX_COUNT 4096
    if (count > MC_DELTA_F32_MAX_COUNT) return MC_ERR_INVALID;

    /* Kvantizácia priamo do dst po headerí — uložíme scale do headra */
    /* Najprv zapíš header so scale */
    mc_err_t err = delta_write_header(dst, MC_DELTA_F32, order,
                                       (uint16_t)count,
                                       MICROCODEC_DELTA_F32_SCALE);
    if (err != MC_OK) return err;

    /* Prvá hodnota raw (kvantizovaná) */
    int32_t q0 = (int32_t)(src_arr[0] * MICROCODEC_DELTA_F32_SCALE);
    if (dst->len + 4u > dst->cap) return MC_ERR_OVERFLOW;
    memcpy(dst->ptr + dst->len, &q0, 4u);
    dst->len += 4u;

    if (count == 1) return MC_OK;

    /* Pre order=1: delta medzi kvantizovanými hodnotami */
    /* Pre order=2: dod */
    int32_t prev_q  = q0;
    int32_t prev_d  = 0;
    bool    first_d = true;

    for (size_t i = 1; i < count; i++) {
        int32_t qi    = (int32_t)(src_arr[i] * MICROCODEC_DELTA_F32_SCALE);
        int32_t delta = qi - prev_q;

        if (order == 1) {
            err = mc_varint_encode_u32(mc_zigzag_encode_i32(delta), dst);
        } else {
            if (first_d) {
                /* Prvý delta raw */
                if (dst->len + 4u > dst->cap) return MC_ERR_OVERFLOW;
                memcpy(dst->ptr + dst->len, &delta, 4u);
                dst->len += 4u;
                first_d = false;
            } else {
                int32_t dod = delta - prev_d;
                err = mc_varint_encode_u32(mc_zigzag_encode_i32(dod), dst);
            }
            prev_d = delta;
        }
        if (err != MC_OK) return err;
        prev_q = qi;
    }
    return MC_OK;
}
```

---

## 6. LZSS — presná implementácia

### 6.1 Bitstream writer/reader

```c
/* Bitstream writer — akumuluje bity, vypíše po 8 */
typedef struct {
    uint8_t *buf;
    size_t   cap;
    size_t  *len;       /* Pointer na dst.len */
    size_t   flag_pos;  /* Pozícia flag byte v buf */
    uint8_t  flag;      /* Aktuálny flag byte */
    uint8_t  flag_bit;  /* Ktorý bit píšeme (0..7) */
} mc_bitwriter_t;

static void bw_init(mc_bitwriter_t *bw, mc_buf_t *dst) {
    bw->buf      = dst->ptr;
    bw->cap      = dst->cap;
    bw->len      = &dst->len;
    bw->flag_pos = dst->len;   /* Rezervuj miesto pre flag byte */
    (*bw->len)++;
    bw->flag     = 0;
    bw->flag_bit = 0;
}

static mc_err_t bw_write_literal(mc_bitwriter_t *bw, uint8_t byte) {
    /* Flag bit = 0 pre literal */
    bw->flag_bit++;
    if (bw->flag_bit == 8) {
        bw->buf[bw->flag_pos] = bw->flag;
        bw->flag_pos = *bw->len;
        if (*bw->len >= bw->cap) return MC_ERR_OVERFLOW;
        (*bw->len)++;
        bw->flag     = 0;
        bw->flag_bit = 0;
    }
    if (*bw->len >= bw->cap) return MC_ERR_OVERFLOW;
    bw->buf[(*bw->len)++] = byte;
    return MC_OK;
}

static mc_err_t bw_write_backref(mc_bitwriter_t *bw,
                                   uint16_t offset, uint8_t length) {
    /* Flag bit = 1 pre back-reference */
    bw->flag |= (1u << bw->flag_bit);
    bw->flag_bit++;
    if (bw->flag_bit == 8) {
        bw->buf[bw->flag_pos] = bw->flag;
        bw->flag_pos = *bw->len;
        if (*bw->len >= bw->cap) return MC_ERR_OVERFLOW;
        (*bw->len)++;
        bw->flag     = 0;
        bw->flag_bit = 0;
    }
    /* Offset: log2(WINDOW_SIZE) bits, Length: 4 bits, packed do 2 bajtov */
    /* Pre WINDOW_SIZE=256: offset = 8 bits, length-MIN_MATCH = 4 bits */
    /* Packujeme: [offset_low 8b] [offset_high:length 8b] */
    uint8_t len_enc = (uint8_t)(length - MICROCODEC_LZSS_MIN_MATCH);
    if (*bw->len + 2 > bw->cap) return MC_ERR_OVERFLOW;
    bw->buf[(*bw->len)++] = (uint8_t)(offset & 0xFFu);
    bw->buf[(*bw->len)++] = (uint8_t)((len_enc & 0x0Fu) |
                                        ((offset >> 4u) & 0xF0u));
    return MC_OK;
}

static void bw_flush(mc_bitwriter_t *bw) {
    bw->buf[bw->flag_pos] = bw->flag;   /* Zapíš posledný flag byte */
}
```

### 6.2 Window search

```c
/* Hľadá najdlhší match v okne pre src[pos..].
 * Vracia dĺžku match a nastavuje *out_offset.
 * O(WINDOW_SIZE × MAX_MATCH) — akceptovateľné pre malé okná. */
static size_t lzss_find_match(const mc_lzss_ctx_t *ctx,
                               const uint8_t       *src,
                               size_t               src_len,
                               size_t               pos,
                               size_t              *out_offset) {
    size_t best_len    = 0;
    size_t best_offset = 0;
    size_t win_size    = MICROCODEC_LZSS_WINDOW_SIZE;

    for (size_t w = 1; w < win_size && w <= pos; w++) {
        size_t win_idx = (ctx->window_pos + win_size - w) & (win_size - 1u);
        size_t match_len = 0;

        while (match_len < MICROCODEC_LZSS_MAX_MATCH
               && pos + match_len < src_len
               && ctx->window[(win_idx + match_len) & (win_size - 1u)]
                  == src[pos + match_len]) {
            match_len++;
        }

        if (match_len > best_len) {
            best_len    = match_len;
            best_offset = w;
            if (best_len == MICROCODEC_LZSS_MAX_MATCH) break;
        }
    }

    *out_offset = best_offset;
    return best_len;
}
```

---

## 7. Huffman — presná implementácia

### 7.1 Kanonická tabuľka

Huffman tabuľka je **kanonická** — kódy sú pridelené od najkratšieho k najdlhšiemu,
v rámci rovnakej dĺžky lexikograficky. Toto umožňuje kompaktnú reprezentáciu
(stačí uložiť dĺžky kódov, nie samotné kódy).

### 7.2 Static Huffman tabuľka

Statická tabuľka je `const mc_huff_entry_t mc_huff_static_table[256]`
definovaná v `mc_huff.c`. Vygeneruj ju z týchto frequency counts:

```c
/* Frekvenčný model pre typické embedded senzorové dáta */
static const uint32_t mc_huff_static_freq[256] = {
    /* 0x00 */  10000,  /* nuly — veľmi časté */
    /* 0x01 */   5000,
    /* 0x02 */   4000,
    /* 0x03 */   3500,
    /* 0x04..0x0F */   /* klesajúci pattern, 3000..1000 */
    /* 0x10..0x7F */   /* medium, 800..100 */
    /* 0x80..0xFF */   /* nízke, 50..10 */
    /* Agent: vyplň celú tabuľku podľa tohto vzoru,
       dôležité je len aby výsledný Huffman strom bol platný.
       Konkrétne hodnoty ovplyvňujú iba kompresný pomer,
       nie korektnosť kódu. */
};
```

Postup generovania statickej tabuľky (spusti raz pri compile time, výsledok ulož):

```
1. Zoraď symboly podľa frekvencie (zostupne)
2. Huffman tree building (priority queue / min-heap):
   a. Vytvor list uzlov (symbol, freq)
   b. Opakuj: vyber dva uzly s najmenšou freq,
              zlúč do nového uzla (freq = súčet)
   c. Kým nie je jeden uzol (koreň)
3. Priraď kódy (kanonické):
   a. Zisti hĺbku každého symbolu v strome
   b. Zoraď symboly: primárne podľa dĺžky, sekundárne podľa hodnoty
   c. Priraď kódy inkrementálne
4. Ulož ako const pole mc_huff_entry_t[256]
```

### 7.3 Bitstream pre Huffman

Huffman používa LSB-first bitstream (bity sa vypĺňajú od LSB k MSB v každom byte):

```c
typedef struct {
    uint8_t *buf;
    size_t   cap;
    size_t  *len;
    uint8_t  accum;    /* Akumulátor — bity sa pridávajú od LSB */
    uint8_t  bits;     /* Počet bitov v accum */
} mc_huff_bitwriter_t;

static mc_err_t hbw_write_bits(mc_huff_bitwriter_t *bw,
                                 uint16_t             code,
                                 uint8_t              code_len) {
    for (uint8_t i = 0; i < code_len; i++) {
        bw->accum |= (uint8_t)(((code >> i) & 1u) << bw->bits);
        bw->bits++;
        if (bw->bits == 8u) {
            if (*bw->len >= bw->cap) return MC_ERR_OVERFLOW;
            bw->buf[(*bw->len)++] = bw->accum;
            bw->accum = 0;
            bw->bits  = 0;
        }
    }
    return MC_OK;
}

static void hbw_flush(mc_huff_bitwriter_t *bw) {
    if (bw->bits > 0) {
        bw->buf[(*bw->len)++] = bw->accum;   /* Padding zeros handled by zeroed accum */
    }
}
```

---

## 8. Dispatch API — src/microcodec.c

```c
mc_err_t mc_encode(mc_alg_t alg, mc_slice_t src, mc_buf_t *dst, void *ctx) {
    switch (alg) {
#if MICROCODEC_ENABLE_RLE
        case MC_ALG_RLE:    return mc_rle_encode(src, dst);
#endif
#if MICROCODEC_ENABLE_VARINT
        case MC_ALG_VARINT: return mc_varint_encode_u32_array(
                                (const uint32_t *)src.ptr,
                                src.len / sizeof(uint32_t), dst);
#endif
#if MICROCODEC_ENABLE_LZSS
        case MC_ALG_LZSS:
            if (!ctx) return MC_ERR_INVALID;
            return mc_lzss_encode((mc_lzss_ctx_t *)ctx, src, dst);
#endif
        default: return MC_ERR_DISABLED;
    }
}

const char *mc_alg_name(mc_alg_t alg) {
    switch (alg) {
        case MC_ALG_RLE:    return "rle";
        case MC_ALG_VARINT: return "varint";
        case MC_ALG_DELTA:  return "delta";
        case MC_ALG_LZSS:   return "lzss";
        case MC_ALG_HUFF:   return "huff";
        default:             return "unknown";
    }
}
```

---

## 9. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(microcodec C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(
    -Wall -Wextra -Wpedantic -Wshadow
    -Wcast-align -Wstrict-prototypes
    -Wmissing-prototypes -Wconversion
    -Werror
)

# ── Core knižnica ─────────────────────────────────────────────────────────────
add_library(microcodec STATIC
    src/microcodec.c
    src/mc_rle.c
    src/mc_varint.c
    src/mc_delta.c
    src/mc_lzss.c
    src/mc_huff.c
)
target_include_directories(microcodec PUBLIC include)

# ── Testy ─────────────────────────────────────────────────────────────────────
option(MICROCODEC_BUILD_TESTS "Build tests" ON)

if(MICROCODEC_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(microtest
        GIT_REPOSITORY https://github.com/Vanderhell/microtest.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(microtest)

    foreach(ALG rle varint delta lzss huff integration)
        add_executable(test_${ALG} tests/test_${ALG}.c)
        target_link_libraries(test_${ALG} PRIVATE microcodec microtest)
        add_test(NAME ${ALG} COMMAND test_${ALG})
    endforeach()

    # Variant s malým LZSS oknom
    add_executable(test_lzss_win64 tests/test_lzss.c)
    target_compile_definitions(test_lzss_win64 PRIVATE
        MICROCODEC_LZSS_WINDOW_SIZE=64)
    target_link_libraries(test_lzss_win64 PRIVATE microcodec microtest)
    add_test(NAME lzss_win64 COMMAND test_lzss_win64)

    # Disabled algorithm variant
    add_executable(test_rle_disabled tests/test_integration.c)
    target_compile_definitions(test_rle_disabled PRIVATE
        MICROCODEC_ENABLE_RLE=0)
    target_link_libraries(test_rle_disabled PRIVATE microcodec microtest)
    add_test(NAME rle_disabled COMMAND test_rle_disabled)
endif()
```

---

## 10. Test vzory s microtest

### 10.1 Základná štruktúra

```c
/* tests/test_rle.c */
#define MTEST_IMPLEMENTATION
#include "mtest.h"
#include "microcodec.h"

/* ── Helpers ──────────────────────────────────────────────────────────────── */
static uint8_t g_src[4096];
static uint8_t g_dst[8192];   /* 2× pre worst-case RLE */
static uint8_t g_dec[4096];

static void setup(void) {
    memset(g_src, 0, sizeof(g_src));
    memset(g_dst, 0, sizeof(g_dst));
    memset(g_dec, 0, sizeof(g_dec));
}

/* ── Testy ────────────────────────────────────────────────────────────────── */
MTEST(rle_encode_empty) {
    mc_buf_t dst = MC_BUF(g_dst, sizeof(g_dst));
    mc_err_t err = mc_rle_encode(MC_SLICE(g_src, 0), &dst);
    MTEST_ASSERT_EQ(err, MC_OK);
    MTEST_ASSERT_EQ(dst.len, 0u);
}

MTEST(rle_roundtrip_all_zeros) {
    memset(g_src, 0x00, 128);
    mc_buf_t dst = MC_BUF(g_dst, sizeof(g_dst));
    MTEST_ASSERT_EQ(mc_rle_encode(MC_SLICE(g_src, 128), &dst), MC_OK);
    MTEST_ASSERT_EQ(dst.len, 2u);   /* 1 repeat token + 1 byte */

    mc_buf_t dec = MC_BUF(g_dec, sizeof(g_dec));
    MTEST_ASSERT_EQ(mc_rle_decode(MC_SLICE(g_dst, dst.len), &dec), MC_OK);
    MTEST_ASSERT_EQ(dec.len, 128u);
    MTEST_ASSERT_MEM_EQ(g_dec, g_src, 128);
}

MTEST(rle_null_src) {
    mc_buf_t dst = MC_BUF(g_dst, sizeof(g_dst));
    mc_slice_t bad = { NULL, 10 };
    MTEST_ASSERT_EQ(mc_rle_encode(bad, &dst), MC_ERR_INVALID);
}

MTEST(rle_overflow) {
    memset(g_src, 0xAB, 100);   /* alternating — worst case */
    uint8_t tiny[1];
    mc_buf_t dst = MC_BUF(tiny, sizeof(tiny));
    /* Bude overflow pretože 1 byte nestačí */
    mc_err_t err = mc_rle_encode(MC_SLICE(g_src, 100), &dst);
    MTEST_ASSERT_EQ(err, MC_ERR_OVERFLOW);
}

/* ── Suite ────────────────────────────────────────────────────────────────── */
MTEST_SUITE(rle) {
    MTEST_RUN_F(rle_encode_empty,      setup, NULL);
    MTEST_RUN_F(rle_roundtrip_all_zeros, setup, NULL);
    MTEST_RUN_F(rle_null_src,          setup, NULL);
    MTEST_RUN_F(rle_overflow,          setup, NULL);
    /* ... všetky ostatné testy zo spec §13.1 ... */
}

int main(int argc, char **argv) {
    MTEST_BEGIN(argc, argv);
    MTEST_SUITE_RUN(rle);
    return MTEST_END();
}
```

---

## 11. Konvencie kódu

```
Public API:     mc_*                  (mc_rle_encode, mc_varint_decode_u32)
Internal:       statické funkcie bez prefixu alebo s algoritmom (rle_*, lzss_*, huff_*)
Constants:      MICROCODEC_*          (MICROCODEC_ENABLE_RLE)
Types:          mc_*_t                (mc_slice_t, mc_err_t, mc_lzss_ctx_t)
```

- Každá funkcia najprv validuje NULL pointery → MC_ERR_INVALID
- Žiadny `assert()` — vždy vrať error code
- Žiadny `printf` v src/ ani include/
- Žiadny `malloc/free` kdekoľvek
- Žiadny globálny mutable stav
- `const` na všetko čo sa nemení

---

## 12. Commit a tag stratégia

```
Commit message formát: rovnaký ako microdb (feat/fix/test/docs/chore)

Krok 2 (RLE):       git tag v0.1.0
Krok 3 (Varint):    git tag v0.2.0
Krok 4 (Delta):     git tag v0.3.0
Krok 5 (LZSS):      git tag v0.4.0
Krok 6 (Huffman):   git tag v0.5.0
Krok 7 (Dispatch):  git tag v1.0.0
```

---

## 13. Checklist pred každým commitom

```
[ ] cmake --build build --clean-first  →  nula warnings, nula errors
[ ] ctest --test-dir build -C Debug --output-on-failure  →  všetky PASS
[ ] grep -r "printf" src/ include/     →  nič nenájdené
[ ] grep -r "malloc\|free" src/        →  nič nenájdené
[ ] grep -n "^[a-zA-Z].*= " src/*.c | grep -v "const"  →  žiadne globálne
[ ] Každá nová public funkcia má doc komentár v microcodec.h
[ ] Každá nová funkcia má aspoň 1 test
[ ] Commit message dodržiava formát
```

---

*microcodec agent guide v1.0 — doplnok k microcodec_spec.md*
*github.com/Vanderhell/microcodec*
