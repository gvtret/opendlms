/**
 * \file test_block_transfer.cpp
 * \brief Tests for Block Transfer (GBT) support
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "catch.hpp"
#include "csm_block_transfer.h"
#include "csm_array.h"
#include <cstring>

extern "C" void csm_sys_init();

/* ── Block Transfer State Tests ─────────────────────────────────────────── */

TEST_CASE("Block transfer init", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    REQUIRE(state.direction == CSM_BLOCK_DIR_NONE);
    REQUIRE(state.block_number == 0U);
    REQUIRE(state.total_size == 0U);
    REQUIRE(state.offset == 0U);
    REQUIRE(state.block_size == CSM_MAX_BLOCK_SIZE);
    REQUIRE(state.invoke_id == 0U);
    REQUIRE(state.last_block == 0U);
    REQUIRE(state.active == 0U);
    REQUIRE(state.data == NULL);
}

TEST_CASE("Block transfer is_active", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    REQUIRE(csm_block_is_active(&state) == 0);

    state.active = 1U;
    REQUIRE(csm_block_is_active(&state) == 1);

    state.active = 0U;
    REQUIRE(csm_block_is_active(&state) == 0);
}

TEST_CASE("Block transfer abort", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    state.active = 1U;
    state.direction = CSM_BLOCK_DIR_SERVER_TO_CLIENT;
    state.data = (const uint8_t *)"test";

    csm_block_abort(&state);

    REQUIRE(state.active == 0U);
    REQUIRE(state.direction == CSM_BLOCK_DIR_NONE);
    REQUIRE(state.data == NULL);
}

/* ── Block Transfer Start Tests ─────────────────────────────────────────── */

TEST_CASE("Block transfer start server - valid", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    static const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    int result = csm_block_start_server(&state, 0x01U, test_data, sizeof(test_data), 0U);

    REQUIRE(result == 1);
    REQUIRE(state.direction == CSM_BLOCK_DIR_SERVER_TO_CLIENT);
    REQUIRE(state.block_number == 0U);
    REQUIRE(state.total_size == sizeof(test_data));
    REQUIRE(state.offset == 0U);
    REQUIRE(state.block_size == CSM_MAX_BLOCK_SIZE);
    REQUIRE(state.invoke_id == 0x01U);
    REQUIRE(state.last_block == 0U);
    REQUIRE(state.active == 1U);
    REQUIRE(state.data == test_data);
}

TEST_CASE("Block transfer start server - invalid params", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    static const uint8_t test_data[] = {0x01, 0x02};

    /* NULL state */
    REQUIRE(csm_block_start_server(NULL, 0x01U, test_data, sizeof(test_data), 0U) == 0);

    /* NULL data */
    REQUIRE(csm_block_start_server(&state, 0x01U, NULL, sizeof(test_data), 0U) == 0);

    /* Zero size */
    REQUIRE(csm_block_start_server(&state, 0x01U, test_data, 0U, 0U) == 0);
}

/* ── Block Encoding Tests ───────────────────────────────────────────────── */

TEST_CASE("Block encode first - small data fits in one block", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    static const uint8_t test_data[] = {0x01, 0x02, 0x03};

    csm_block_start_server(&state, 0x01U, test_data, sizeof(test_data), 0U);

    uint8_t buf[64];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    int result = csm_block_encode_first(&state, &array, sizeof(buf));

    REQUIRE(result == 1);

    /* Check encoded data:
     * Byte 0: AXDR_GET_RESPONSE (0xC4)
     * Byte 1: 0x04 (with block)
     * Byte 2: invoke_id (0x01)
     * Byte 3: last_block (0x01 - single block)
     * Byte 4-7: block_number (0x00000000)
     * Byte 8+: data
     */
    uint32_t written = csm_array_written(&array);
    REQUIRE(written == 8U + sizeof(test_data));

    REQUIRE(array.buff[0] == 0xC4U); /* AXDR_GET_RESPONSE */
    REQUIRE(array.buff[1] == 0x04U); /* with block */
    REQUIRE(array.buff[2] == 0x01U); /* invoke_id */
    REQUIRE(array.buff[3] == 0x01U); /* last_block = true */
    REQUIRE(array.buff[4] == 0x00U); /* block_number = 0 */
    REQUIRE(array.buff[5] == 0x00U);
    REQUIRE(array.buff[6] == 0x00U);
    REQUIRE(array.buff[7] == 0x00U);

    /* Check data */
    REQUIRE(array.buff[8] == 0x01U);
    REQUIRE(array.buff[9] == 0x02U);
    REQUIRE(array.buff[10] == 0x03U);

    /* After encoding, transfer should be complete */
    REQUIRE(state.active == 0U);
}

