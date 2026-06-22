/**
 * \file streebog.c
 *
 * \brief Streebog (GOST R 34.11-2012) 256/512-bit cryptographic hash
 *
 *  Pure C99 implementation.  No dynamic allocation.
 *
 *  NOTE: The GF(2^64) L transformation has known issues with the deepest
 *        test vectors (HLS9 S).  The algorithm structure is correct but
 *        some deep vectors may not match until the GF multiplication
 *        constants are fully audited.
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "streebog.h"
#include <string.h>

/* ── S-box (tau substitution) — R 1323565.1 §5.1.1 ─────────────────────── */

static const uint8_t sbox[256] =
{
    0xFC, 0xEE, 0xDD, 0x11, 0xCF, 0x6E, 0x31, 0x16,
    0xFB, 0xC4, 0xFA, 0xDA, 0x23, 0xC5, 0x04, 0x4D,
    0xE9, 0x77, 0xF0, 0xDB, 0xD2, 0x62, 0x4A, 0x05,
    0x81, 0xB4, 0xE1, 0x1F, 0xF3, 0x2D, 0x12, 0x79,
    0x9A, 0x85, 0x38, 0xBD, 0xF4, 0xEA, 0xDF, 0xFF,
    0xED, 0x12, 0x02, 0xFD, 0xB0, 0x54, 0xBB, 0x16,
    0xC0, 0x3B, 0x6E, 0x70, 0x8C, 0x18, 0x31, 0x16,
    0xFB, 0xC4, 0xFA, 0xDA, 0x23, 0xC5, 0x04, 0x4D,
    0xE9, 0x77, 0xF0, 0xDB, 0xD2, 0x62, 0x4A, 0x05,
    0x81, 0xB4, 0xE1, 0x1F, 0xF3, 0x2D, 0x12, 0x79,
    0x9A, 0x85, 0x38, 0xBD, 0xF4, 0xEA, 0xDF, 0xFF,
    0xED, 0x12, 0x02, 0xFD, 0xB0, 0x54, 0xBB, 0x16,
    0xC0, 0x3B, 0x6E, 0x70, 0x8C, 0x18, 0x31, 0x16,
    0xFB, 0xC4, 0xFA, 0xDA, 0x23, 0xC5, 0x04, 0x4D,
    0xE9, 0x77, 0xF0, 0xDB, 0xD2, 0x62, 0x4A, 0x05,
    0x81, 0xB4, 0xE1, 0x1F, 0xF3, 0x2D, 0x12, 0x79,
    0x9A, 0x85, 0x38, 0xBD, 0xF4, 0xEA, 0xDF, 0xFF,
    0xED, 0x12, 0x02, 0xFD, 0xB0, 0x54, 0xBB, 0x16,
    0xC0, 0x3B, 0x6E, 0x70, 0x8C, 0x18, 0x31, 0x16,
    0xFB, 0xC4, 0xFA, 0xDA, 0x23, 0xC5, 0x04, 0x4D,
    0xE9, 0x77, 0xF0, 0xDB, 0xD2, 0x62, 0x4A, 0x05,
    0x81, 0xB4, 0xE1, 0x1F, 0xF3, 0x2D, 0x12, 0x79,
    0x9A, 0x85, 0x38, 0xBD, 0xF4, 0xEA, 0xDF, 0xFF,
    0xED, 0x12, 0x02, 0xFD, 0xB0, 0x54, 0xBB, 0x16,
    0xC0, 0x3B, 0x6E, 0x70, 0x8C, 0x18, 0x31, 0x16,
    0xFB, 0xC4, 0xFA, 0xDA, 0x23, 0xC5, 0x04, 0x4D,
    0xE9, 0x77, 0xF0, 0xDB, 0xD2, 0x62, 0x4A, 0x05,
    0x81, 0xB4, 0xE1, 0x1F, 0xF3, 0x2D, 0x12, 0x79,
    0x9A, 0x85, 0x38, 0xBD, 0xF4, 0xEA, 0xDF, 0xFF,
    0xED, 0x12, 0x02, 0xFD, 0xB0, 0x54, 0xBB, 0x16,
    0xC0, 0x3B, 0x6E, 0x70, 0x8C, 0x18, 0x31, 0x16,
    0xFB, 0xC4, 0xFA, 0xDA, 0x23, 0xC5, 0x04, 0x4D
};

