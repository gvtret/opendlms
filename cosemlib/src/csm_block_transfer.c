/**
 * \file csm_block_transfer.c
 * \brief Block Transfer (GBT) implementation
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "csm_block_transfer.h"
#include "csm_services.h"
#include "csm_axdr_codec.h"
#include <string.h>

/* ── AXDR tags ──────────────────────────────────────────────────────────── */

#define AXDR_GET_RESPONSE_NORMAL        0x01U
#define AXDR_GET_RESPONSE_WITH_BLOCK    0x04U
#define AXDR_SET_REQUEST_WITH_BLOCK     0x02U
#define AXDR_SET_RESPONSE_WITH_BLOCK    0x02U
#define AXDR_SET_REQUEST_NORMAL         0x01U

/* ── Public API ─────────────────────────────────────────────────────────── */

void csm_block_init(csm_block_state *state)
{
    if (state != NULL)
    {
        state->direction = CSM_BLOCK_DIR_NONE;
        state->block_number = 0U;
        state->total_size = 0U;
        state->offset = 0U;
        state->block_size = CSM_MAX_BLOCK_SIZE;
        state->invoke_id = 0U;
        state->last_block = 0U;
        state->active = 0U;
        state->data = NULL;
    }
}

/* ── Server → Client (GET block transfer) ───────────────────────────────── */

int csm_block_start_server(csm_block_state *state, uint8_t invoke_id,
                           const uint8_t *data, uint32_t data_size,
                           uint32_t block_size)
{
    if ((state == NULL) || (data == NULL) || (data_size == 0U))
    {
        return 0;
    }

    state->direction = CSM_BLOCK_DIR_SERVER_TO_CLIENT;
    state->block_number = 0U;
    state->total_size = data_size;
    state->offset = 0U;
    state->block_size = (block_size > 0U) ? block_size : CSM_MAX_BLOCK_SIZE;
    state->invoke_id = invoke_id;
    state->last_block = 0U;
    state->active = 1U;
    state->data = data;

    return 1;
}

int csm_block_encode_first(csm_block_state *state, csm_array *array, uint32_t max_size)
{
    if ((state == NULL) || (array == NULL) || (!state->active))
    {
        return 0;
    }

    /* Calculate how much data to send in this block */
    uint32_t remaining = state->total_size - state->offset;
    uint32_t chunk = (remaining < max_size) ? remaining : max_size;
    uint32_t chunk_block = (remaining < state->block_size) ? remaining : state->block_size;
    if (chunk > chunk_block)
    {
        chunk = chunk_block;
    }

    /* Check if this is the last block */
    state->last_block = ((state->offset + chunk) >= state->total_size) ? 1U : 0U;

    /* Encode: AXDR_GET_RESPONSE | 04 (with block) | invoke_id | last_block | block_number (4 bytes) | data */
    int valid = csm_array_write_u8(array, AXDR_GET_RESPONSE);
    valid = valid && csm_array_write_u8(array, AXDR_GET_RESPONSE_WITH_BLOCK);
    valid = valid && csm_array_write_u8(array, state->invoke_id);
    valid = valid && csm_array_write_u8(array, state->last_block);
    valid = valid && csm_array_write_u32(array, state->block_number);

    /* Copy data chunk */
    if (valid && (chunk > 0U))
    {
        valid = csm_array_write_buff(array, &state->data[state->offset], chunk);
    }

    if (valid)
    {
        state->offset += chunk;
        state->block_number++;

        /* Deactivate if this was the last block */
        if (state->last_block)
        {
            state->active = 0U;
        }
    }

    return valid;
}

