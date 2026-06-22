/**
 * \file kuznyechik_modes.h
 *
 * \brief Kuznyechik-CMAC and Kuznyechik-CTR modes
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef KUZNYECHIK_MODES_H
#define KUZNYECHIK_MODES_H

#include "kuznyechik.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          Kuznyechik-CMAC context structure
 */
typedef struct
{
    kuznyechik_context cipher;  /*!< Block cipher context */
    uint8_t state[16];          /*!< Current CMAC state */
    uint8_t buf[16];            /*!< Partial block buffer */
    size_t  buf_len;            /*!< Bytes in buffer */
}
kuznyechik_cmac_context;

/**
 * \brief          Initialize a Kuznyechik-CMAC context
 *
 * \param ctx      CMAC context to initialize
 * \param key      32-byte key
 * \return         0 on success
 */
int kuznyechik_cmac_init(kuznyechik_cmac_context *ctx, const uint8_t key[32]);

/**
 * \brief          Feed data into the CMAC computation
 *
 * \param ctx      CMAC context
 * \param data     Input data
 * \param len      Length of input data in bytes
 */
void kuznyechik_cmac_update(kuznyechik_cmac_context *ctx,
                             const uint8_t *data, size_t len);

/**
 * \brief          Finalize CMAC and write 16-byte tag
 *
 * \param ctx      CMAC context
 * \param tag      16-byte output tag
 */
void kuznyechik_cmac_finish(kuznyechik_cmac_context *ctx, uint8_t tag[16]);

/**
 * \brief          One-shot Kuznyechik-CMAC computation
 *
 * \param key      32-byte key
 * \param data     Input data
 * \param len      Length of input data in bytes
 * \param tag      16-byte output tag
 */
void kuznyechik_cmac(const uint8_t key[32],
                      const uint8_t *data, size_t len,
                      uint8_t tag[16]);

/**
 * \brief          Kuznyechik-CTR context structure
 */
typedef struct
{
    kuznyechik_context cipher;  /*!< Block cipher context */
    uint8_t ctr[16];            /*!< Current counter value */
}
kuznyechik_ctr_context;

/**
 * \brief          Initialize a Kuznyechik-CTR context
 *
 * \param ctx      CTR context to initialize
 * \param key      32-byte key
 * \param nonce    12-byte nonce (padded to 16 bytes internally)
 * \return         0 on success
 */
int kuznyechik_ctr_init(kuznyechik_ctr_context *ctx,
                         const uint8_t key[32],
                         const uint8_t nonce[12]);

/**
 * \brief          Encrypt/decrypt data in CTR mode (XOR with keystream)
 *
 * \param ctx      CTR context
 * \param input    Input data
 * \param output   Output data (can overlap with input)
 * \param len      Length of data in bytes
 */
void kuznyechik_ctr_crypt(kuznyechik_ctr_context *ctx,
                           const uint8_t *input,
                           uint8_t *output, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* KUZNYECHIK_MODES_H */