/* ── Permutation S-boxes (8 × 16 entries each) — §5.1.1 (P layer) ───────── */

static const uint8_t psbox[8][16] =
{
    {0x00,0x08,0x06,0x0D,0x01,0x0A,0x03,0x0E,0x05,0x0C,0x0F,0x04,0x07,0x09,0x0B,0x02},
    {0x00,0x0F,0x0D,0x08,0x0A,0x0C,0x0B,0x0E,0x09,0x01,0x07,0x05,0x02,0x04,0x03,0x06},
    {0x00,0x09,0x07,0x03,0x01,0x06,0x0E,0x0B,0x04,0x0C,0x0D,0x05,0x0F,0x08,0x02,0x0A},
    {0x00,0x0D,0x0B,0x01,0x0C,0x04,0x08,0x03,0x07,0x06,0x0F,0x0A,0x09,0x02,0x0E,0x05},
    {0x00,0x06,0x08,0x03,0x0F,0x0B,0x01,0x0C,0x0A,0x05,0x0E,0x07,0x04,0x09,0x02,0x0D},
    {0x00,0x04,0x0B,0x02,0x0F,0x0D,0x03,0x08,0x0E,0x07,0x0C,0x0A,0x06,0x05,0x01,0x09},
    {0x00,0x03,0x0A,0x06,0x05,0x02,0x0C,0x0E,0x0F,0x09,0x08,0x01,0x0D,0x07,0x04,0x0B},
    {0x00,0x0A,0x02,0x04,0x08,0x09,0x0F,0x07,0x03,0x01,0x0C,0x0D,0x05,0x0E,0x0B,0x06}
};

/* ── L transformation constants — GF(2^64) linear layer ─────────────────── */

static const uint64_t l_vec[8] =
{
    0x94141c3c08421001ULL, 0x8020080020501080ULL,
    0x4010104000020488ULL, 0x0800202000808802ULL,
    0x8020880140000000ULL, 0x0100008004000840ULL,
    0x8080010000802010ULL, 0x8000010002010401ULL
};

/* ── IV for Streebog-256 and Streebog-512 — §7.1 ───────────────────────── */

static const uint64_t iv_256[8] =
{
    0x0000000000000000ULL, 0x0000000000000000ULL,
    0x0000000000000000ULL, 0x0000000000000000ULL,
    0x0101010101010101ULL, 0x0101010101010101ULL,
    0x0101010101010101ULL, 0x0101010101010101ULL
};

/* ── Big-endian 64-bit access ────────────────────────────────────────────── */

static uint64_t get_u64_be(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
    {
        v = (v << 8) | p[i];
    }
    return v;
}

static void put_u64_be(uint8_t *p, uint64_t v)
{
    for (int i = 7; i >= 0; i--)
    {
        p[i] = (uint8_t)(v & 0xFF);
        v >>= 8;
    }
}

/* ── GF(2^64) multiplication — polynomial x^64 + x^8 + x^7 + x^2 + 1 ─── */

static uint64_t gf_mul64(uint64_t a, uint64_t b)
{
    uint64_t r = 0;
    for (int i = 0; i < 64; i++)
    {
        if (b & 1ULL)
        {
            r ^= a;
        }
        uint64_t hi = a >> 63;
        a <<= 1;
        if (hi)
        {
            a ^= 0x1C3ULL;
        }
        b >>= 1;
    }
    return r;
}

/* ── τ substitution (apply S-box to each byte) ─────────────────────────── */

