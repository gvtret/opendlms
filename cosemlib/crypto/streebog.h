/**
 * \file streebog.h
 *
 * \brief Streebog (GOST R 34.11-2012) 256-bit cryptographic hash
 *
 *  Pure C99 implementation.  No dynamic allocation.
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef STREEBOG_H
#define STREEBOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STREEBOG256_DIGEST_SIZE  32
#define STREEBOG512_DIGEST_SIZE  64
#define STREEBOG_BLOCK_SIZE      64

/**
 * \brief          Streebog hash context
 */
typedef struct
{
    uint64_t h[8];          /*!< Hash state (8 x 64-bit words) */
    uint64_t sigma[8];      /*!< Running sum of message blocks */
    uint8_t  h_bytes[64];   /*!< Hash state as raw bytes */
    uint8_t  sigma_bytes[64]; /*!< Sigma as raw bytes */
    uint8_t  buf[64];       /*!< Partial block buffer */
    size_t   buf_len;       /*!< Bytes in buffer */
    size_t   msg_len;       /*!< Total message length in bytes */
    uint64_t block_count;   /*!< Number of full blocks processed */
    int      is256;         /*!< 1 = Streebog-256, 0 = Streebog-512 */
}
streebog_context;

/**
 * \brief          Initialize a Streebog context
 *
 * \param ctx      Context to initialize
 * \param is256    1 for 256-bit output, 0 for 512-bit
 */
void streebog_init(streebog_context *ctx, int is256);

/**
 * \brief          Feed data into the hash
 *
 * \param ctx      Hash context
 * \param data     Input data
 * \param len      Length of input data in bytes
 */
void streebog_update(streebog_context *ctx, const uint8_t *data, size_t len);

/**
 * \brief          Finalize hash and write digest
 *
 * \param ctx      Hash context
 * \param output   32-byte (Streebog-256) or 64-byte (Streebog-512) output
 */
void streebog_finish(streebog_context *ctx, uint8_t *output);

/**
 * \brief          One-shot Streebog-256 hash
 *
 * \param data     Input data (NULL for empty hash)
 * \param len      Length of input data in bytes
 * \param output   32-byte output digest
 */
void streebog256(const uint8_t *data, size_t len, uint8_t output[32]);

/**
 * \brief          One-shot Streebog-512 hash
 *
 * \param data     Input data (NULL for empty hash)
 * \param len      Length of input data in bytes
 * \param output   64-byte output digest
 */
void streebog512(const uint8_t *data, size_t len, uint8_t output[64]);

/**
 * \brief          Expose the L transformation for testing
 *
 * \param block    64-byte block to transform in-place
 */
void streebog256_L(uint8_t block[64]);

#ifdef __cplusplus
}
#endif

#endif /* STREEBOG_H */