TEST_CASE("Block encode first - large data requires multiple blocks", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    /* 10 bytes of data, block size = 4 bytes */
    static const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

    csm_block_start_server(&state, 0x01U, test_data, sizeof(test_data), 4U);

    uint8_t buf[64];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    int result = csm_block_encode_first(&state, &array, sizeof(buf));

    REQUIRE(result == 1);

    /* First block: 8 header + 4 data = 12 bytes */
    uint32_t written = csm_array_written(&array);
    REQUIRE(written == 8U + 4U);

    REQUIRE(array.buff[0] == 0xC4U); /* AXDR_GET_RESPONSE */
    REQUIRE(array.buff[1] == 0x04U); /* with block */
    REQUIRE(array.buff[2] == 0x01U); /* invoke_id */
    REQUIRE(array.buff[3] == 0x00U); /* last_block = false */
    REQUIRE(array.buff[4] == 0x00U); /* block_number = 0 */
    REQUIRE(array.buff[5] == 0x00U);
    REQUIRE(array.buff[6] == 0x00U);
    REQUIRE(array.buff[7] == 0x00U);

    /* Check data (first 4 bytes) */
    REQUIRE(array.buff[8] == 0x01U);
    REQUIRE(array.buff[9] == 0x02U);
    REQUIRE(array.buff[10] == 0x03U);
    REQUIRE(array.buff[11] == 0x04U);

    /* Transfer still active */
    REQUIRE(state.active == 1U);
    REQUIRE(state.block_number == 1U);
    REQUIRE(state.offset == 4U);
}

TEST_CASE("Block encode next - continues transfer", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    /* 10 bytes of data, block size = 4 bytes */
    static const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

    csm_block_start_server(&state, 0x01U, test_data, sizeof(test_data), 4U);

    uint8_t buf[64];
    csm_array array;

    /* Encode first block */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    csm_block_encode_first(&state, &array, sizeof(buf));

    REQUIRE(state.active == 1U);
    REQUIRE(state.offset == 4U);

    /* Encode second block */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    int result = csm_block_encode_next(&state, &array, sizeof(buf));

    REQUIRE(result == 1);

    uint32_t written = csm_array_written(&array);
    REQUIRE(written == 8U + 4U);

    REQUIRE(array.buff[0] == 0xC4U); /* AXDR_GET_RESPONSE */
    REQUIRE(array.buff[1] == 0x04U); /* with block */
    REQUIRE(array.buff[2] == 0x01U); /* invoke_id */
    REQUIRE(array.buff[3] == 0x00U); /* last_block = false (more data) */
    REQUIRE(array.buff[4] == 0x00U); /* block_number = 1 */
    REQUIRE(array.buff[5] == 0x00U);
    REQUIRE(array.buff[6] == 0x00U);
    REQUIRE(array.buff[7] == 0x01U);

    /* Check data (bytes 4-7) */
    REQUIRE(array.buff[8] == 0x05U);
    REQUIRE(array.buff[9] == 0x06U);
    REQUIRE(array.buff[10] == 0x07U);
    REQUIRE(array.buff[11] == 0x08U);

    REQUIRE(state.active == 1U);
    REQUIRE(state.block_number == 2U);
    REQUIRE(state.offset == 8U);
}