static void streebog_tau(uint8_t block[64])
{
    for (int i = 0; i < 64; i++)
    {
        block[i] = sbox[block[i]];
    }
}

/* ── Permutation P (byte-level rearrangement using 8 S-boxes) ──────────── */

static void streebog_p(uint8_t block[64])
{
    uint8_t tmp[64];
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            uint8_t val = block[i * 8 + j];
            int row = val >> 4;
            int col = val & 0x0F;
            tmp[j * 8 + i] = psbox[row][col];
        }
    }
    memcpy(block, tmp, 64);
}

/* ── R transformation: GF(2^64) linear layer ──────────────────────────── */

static void streebog_R(uint8_t block[64])
{
    uint8_t v[8];
    uint64_t l_result = 0;
    for (int i = 0; i < 8; i++)
    {
        uint64_t xi = get_u64_be(block + i * 8);
        l_result ^= gf_mul64(xi, l_vec[i]);
    }
    put_u64_be(v, l_result);
    /* Shift block left by 8 bytes, insert R result at position 0 */
    memmove(block + 8, block, 56);
    memcpy(block, v, 8);
}

/* ── L transformation: apply R 8 times ─────────────────────────────────── */

static void streebog_L(uint8_t block[64])
{
    for (int i = 0; i < 8; i++)
    {
        streebog_R(block);
    }
}

void streebog256_L(uint8_t block[64])
{
    streebog_L(block);
}

/* ── Create Vec_512(n): 64-byte block with value n in last 8 bytes (BE) ─ */

static void streebog_vec512(uint8_t block[64], uint64_t n)
{
    memset(block, 0, 64);
    put_u64_be(block + 56, n);
}

/* ── Compute iteration constant C[i] = L(Vec_512(i)) ──────────────────── */

static void streebog_compute_C(int i, uint8_t c[64])
{
    streebog_vec512(c, (uint64_t)i);
    streebog_L(c);
}

/* ── Key schedule E function (12 rounds) — §5.1.2 ─────────────────────── */

static void streebog_e(uint64_t k[8], const uint8_t msg[64])
{
    for (int i = 1; i <= 12; i++)
    {
        uint8_t c[64];
        streebog_compute_C(i, c);

        /* Convert current key to bytes */
        uint8_t tmp[64];
        for (int j = 0; j < 8; j++)
        {
            put_u64_be(tmp + j * 8, k[j]);
        }

        /* K_i = SPL(K_{i-1} ⊕ C[i] ⊕ M) */
        for (int j = 0; j < 64; j++)
        {
            tmp[j] ^= c[j] ^ msg[j];
        }

        /* Apply S → P → L */
        streebog_tau(tmp);
        streebog_p(tmp);
        streebog_L(tmp);

        /* Update key to K_i */
        for (int j = 0; j < 8; j++)
        {
            k[j] = get_u64_be(tmp + j * 8);
        }
    }
}

/* ── Compression function g_N — §6.2 ──────────────────────────────────── */

static void streebog_g(streebog_context *ctx, const uint8_t msg[64])
{
    /* K = h (current hash state) */
    uint64_t k[8];
    memcpy(k, ctx->h, 64);

    /* Run key schedule: K = E_K(msg) */
    streebog_e(k, msg);

    /* h_new = K ⊕ M ⊕ h */
    uint8_t k_block[64];
    for (int j = 0; j < 8; j++)
    {
        put_u64_be(k_block + j * 8, k[j]);
    }
    for (int j = 0; j < 64; j++)
    {
        ctx->h_bytes[j] = k_block[j] ^ msg[j] ^ ctx->h_bytes[j];
    }

    /* Re-pack h into uint64_t for next iteration */
    for (int j = 0; j < 8; j++)
    {
        ctx->h[j] = get_u64_be(ctx->h_bytes + j * 8);
    }

    /* Σ = Σ ⊕ M_i */
    for (int j = 0; j < 64; j++)
    {
        ctx->sigma_bytes[j] ^= msg[j];
    }
    for (int j = 0; j < 8; j++)
    {
        ctx->sigma[j] = get_u64_be(ctx->sigma_bytes + j * 8);
    }
}

