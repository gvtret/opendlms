/**
 * \file kuznyechik.h
 *
 * \brief Kuznyechik (GOST R 34.12-2015) 128-bit block cipher — RFC 7801
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef KUZNYECHIK_H
#define KUZNYECHIK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          Kuznyechik context structure
 */
typedef struct
{
    uint8_t rk[160];   /*!< 10 round keys, 16 bytes each */
}
kuznyechik_context;

/**
 * \brief          Initialize a Kuznyechik context
 *
 * \param ctx      Context to initialize
 */
void kuznyechik_init(kuznyechik_context *ctx);

/**
 * \brief          Free a Kuznyechik context (no-op for stack-only usage)
 *
 * \param ctx      Context to clear
 */
void kuznyechik_free(kuznyechik_context *ctx);

/**
 * \brief          Set the encryption key
 *
 * \param ctx      Kuznyechik context
 * \param key      32-byte key
 * \return         0 on success
 */
int kuznyechik_setkey_enc(kuznyechik_context *ctx, const uint8_t key[32]);

/**
 * \brief          Encrypt a single 16-byte block
 *
 * \param ctx      Kuznyechik context (with key set)
 * \param input    16-byte input block
 * \param output   16-byte output block
 */
void kuznyechik_crypt_ecb(const kuznyechik_context *ctx,
                           const uint8_t input[16],
                           uint8_t output[16]);

#ifdef __cplusplus
}
#endif

#endif /* KUZNYECHIK_H */