TEST_CASE("Block encode final block", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    /* 10 bytes of data, block size = 4 bytes */
    static const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

    csm_block_start_server(&state, 0x01U, test_data, sizeof(test_data), 4U);

    uint8_t buf[64];
    csm_array array;

    /* Encode first block */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    csm_block_encode_first(&state, &array, sizeof(buf));

    /* Encode second block */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    csm_block_encode_next(&state, &array, sizeof(buf));

    /* Encode third (final) block */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    int result = csm_block_encode_next(&state, &array, sizeof(buf));

    REQUIRE(result == 1);

    uint32_t written = csm_array_written(&array);
    REQUIRE(written == 8U + 2U); /* 2 remaining bytes */

    REQUIRE(array.buff[0] == 0xC4U); /* AXDR_GET_RESPONSE */
    REQUIRE(array.buff[1] == 0x04U); /* with block */
    REQUIRE(array.buff[2] == 0x01U); /* invoke_id */
    REQUIRE(array.buff[3] == 0x01U); /* last_block = true */
    REQUIRE(array.buff[4] == 0x00U); /* block_number = 2 */
    REQUIRE(array.buff[5] == 0x00U);
    REQUIRE(array.buff[6] == 0x00U);
    REQUIRE(array.buff[7] == 0x02U);

    /* Check data (last 2 bytes) */
    REQUIRE(array.buff[8] == 0x09U);
    REQUIRE(array.buff[9] == 0x0AU);

    /* Transfer should be complete */
    REQUIRE(state.active == 0U);
    REQUIRE(state.offset == 10U);
}

TEST_CASE("Block encode next - inactive state", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    uint8_t buf[64];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    /* Try to encode without starting transfer */
    int result = csm_block_encode_next(&state, &array, sizeof(buf));

    REQUIRE(result == 0);
}

TEST_CASE("Block encode first - inactive state", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    uint8_t buf[64];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    /* Try to encode without starting transfer */
    int result = csm_block_encode_first(&state, &array, sizeof(buf));

    REQUIRE(result == 0);
}

/* ── SET Block Transfer Tests ───────────────────────────────────────────── */

TEST_CASE("Block start client - valid", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    static const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    int result = csm_block_start_client(&state, 0x01U, test_data, sizeof(test_data), 0U);

    REQUIRE(result == 1);
    REQUIRE(state.direction == CSM_BLOCK_DIR_CLIENT_TO_SERVER);
    REQUIRE(state.block_number == 0U);
    REQUIRE(state.total_size == sizeof(test_data));
    REQUIRE(state.offset == 0U);
    REQUIRE(state.invoke_id == 0x01U);
    REQUIRE(state.active == 1U);
    REQUIRE(state.data == test_data);
}

TEST_CASE("Block encode SET request - first block", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    static const uint8_t test_data[] = {0x01, 0x02, 0x03};

    csm_block_start_client(&state, 0x01U, test_data, sizeof(test_data), 0U);

    /* Create a request with object info */
    csm_request request;
    memset(&request, 0, sizeof(request));
    request.db_request.logical_name.class_id = 0x0003U; /* Data class */
    request.db_request.logical_name.obis.A = 0U;
    request.db_request.logical_name.obis.B = 0U;
    request.db_request.logical_name.obis.C = 1U;
    request.db_request.logical_name.obis.D = 0U;
    request.db_request.logical_name.obis.E = 0U;
    request.db_request.logical_name.obis.F = 255U;
    request.db_request.logical_name.id = 2U;

    uint8_t buf[64];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    int result = csm_block_encode_set_request(&state, &array, &request, sizeof(buf));

    REQUIRE(result == 1);

    /*
     * SET-Request-With-DataBlock format:
     * Byte 0: AXDR_SET_REQUEST (0xC1)
     * Byte 1: 0x02 (type = with block)
     * Byte 2: invoke_id (0x01)
     * Byte 3: last_block (0x01 - single block)
     * Byte 4-7: block_number (0x00000000)
     * Byte 8-9: class_id (0x0003)
     * Byte 10-15: obis (00 00 01 00 00 FF)
     * Byte 16: id (0x02)
     * Byte 17: sel_access (0x00)
     * Byte 18+: data
     */
    uint32_t written = csm_array_written(&array);
    REQUIRE(written == 18U + sizeof(test_data));

    REQUIRE(array.buff[0] == 0xC1U); /* AXDR_SET_REQUEST */
    REQUIRE(array.buff[1] == 0x02U); /* with block */
    REQUIRE(array.buff[2] == 0x01U); /* invoke_id */
    REQUIRE(array.buff[3] == 0x01U); /* last_block = true (single block) */
    REQUIRE(array.buff[4] == 0x00U); /* block_number = 0 */
    REQUIRE(array.buff[5] == 0x00U);
    REQUIRE(array.buff[6] == 0x00U);
    REQUIRE(array.buff[7] == 0x00U);

    /* Object info */
    REQUIRE(array.buff[8] == 0x00U); /* class_id high */
    REQUIRE(array.buff[9] == 0x03U); /* class_id low */
    REQUIRE(array.buff[10] == 0x00U); /* obis A */
    REQUIRE(array.buff[11] == 0x00U); /* obis B */
    REQUIRE(array.buff[12] == 0x01U); /* obis C */
    REQUIRE(array.buff[13] == 0x00U); /* obis D */
    REQUIRE(array.buff[14] == 0x00U); /* obis E */
    REQUIRE(array.buff[15] == 0xFFU); /* obis F */
    REQUIRE(array.buff[16] == 0x02U); /* id */
    REQUIRE(array.buff[17] == 0x00U); /* sel_access = none */

    /* Data */
    REQUIRE(array.buff[18] == 0x01U);
    REQUIRE(array.buff[19] == 0x02U);
    REQUIRE(array.buff[20] == 0x03U);

    REQUIRE(state.active == 0U);
}