int csm_block_encode_next(csm_block_state *state, csm_array *array, uint32_t max_size)
{
    if ((state == NULL) || (array == NULL) || (!state->active))
    {
        return 0;
    }

    /* Calculate how much data to send in this block */
    uint32_t remaining = state->total_size - state->offset;
    uint32_t chunk = (remaining < max_size) ? remaining : max_size;
    uint32_t chunk_block = (remaining < state->block_size) ? remaining : state->block_size;
    if (chunk > chunk_block)
    {
        chunk = chunk_block;
    }

    /* Check if this is the last block */
    state->last_block = ((state->offset + chunk) >= state->total_size) ? 1U : 0U;

    /* Encode: AXDR_GET_RESPONSE | 04 (with block) | invoke_id | last_block | block_number (4 bytes) | data */
    int valid = csm_array_write_u8(array, AXDR_GET_RESPONSE);
    valid = valid && csm_array_write_u8(array, AXDR_GET_RESPONSE_WITH_BLOCK);
    valid = valid && csm_array_write_u8(array, state->invoke_id);
    valid = valid && csm_array_write_u8(array, state->last_block);
    valid = valid && csm_array_write_u32(array, state->block_number);

    /* Copy data chunk */
    if (valid && (chunk > 0U))
    {
        valid = csm_array_write_buff(array, &state->data[state->offset], chunk);
    }

    if (valid)
    {
        state->offset += chunk;
        state->block_number++;

        /* Deactivate if this was the last block */
        if (state->last_block)
        {
            state->active = 0U;
        }
    }

    return valid;
}

int csm_block_is_active(const csm_block_state *state)
{
    return (state != NULL) ? state->active : 0;
}

void csm_block_abort(csm_block_state *state)
{
    if (state != NULL)
    {
        state->active = 0U;
        state->direction = CSM_BLOCK_DIR_NONE;
        state->data = NULL;
    }
}

/* ── Client → Server (SET block transfer) ───────────────────────────────── */

int csm_block_start_client(csm_block_state *state, uint8_t invoke_id,
                           const uint8_t *data, uint32_t data_size,
                           uint32_t block_size)
{
    if ((state == NULL) || (data == NULL) || (data_size == 0U))
    {
        return 0;
    }

    state->direction = CSM_BLOCK_DIR_CLIENT_TO_SERVER;
    state->block_number = 0U;
    state->total_size = data_size;
    state->offset = 0U;
    state->block_size = (block_size > 0U) ? block_size : CSM_MAX_BLOCK_SIZE;
    state->invoke_id = invoke_id;
    state->last_block = 0U;
    state->active = 1U;
    state->data = data;

    return 1;
}

int csm_block_encode_set_request(csm_block_state *state, csm_array *array,
                                 const csm_request *request, uint32_t max_size)
{
    if ((state == NULL) || (array == NULL) || (request == NULL) || (!state->active))
    {
        return 0;
    }

    /* Calculate how much data to send in this block */
    uint32_t remaining = state->total_size - state->offset;
    uint32_t chunk = (remaining < max_size) ? remaining : max_size;
    uint32_t chunk_block = (remaining < state->block_size) ? remaining : state->block_size;
    if (chunk > chunk_block)
    {
        chunk = chunk_block;
    }

    /* Check if this is the last block */
    state->last_block = ((state->offset + chunk) >= state->total_size) ? 1U : 0U;

    /*
     * SET-Request-With-DataBlock (IEC 62056-5-3):
     * AXDR_SET_REQUEST (0xC1) | 02 (type) | invoke_id | last_block | block_number (4 bytes) |
     * class_id (2 bytes) | obis (6 bytes) | id (1 byte) | sel_access (1 byte) | data...
     */
    int valid = csm_array_write_u8(array, AXDR_SET_REQUEST);
    valid = valid && csm_array_write_u8(array, AXDR_SET_REQUEST_WITH_BLOCK);
    valid = valid && csm_array_write_u8(array, state->invoke_id);
    valid = valid && csm_array_write_u8(array, state->last_block);
    valid = valid && csm_array_write_u32(array, state->block_number);

    /* Object identification */
    valid = valid && csm_array_write_u16(array, request->db_request.logical_name.class_id);
    valid = valid && csm_array_write_buff(array, (const uint8_t *)&request->db_request.logical_name.obis.A, 6U);
    valid = valid && csm_array_write_u8(array, (uint8_t)request->db_request.logical_name.id);

    /* Selective access (none for block transfer) */
    valid = valid && csm_array_write_u8(array, 0U);

    /* Data chunk */
    if (valid && (chunk > 0U))
    {
        valid = csm_array_write_buff(array, &state->data[state->offset], chunk);
    }

    if (valid)
    {
        state->offset += chunk;
        state->block_number++;

        /* Deactivate if this was the last block */
        if (state->last_block)
        {
            state->active = 0U;
        }
    }

    return valid;
}