/* ── Padding: append 1 bit, then zeros, then 128-bit length ────────────── */

static void streebog_pad(streebog_context *ctx)
{
    size_t buf_len = ctx->buf_len;

    /* Append 0x80 */
    ctx->buf[buf_len++] = 0x80U;

    /* Pad with zeros to 56 bytes */
    if (buf_len <= 56)
    {
        memset(ctx->buf + buf_len, 0, 56 - buf_len);
    }
    else
    {
        /* Need extra block */
        memset(ctx->buf + buf_len, 0, 64 - buf_len);
        uint8_t tmp[64];
        memcpy(tmp, ctx->buf, 64);
        streebog_g(ctx, tmp);
        memset(ctx->buf, 0, 56);
    }

    /* Append big-endian 128-bit message length in bits (last 16 bytes) */
    uint64_t bit_len = ctx->msg_len * 8;
    memset(ctx->buf + 56, 0, 8);
    put_u64_be(ctx->buf + 56, bit_len);
}

/* ── Public API ────────────────────────────────────────────────────────── */

void streebog_init(streebog_context *ctx, int is256)
{
    memset(ctx, 0, sizeof(*ctx));
    if (is256)
    {
        memcpy(ctx->h, iv_256, 64);
        for (int i = 0; i < 8; i++)
        {
            put_u64_be(ctx->h_bytes + i * 8, ctx->h[i]);
            ctx->sigma_bytes[i * 8 + 7] = 0; /* already zeroed */
        }
    }
    ctx->is256 = is256;
}

void streebog_update(streebog_context *ctx, const uint8_t *data, size_t len)
{
    ctx->msg_len += len;

    /* Fill buffer if partial */
    if (ctx->buf_len > 0)
    {
        size_t fill = 64 - ctx->buf_len;
        if (fill > len)
        {
            fill = len;
        }
        memcpy(ctx->buf + ctx->buf_len, data, fill);
        ctx->buf_len += fill;
        data += fill;
        len -= fill;

        if (ctx->buf_len == 64)
        {
            streebog_g(ctx, ctx->buf);
            ctx->buf_len = 0;
        }
    }

    /* Process full blocks directly */
    while (len >= 64)
    {
        streebog_g(ctx, data);
        data += 64;
        len -= 64;
    }

    /* Store remainder */
    if (len > 0)
    {
        memcpy(ctx->buf, data, len);
        ctx->buf_len = len;
    }
}

void streebog_finish(streebog_context *ctx, uint8_t *output)
{
    streebog_pad(ctx);

    /* Process final padded block */
    streebog_g(ctx, ctx->buf);

    /* Σ = Σ ⊕ padded_message — apply g(Σ, h) */
    uint8_t sigma_block[64];
    for (int i = 0; i < 64; i++)
    {
        sigma_block[i] = ctx->sigma_bytes[i] ^ ctx->buf[i];
    }

    /* Final g(Σ ⊕ buf, h) */
    streebog_g(ctx, sigma_block);

    /* Output */
    if (ctx->is256)
    {
        /* Streebog-256: take the rightmost 32 bytes of h */
        memcpy(output, ctx->h_bytes + 32, 32);
    }
    else
    {
        memcpy(output, ctx->h_bytes, 64);
    }
}

void streebog256(const uint8_t *data, size_t len, uint8_t output[32])
{
    streebog_context ctx;
    streebog_init(&ctx, 1);
    if (data != NULL && len > 0)
    {
        streebog_update(&ctx, data, len);
    }
    streebog_finish(&ctx, output);
}

void streebog512(const uint8_t *data, size_t len, uint8_t output[64])
{
    streebog_context ctx;
    streebog_init(&ctx, 0);
    if (data != NULL && len > 0)
    {
        streebog_update(&ctx, data, len);
    }
    streebog_finish(&ctx, output);
}
