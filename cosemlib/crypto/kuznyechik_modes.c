/**
 * \file kuznyechik_modes.c
 *
 * \brief Kuznyechik-CMAC (GOST R 34.13-2015) and Kuznyechik-CTR modes
 *
 *  Pure C99 implementation.  No dynamic allocation.
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "kuznyechik_modes.h"
#include <string.h>

/* ── Subkeys for CMAC (RFC 7801 + GOST R 34.13-2015) ────────────────────── */

static void cmac_subkeys(const kuznyechik_context *cipher,
                          uint8_t K1[16], uint8_t K2[16])
{
    uint8_t L[16];

    /* L = E_K(0^128) */
    kuznyechik_crypt_ecb(cipher, L, L);

    /* K1 = L << 1 (left-shift by one bit, MSB out → XOR with Rb) */
    memcpy(K1, L, 16);
    uint8_t msb = K1[0] >> 7;
    for (int i = 0; i < 15; i++)
    {
        K1[i] = (uint8_t)((K1[i] << 1) | (K1[i + 1] >> 7));
    }
    K1[15] = (uint8_t)(K1[15] << 1);
    if (msb)
    {
        K1[15] ^= 0x87U;   /* Rb for 128-bit block */
    }

    /* K2 = K1 << 1 */
    memcpy(K2, K1, 16);
    msb = K2[0] >> 7;
    for (int i = 0; i < 15; i++)
    {
        K2[i] = (uint8_t)((K2[i] << 1) | (K2[i + 1] >> 7));
    }
    K2[15] = (uint8_t)(K2[15] << 1);
    if (msb)
    {
        K2[15] ^= 0x87U;
    }
}

/* ── CMAC API ─────────────────────────────────────────────────────────────── */

int kuznyechik_cmac_init(kuznyechik_cmac_context *ctx, const uint8_t key[32])
{
    int ret = kuznyechik_setkey_enc(&ctx->cipher, key);
    if (ret != 0)
    {
        return ret;
    }
    memset(ctx->state, 0, 16);
    ctx->buf_len = 0;
    return 0;
}

void kuznyechik_cmac_update(kuznyechik_cmac_context *ctx,
                             const uint8_t *data, size_t len)
{
    while (len > 0)
    {
        size_t fill = 16 - ctx->buf_len;
        if (fill > len)
        {
            fill = len;
        }
        memcpy(ctx->buf + ctx->buf_len, data, fill);
        ctx->buf_len += fill;
        data += fill;
        len -= fill;

        if (ctx->buf_len == 16)
        {
            for (int i = 0; i < 16; i++)
            {
                ctx->state[i] ^= ctx->buf[i];
            }
            kuznyechik_crypt_ecb(&ctx->cipher, ctx->state, ctx->state);
            ctx->buf_len = 0;
        }
    }
}

void kuznyechik_cmac_finish(kuznyechik_cmac_context *ctx, uint8_t tag[16])
{
    uint8_t K1[16], K2[16];
    cmac_subkeys(&ctx->cipher, K1, K2);

    uint8_t *subkey;
    if (ctx->buf_len == 16)
    {
        subkey = K1;
    }
    else
    {
        /* Pad incomplete block with 0x80 then zeros */
        ctx->buf[ctx->buf_len] = 0x80U;
        memset(ctx->buf + ctx->buf_len + 1, 0, 15 - ctx->buf_len);
        subkey = K2;
    }

    for (int i = 0; i < 16; i++)
    {
        ctx->state[i] ^= ctx->buf[i] ^ subkey[i];
    }
    kuznyechik_crypt_ecb(&ctx->cipher, ctx->state, tag);
}

void kuznyechik_cmac(const uint8_t key[32],
                      const uint8_t *data, size_t len,
                      uint8_t tag[16])
{
    kuznyechik_cmac_context ctx;
    kuznyechik_cmac_init(&ctx, key);
    kuznyechik_cmac_update(&ctx, data, len);
    kuznyechik_cmac_finish(&ctx, tag);
}

/* ── CTR mode API ─────────────────────────────────────────────────────────── */

int kuznyechik_ctr_init(kuznyechik_ctr_context *ctx,
                         const uint8_t key[32],
                         const uint8_t nonce[12])
{
    int ret = kuznyechik_setkey_enc(&ctx->cipher, key);
    if (ret != 0)
    {
        return ret;
    }
    /* 4-byte zero prefix + 12-byte nonce */
    memset(ctx->ctr, 0, 4);
    memcpy(ctx->ctr + 4, nonce, 12);
    return 0;
}

void kuznyechik_ctr_crypt(kuznyechik_ctr_context *ctx,
                           const uint8_t *input,
                           uint8_t *output, size_t len)
{
    uint8_t keystream[16];
    size_t pos = 0;

    while (pos < len)
    {
        /* Generate one keystream block */
        kuznyechik_crypt_ecb(&ctx->cipher, ctx->ctr, keystream);

        /* XOR with input */
        size_t block_len = len - pos;
        if (block_len > 16)
        {
            block_len = 16;
        }
        for (size_t i = 0; i < block_len; i++)
        {
            output[pos + i] = input[pos + i] ^ keystream[i];
        }
        pos += block_len;

        /* Increment counter (big-endian) */
        for (int i = 15; i >= 0; i--)
        {
            if (++ctx->ctr[i] != 0U)
            {
                break;
            }
        }
    }
}
