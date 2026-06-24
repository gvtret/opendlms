/**
 * \file csm_block_transfer.h
 * \brief Block Transfer (GBT) support for DLMS/COSEM
 *
 *  Implements IEC 62056-5-3 block transfer:
 *  - Get-Response-With-DataBlock (tag 04)
 *  - Set-Request-With-DataBlock (type 2)
 *  - Server-side buffering and block state management
 *  - Client-side block encoding for SET
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef CSM_BLOCK_TRANSFER_H
#define CSM_BLOCK_TRANSFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "csm_array.h"
#include "csm_definitions.h"

/* ── Limits ─────────────────────────────────────────────────────────────── */

#define CSM_MAX_BLOCK_SIZE      512U   ///< Default block size for transfer
#define CSM_MAX_BLOCK_TRANSFERS 64U    ///< Max concurrent block transfers per association

/* ── Block Transfer State ───────────────────────────────────────────────── */

/**
 * \brief Block transfer direction
 */
typedef enum
{
    CSM_BLOCK_DIR_NONE = 0U,        ///< No active transfer
    CSM_BLOCK_DIR_SERVER_TO_CLIENT, ///< Server sending data to client (GET)
    CSM_BLOCK_DIR_CLIENT_TO_SERVER  ///< Client sending data to server (SET)
} csm_block_direction;

/**
 * \brief Active block transfer state per association
 *
 *  Tracks one in-progress block transfer. When a GET response
 *  exceeds the PDU size, the server stores the full response
 *  and sends it in blocks via Get-Response-With-DataBlock.
 *
 *  For SET: tracks client sending data in blocks to server.
 */
typedef struct
{
    csm_block_direction direction;  ///< Transfer direction
    uint32_t block_number;          ///< Current block number (0-based)
    uint32_t total_size;            ///< Total data size in bytes
    uint32_t offset;                ///< Current offset into buffer
    uint32_t block_size;            ///< Max bytes per block
    uint8_t invoke_id;              ///< Invoke ID for this transfer
    uint8_t last_block;             ///< 1 if this is the last block
    uint8_t active;                 ///< 1 if transfer is active
    const uint8_t *data;            ///< Pointer to data buffer (server-owned for GET, client-owned for SET)
} csm_block_state;

/* ── Server-Side Block Transfer API ─────────────────────────────────────── */

/**
 * \brief Initialize block transfer state
 */
void csm_block_init(csm_block_state *state);

/**
 * \brief Start a new block transfer (server → client)
 *
 *  Called when a GET response exceeds PDU size. The server stores
 *  the data pointer and prepares to send blocks.
 *
 * \param state        Block transfer state to initialize
 * \param invoke_id    Invoke ID from the original request
 * \param data         Pointer to the complete response data
 * \param data_size    Total size of the response data
 * \param block_size   Maximum bytes per block (0 = use default)
 * \return 0 on success
 */
int csm_block_start_server(csm_block_state *state, uint8_t invoke_id,
                           const uint8_t *data, uint32_t data_size,
                           uint32_t block_size);

/**
 * \brief Encode the first block of a server → client transfer
 *
 *  Encodes: AXDR_GET_RESPONSE | 04 | invoke_id | last_block | block_number | data...
 *
 * \param state     Block transfer state
 * \param array     Output array to encode into
 * \param max_size  Maximum data bytes to include in this block
 * \return 1 on success, 0 on error
 */
int csm_block_encode_first(csm_block_state *state, csm_array *array, uint32_t max_size);

/**
 * \brief Encode the next block of a server → client transfer
 *
 *  Called in response to Get-Request-Next. Encodes the next chunk of data.
 *
 * \param state     Block transfer state
 * \param array     Output array to encode into
 * \param max_size  Maximum data bytes to include in this block
 * \return 1 on success, 0 on error
 */
int csm_block_encode_next(csm_block_state *state, csm_array *array, uint32_t max_size);

/**
 * \brief Check if a block transfer is active
 */
int csm_block_is_active(const csm_block_state *state);

/**
 * \brief Abort an active block transfer
 */
void csm_block_abort(csm_block_state *state);

/* ── Client-Side Block Transfer API (SET) ───────────────────────────────── */

/**
 * \brief Start a client-side block transfer (client → server)
 *
 *  Called when a SET request data exceeds PDU size. The client stores
 *  the data pointer and prepares to send blocks.
 *
 * \param state        Block transfer state to initialize
 * \param invoke_id    Invoke ID for the request
 * \param data         Pointer to the data to send
 * \param data_size    Total size of the data
 * \param block_size   Maximum bytes per block (0 = use default)
 * \return 1 on success, 0 on error
 */
int csm_block_start_client(csm_block_state *state, uint8_t invoke_id,
                           const uint8_t *data, uint32_t data_size,
                           uint32_t block_size);

/**
 * \brief Encode a SET request with block transfer
 *
 *  Encodes: AXDR_SET_REQUEST | 02 | invoke_id | last_block | block_number | class_id | obis | id | [sel_access] | data...
 *
 * \param state       Block transfer state
 * \param array       Output array to encode into
 * \param request     Original request with object info (class_id, obis, id)
 * \param max_size    Maximum data bytes to include in this block
 * \return 1 on success, 0 on error
 */
int csm_block_encode_set_request(csm_block_state *state, csm_array *array,
                                 const csm_request *request, uint32_t max_size);

/**
 * \brief Encode the next SET request block
 *
 * \param state     Block transfer state
 * \param array     Output array to encode into
 * \param max_size  Maximum data bytes to include in this block
 * \return 1 on success, 0 on error
 */
int csm_block_encode_set_next(csm_block_state *state, csm_array *array, uint32_t max_size);

/* ── Server-Side SET Block Transfer API ─────────────────────────────────── */

/**
 * \brief Start accumulating a SET block transfer from client
 *
 *  Called when server receives first block of SET-Request-With-DataBlock.
 *  Allocates/reuses buffer to accumulate incoming data.
 *
 * \param state        Block transfer state
 * \param invoke_id    Invoke ID from the request
 * \param block_size   Expected block size (0 = use default)
 * \return 1 on success, 0 on error
 */
int csm_block_start_receive(csm_block_state *state, uint8_t invoke_id,
                            uint32_t block_size);

/**
 * \brief Add received data block to the accumulation buffer
 *
 *  Called for each incoming block (first and subsequent).
 *
 * \param state     Block transfer state
 * \param data      Data chunk to add
 * \param data_size Size of the data chunk
 * \param is_last   1 if this is the last block
 * \return 1 on success, 0 on error
 */
int csm_block_receive_data(csm_block_state *state, const uint8_t *data,
                           uint32_t data_size, uint8_t is_last);

/**
 * \brief Get accumulated data after transfer complete
 *
 * \param state     Block transfer state
 * \param data      Pointer to receive data pointer
 * \param data_size Pointer to receive total size
 * \return 1 if data available, 0 if transfer not complete
 */
int csm_block_get_received(const csm_block_state *state, const uint8_t **data,
                           uint32_t *data_size);

/**
 * \brief Encode SET response acknowledgment (block received)
 *
 * \param state     Block transfer state
 * \param array     Output array to encode into
 * \return 1 on success, 0 on error
 */
int csm_block_encode_set_response(csm_block_state *state, csm_array *array);

/**
 * \brief Check if we can accept more blocks (buffer not full)
 */
int csm_block_can_receive(const csm_block_state *state);

#ifdef __cplusplus
}
#endif

#endif /* CSM_BLOCK_TRANSFER_H */