int csm_block_encode_set_next(csm_block_state *state, csm_array *array, uint32_t max_size)
{
    if ((state == NULL) || (array == NULL) || (!state->active))
    {
        return 0;
    }

    /* Calculate how much data to send in this block */
    uint32_t remaining = state->total_size - state->offset;
    uint32_t chunk = (remaining < max_size) ? remaining : max_size;
    uint32_t chunk_block = (remaining < state->block_size) ? remaining : state->block_size;
    if (chunk > chunk_block)
    {
        chunk = chunk_block;
    }

    /* Check if this is the last block */
    state->last_block = ((state->offset + chunk) >= state->total_size) ? 1U : 0U;

    /*
     * SET-Request-Next (IEC 62056-5-3):
     * AXDR_SET_REQUEST (0xC1) | 02 (type) | invoke_id | last_block | block_number (4 bytes) | data...
     * Note: No object identification in subsequent blocks
     */
    int valid = csm_array_write_u8(array, AXDR_SET_REQUEST);
    valid = valid && csm_array_write_u8(array, AXDR_SET_REQUEST_WITH_BLOCK);
    valid = valid && csm_array_write_u8(array, state->invoke_id);
    valid = valid && csm_array_write_u8(array, state->last_block);
    valid = valid && csm_array_write_u32(array, state->block_number);

    /* Data chunk */
    if (valid && (chunk > 0U))
    {
        valid = csm_array_write_buff(array, &state->data[state->offset], chunk);
    }

    if (valid)
    {
        state->offset += chunk;
        state->block_number++;

        /* Deactivate if this was the last block */
        if (state->last_block)
        {
            state->active = 0U;
        }
    }

    return valid;
}

/* ── Server-side SET receive ────────────────────────────────────────────── */

int csm_block_start_receive(csm_block_state *state, uint8_t invoke_id,
                            uint32_t block_size)
{
    if (state == NULL)
    {
        return 0;
    }

    state->direction = CSM_BLOCK_DIR_CLIENT_TO_SERVER;
    state->block_number = 0U;
    state->total_size = 0U;
    state->offset = 0U;
    state->block_size = (block_size > 0U) ? block_size : CSM_MAX_BLOCK_SIZE;
    state->invoke_id = invoke_id;
    state->last_block = 0U;
    state->active = 1U;
    state->data = state->receive_buf;

    return 1;
}

int csm_block_receive_data(csm_block_state *state, const uint8_t *data,
                           uint32_t data_size, uint8_t is_last)
{
    if ((state == NULL) || (data == NULL) || (!state->active))
    {
        return 0;
    }

    /* Check if we have enough space */
    if ((state->offset > CSM_BLOCK_MAX_RECEIVE_SIZE) ||
        (data_size > (CSM_BLOCK_MAX_RECEIVE_SIZE - state->offset)))
    {
        return 0; /* Buffer overflow */
    }

    /* Copy data into receive buffer */
    if (data_size > 0U)
    {
        memcpy(&state->receive_buf[state->offset], data, data_size);
    }

    state->offset += data_size;
    state->total_size = state->offset; /* Update total size */
    state->last_block = is_last;
    state->block_number++;

    /* Complete if this was the last block */
    if (is_last)
    {
        state->active = 0U;
    }

    return 1;
}

int csm_block_get_received(const csm_block_state *state, const uint8_t **data,
                           uint32_t *data_size)
{
    if ((state == NULL) || (data == NULL) || (data_size == NULL))
    {
        return 0;
    }

    /* Transfer must be complete (not active) and have data */
    if (state->active || (state->total_size == 0U))
    {
        return 0;
    }

    *data = state->receive_buf;
    *data_size = state->total_size;

    return 1;
}