TEST_CASE("Block encode SET request - multi-block", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    /* 10 bytes of data, block size = 4 bytes */
    static const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

    csm_block_start_client(&state, 0x01U, test_data, sizeof(test_data), 4U);

    csm_request request;
    memset(&request, 0, sizeof(request));
    request.db_request.logical_name.class_id = 0x0003U;
    request.db_request.logical_name.id = 2U;

    uint8_t buf[64];
    csm_array array;

    /* First block */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    int result = csm_block_encode_set_request(&state, &array, &request, sizeof(buf));

    REQUIRE(result == 1);
    REQUIRE(state.active == 1U);
    REQUIRE(state.offset == 4U);
    REQUIRE(state.block_number == 1U);

    /* Verify first block header */
    REQUIRE(array.buff[0] == 0xC1U); /* AXDR_SET_REQUEST */
    REQUIRE(array.buff[1] == 0x02U); /* with block */
    REQUIRE(array.buff[3] == 0x00U); /* last_block = false */

    /* Second block (next) */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    result = csm_block_encode_set_next(&state, &array, sizeof(buf));

    REQUIRE(result == 1);
    REQUIRE(state.active == 1U);
    REQUIRE(state.offset == 8U);
    REQUIRE(state.block_number == 2U);

    /* Third block (final) */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    result = csm_block_encode_set_next(&state, &array, sizeof(buf));

    REQUIRE(result == 1);
    REQUIRE(state.active == 0U);
    REQUIRE(state.offset == 10U);

    /* Verify last block */
    REQUIRE(array.buff[3] == 0x01U); /* last_block = true */
}

/* ── Server-side SET receive tests ──────────────────────────────────────── */

TEST_CASE("Block receive - start and receive data", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    int result = csm_block_start_receive(&state, 0x01U, 0U);

    REQUIRE(result == 1);
    REQUIRE(state.direction == CSM_BLOCK_DIR_CLIENT_TO_SERVER);
    REQUIRE(state.active == 1U);

    /* Receive first block */
    static const uint8_t block1[] = {0x01, 0x02, 0x03, 0x04};
    result = csm_block_receive_data(&state, block1, sizeof(block1), 0);

    REQUIRE(result == 1);
    REQUIRE(state.offset == 4U);
    REQUIRE(state.active == 1U);

    /* Receive second block (last) */
    static const uint8_t block2[] = {0x05, 0x06};
    result = csm_block_receive_data(&state, block2, sizeof(block2), 1);

    REQUIRE(result == 1);
    REQUIRE(state.offset == 6U);
    REQUIRE(state.active == 0U);
    REQUIRE(state.last_block == 1U);

    /* Get received data */
    const uint8_t *received_data;
    uint32_t received_size;
    result = csm_block_get_received(&state, &received_data, &received_size);

    REQUIRE(result == 1);
    REQUIRE(received_size == 6U);
    REQUIRE(received_data[0] == 0x01U);
    REQUIRE(received_data[1] == 0x02U);
    REQUIRE(received_data[2] == 0x03U);
    REQUIRE(received_data[3] == 0x04U);
    REQUIRE(received_data[4] == 0x05U);
    REQUIRE(received_data[5] == 0x06U);
}