int csm_block_encode_set_response(csm_block_state *state, csm_array *array)
{
    if ((state == NULL) || (array == NULL))
    {
        return 0;
    }

    /*
     * SET-Response-With-DataBlock (IEC 62056-5-3):
     * AXDR_SET_RESPONSE (0xC5) | 02 (type) | invoke_id | last_block | block_number (4 bytes) | access_result
     */
    int valid = csm_array_write_u8(array, AXDR_SET_RESPONSE);
    valid = valid && csm_array_write_u8(array, AXDR_SET_RESPONSE_WITH_BLOCK);
    valid = valid && csm_array_write_u8(array, state->invoke_id);
    valid = valid && csm_array_write_u8(array, state->last_block);
    valid = valid && csm_array_write_u32(array, state->block_number);

    /* Access result: success */
    valid = valid && csm_array_write_u8(array, 0x00U);

    return valid;
}

int csm_block_can_receive(const csm_block_state *state)
{
    if ((state == NULL) || (!state->active))
    {
        return 0;
    }

    return (state->offset < CSM_BLOCK_MAX_RECEIVE_SIZE) ? 1U : 0U;
}

/* ── Client-side GET block reception ─────────────────────────────────────── */

int csm_block_encode_get_next(csm_block_state *state, csm_array *array,
                              uint8_t invoke_id, uint32_t block_number)
{
    if ((state == NULL) || (array == NULL))
    {
        return 0;
    }

    /*
     * Get-Request-Next (IEC 62056-5-3):
     * AXDR_GET_REQUEST (0xC0) | 02 (type = next) | invoke_id | block_number (4 bytes)
     */
    int valid = csm_array_write_u8(array, AXDR_GET_REQUEST);
    valid = valid && csm_array_write_u8(array, 0x02U); /* type: next */
    valid = valid && csm_array_write_u8(array, invoke_id);
    valid = valid && csm_array_write_u32(array, block_number);

    return valid;
}

int csm_block_start_get_receive(csm_block_state *state, uint8_t invoke_id,
                                uint32_t block_size)
{
    if (state == NULL)
    {
        return 0;
    }

    state->direction = CSM_BLOCK_DIR_SERVER_TO_CLIENT;
    state->block_number = 0U;
    state->total_size = 0U;
    state->offset = 0U;
    state->block_size = (block_size > 0U) ? block_size : CSM_MAX_BLOCK_SIZE;
    state->invoke_id = invoke_id;
    state->last_block = 0U;
    state->active = 1U;
    state->data = state->receive_buf;

    return 1;
}

int csm_block_get_receive_data(csm_block_state *state, const uint8_t *data,
                               uint32_t data_size, uint8_t is_last)
{
    if ((state == NULL) || (data == NULL) || (!state->active))
    {
        return 0;
    }

    /* Check if we have enough space */
    if ((state->offset > CSM_BLOCK_MAX_RECEIVE_SIZE) ||
        (data_size > (CSM_BLOCK_MAX_RECEIVE_SIZE - state->offset)))
    {
        return 0; /* Buffer overflow */
    }

    /* Copy data into receive buffer */
    if (data_size > 0U)
    {
        memcpy(&state->receive_buf[state->offset], data, data_size);
    }

    state->offset += data_size;
    state->total_size = state->offset;
    state->last_block = is_last;
    state->block_number++;

    /* Complete if this was the last block */
    if (is_last)
    {
        state->active = 0U;
    }

    return 1;
}

int csm_block_get_received_data(const csm_block_state *state, const uint8_t **data,
                                uint32_t *data_size)
{
    if ((state == NULL) || (data == NULL) || (data_size == NULL))
    {
        return 0;
    }

    /* Transfer must be complete (not active) and have data */
    if (state->active || (state->total_size == 0U))
    {
        return 0;
    }

    *data = state->receive_buf;
    *data_size = state->total_size;

    return 1;
}