TEST_CASE("Block encode SET response", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    csm_block_start_receive(&state, 0x01U, 0U);

    /* Receive a block */
    static const uint8_t block[] = {0x01, 0x02};
    csm_block_receive_data(&state, block, sizeof(block), 0);

    uint8_t buf[32];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    int result = csm_block_encode_set_response(&state, &array);

    REQUIRE(result == 1);

    /*
     * SET-Response-With-DataBlock format:
     * Byte 0: AXDR_SET_RESPONSE (0xC5)
     * Byte 1: 0x02 (type = with block)
     * Byte 2: invoke_id (0x01)
     * Byte 3: last_block (0x00 - not last)
     * Byte 4-7: block_number (0x00000000)
     * Byte 8: access_result (0x00 = success)
     */
    uint32_t written = csm_array_written(&array);
    REQUIRE(written == 9U);

    REQUIRE(array.buff[0] == 0xC5U); /* AXDR_SET_RESPONSE */
    REQUIRE(array.buff[1] == 0x02U); /* with block */
    REQUIRE(array.buff[2] == 0x01U); /* invoke_id */
    REQUIRE(array.buff[3] == 0x00U); /* last_block = false */
    REQUIRE(array.buff[4] == 0x00U); /* block_number = 1 (incremented after receive) */
    REQUIRE(array.buff[5] == 0x00U);
    REQUIRE(array.buff[6] == 0x00U);
    REQUIRE(array.buff[7] == 0x01U);
    REQUIRE(array.buff[8] == 0x00U); /* access_result = success */
}

TEST_CASE("Block can receive", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    REQUIRE(csm_block_can_receive(&state) == 0);

    csm_block_start_receive(&state, 0x01U, 0U);
    REQUIRE(csm_block_can_receive(&state) == 1);

    csm_block_abort(&state);
    REQUIRE(csm_block_can_receive(&state) == 0);
}

/* ── Client-side SET block encoding tests ───────────────────────────────── */

TEST_CASE("Block encode SET next - multi-block", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    /* 10 bytes, block size = 4 */
    static const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

    csm_block_start_client(&state, 0x01U, test_data, sizeof(test_data), 4U);

    uint8_t buf[64];
    csm_array array;

    /* First block (with object info) */
    csm_request request;
    memset(&request, 0, sizeof(request));
    request.db_request.logical_name.class_id = 0x0003U;
    request.db_request.logical_name.id = 2U;

    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    csm_block_encode_set_request(&state, &array, &request, sizeof(buf));

    REQUIRE(state.active == 1U);
    REQUIRE(state.offset == 4U);

    /* Second block (no object info) */
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    int result = csm_block_encode_set_next(&state, &array, sizeof(buf));

    REQUIRE(result == 1);
    REQUIRE(array.buff[0] == 0xC1U); /* AXDR_SET_REQUEST */
    REQUIRE(array.buff[1] == 0x02U); /* with block */
    REQUIRE(array.buff[2] == 0x01U); /* invoke_id */
    REQUIRE(array.buff[3] == 0x00U); /* last_block = false */
    REQUIRE(array.buff[4] == 0x00U); /* block_number = 1 */
    REQUIRE(array.buff[5] == 0x00U);
    REQUIRE(array.buff[6] == 0x00U);
    REQUIRE(array.buff[7] == 0x01U);

    REQUIRE(state.active == 1U);
    REQUIRE(state.offset == 8U);
    REQUIRE(state.block_number == 2U);
}

TEST_CASE("Block encode SET next - inactive", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    uint8_t buf[64];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    int result = csm_block_encode_set_next(&state, &array, sizeof(buf));
    REQUIRE(result == 0);
}

TEST_CASE("Block start client - invalid params", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    static const uint8_t test_data[] = {0x01};

    REQUIRE(csm_block_start_client(NULL, 0x01U, test_data, 1U, 0U) == 0);
    REQUIRE(csm_block_start_client(&state, 0x01U, NULL, 1U, 0U) == 0);
    REQUIRE(csm_block_start_client(&state, 0x01U, test_data, 0U, 0U) == 0);
}
