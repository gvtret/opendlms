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
#include <cstdint>

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
    REQUIRE(state.data == nullptr);
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
    REQUIRE(state.data == nullptr);
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

TEST_CASE("Block receive buffers are per state", "[block_transfer]")
{
    csm_block_state state_a;
    csm_block_state state_b;
    csm_block_init(&state_a);
    csm_block_init(&state_b);

    REQUIRE(csm_block_start_receive(&state_a, 0x01U, 0U) == 1);
    REQUIRE(csm_block_start_receive(&state_b, 0x02U, 0U) == 1);

    static const uint8_t block_a[] = {0xA1, 0xA2};
    static const uint8_t block_b[] = {0xB1, 0xB2, 0xB3};
    REQUIRE(csm_block_receive_data(&state_a, block_a, sizeof(block_a), 1) == 1);
    REQUIRE(csm_block_receive_data(&state_b, block_b, sizeof(block_b), 1) == 1);

    const uint8_t *data_a;
    const uint8_t *data_b;
    uint32_t size_a;
    uint32_t size_b;
    REQUIRE(csm_block_get_received(&state_a, &data_a, &size_a) == 1);
    REQUIRE(csm_block_get_received(&state_b, &data_b, &size_b) == 1);

    REQUIRE(data_a != data_b);
    REQUIRE(size_a == sizeof(block_a));
    REQUIRE(size_b == sizeof(block_b));
    REQUIRE(data_a[0] == 0xA1U);
    REQUIRE(data_a[1] == 0xA2U);
    REQUIRE(data_b[0] == 0xB1U);
    REQUIRE(data_b[1] == 0xB2U);
    REQUIRE(data_b[2] == 0xB3U);
}

TEST_CASE("Block receive rejects overflowed offsets", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    REQUIRE(csm_block_start_receive(&state, 0x01U, 0U) == 1);
    state.offset = UINT32_MAX;

    static const uint8_t data[] = {0x01};
    REQUIRE(csm_block_receive_data(&state, data, sizeof(data), 0) == 0);
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

/* ── Client-side GET block reception tests ───────────────────────────────── */

TEST_CASE("Block encode GET next", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    uint8_t buf[32];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    int result = csm_block_encode_get_next(&state, &array, 0x01U, 0U);

    REQUIRE(result == 1);

    /*
     * Get-Request-Next format:
     * Byte 0: AXDR_GET_REQUEST (0xC0)
     * Byte 1: 0x02 (type = next)
     * Byte 2: invoke_id (0x01)
     * Byte 3-6: block_number (0x00000000)
     */
    uint32_t written = csm_array_written(&array);
    REQUIRE(written == 7U);

    REQUIRE(array.buff[0] == 0xC0U); /* AXDR_GET_REQUEST */
    REQUIRE(array.buff[1] == 0x02U); /* type: next */
    REQUIRE(array.buff[2] == 0x01U); /* invoke_id */
    REQUIRE(array.buff[3] == 0x00U); /* block_number = 0 */
    REQUIRE(array.buff[4] == 0x00U);
    REQUIRE(array.buff[5] == 0x00U);
    REQUIRE(array.buff[6] == 0x00U);
}

TEST_CASE("Block start GET receive - valid", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    int result = csm_block_start_get_receive(&state, 0x01U, 0U);

    REQUIRE(result == 1);
    REQUIRE(state.direction == CSM_BLOCK_DIR_SERVER_TO_CLIENT);
    REQUIRE(state.active == 1U);
    REQUIRE(state.invoke_id == 0x01U);
}

TEST_CASE("Block GET receive data - multi-block", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    csm_block_start_get_receive(&state, 0x01U, 0U);

    /* Receive first block */
    static const uint8_t block1[] = {0x01, 0x02, 0x03, 0x04};
    int result = csm_block_get_receive_data(&state, block1, sizeof(block1), 0);

    REQUIRE(result == 1);
    REQUIRE(state.offset == 4U);
    REQUIRE(state.active == 1U);

    /* Receive second block (last) */
    static const uint8_t block2[] = {0x05, 0x06};
    result = csm_block_get_receive_data(&state, block2, sizeof(block2), 1);

    REQUIRE(result == 1);
    REQUIRE(state.offset == 6U);
    REQUIRE(state.active == 0U);
    REQUIRE(state.last_block == 1U);

    /* Get accumulated data */
    const uint8_t *received_data;
    uint32_t received_size;
    result = csm_block_get_received_data(&state, &received_data, &received_size);

    REQUIRE(result == 1);
    REQUIRE(received_size == 6U);
    REQUIRE(received_data[0] == 0x01U);
    REQUIRE(received_data[1] == 0x02U);
    REQUIRE(received_data[2] == 0x03U);
    REQUIRE(received_data[3] == 0x04U);
    REQUIRE(received_data[4] == 0x05U);
    REQUIRE(received_data[5] == 0x06U);
}

TEST_CASE("Block GET receive buffers are per state", "[block_transfer]")
{
    csm_block_state state_a;
    csm_block_state state_b;
    csm_block_init(&state_a);
    csm_block_init(&state_b);

    REQUIRE(csm_block_start_get_receive(&state_a, 0x01U, 0U) == 1);
    REQUIRE(csm_block_start_get_receive(&state_b, 0x02U, 0U) == 1);

    static const uint8_t block_a[] = {0x11, 0x12};
    static const uint8_t block_b[] = {0x21, 0x22, 0x23};
    REQUIRE(csm_block_get_receive_data(&state_a, block_a, sizeof(block_a), 1) == 1);
    REQUIRE(csm_block_get_receive_data(&state_b, block_b, sizeof(block_b), 1) == 1);

    const uint8_t *data_a;
    const uint8_t *data_b;
    uint32_t size_a;
    uint32_t size_b;
    REQUIRE(csm_block_get_received_data(&state_a, &data_a, &size_a) == 1);
    REQUIRE(csm_block_get_received_data(&state_b, &data_b, &size_b) == 1);

    REQUIRE(data_a != data_b);
    REQUIRE(size_a == sizeof(block_a));
    REQUIRE(size_b == sizeof(block_b));
    REQUIRE(data_a[0] == 0x11U);
    REQUIRE(data_a[1] == 0x12U);
    REQUIRE(data_b[0] == 0x21U);
    REQUIRE(data_b[1] == 0x22U);
    REQUIRE(data_b[2] == 0x23U);
}

TEST_CASE("Block GET receive data - invalid params", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    static const uint8_t data[] = {0x01};

    REQUIRE(csm_block_get_receive_data(NULL, data, 1U, 0) == 0);
    REQUIRE(csm_block_get_receive_data(&state, NULL, 1U, 0) == 0);
    REQUIRE(csm_block_get_receive_data(&state, data, 1U, 0) == 0); /* not active */
}

TEST_CASE("Block GET receive rejects overflowed offsets", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    REQUIRE(csm_block_start_get_receive(&state, 0x01U, 0U) == 1);
    state.offset = UINT32_MAX;

    static const uint8_t data[] = {0x01};
    REQUIRE(csm_block_get_receive_data(&state, data, sizeof(data), 0) == 0);
}

TEST_CASE("Block GET received data - not complete", "[block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    csm_block_start_get_receive(&state, 0x01U, 0U);

    const uint8_t *data;
    uint32_t size;

    /* Should fail - still active */
    REQUIRE(csm_block_get_received_data(&state, &data, &size) == 0);
}

/* ── Full Round-Trip Test: SET block → GET block ─────────────────────────── */

TEST_CASE("Round-trip: SET block transfer then GET block reception", "[block_transfer][roundtrip]")
{
    /*
     * Simulates full round-trip:
     * 1. Client sends 20 bytes in 8-byte blocks via SET
     * 2. Server accumulates and stores the data
     * 3. Server sends data back in 8-byte blocks via GET
     * 4. Client accumulates and verifies complete data
     */

    /* ── Phase 1: Client sends SET blocks ──────────────────────────────── */

    static const uint8_t original_data[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14
    };
    const uint32_t data_len = sizeof(original_data);
    const uint32_t block_size = 8U;

    /* Client-side: start block transfer */
    csm_block_state client_tx;
    csm_block_init(&client_tx);
    REQUIRE(csm_block_start_client(&client_tx, 0x01U, original_data, data_len, block_size) == 1);

    /* Server-side: prepare to receive */
    csm_block_state server_rx;
    csm_block_init(&server_rx);
    REQUIRE(csm_block_start_receive(&server_rx, 0x01U, block_size) == 1);

    uint8_t buf[128];
    csm_array array;
    csm_request request;
    memset(&request, 0, sizeof(request));
    request.db_request.logical_name.class_id = 0x0003U;
    request.db_request.logical_name.id = 2U;

    uint32_t total_sent = 0U;
    uint8_t block_num = 0U;

    /* Send blocks until complete */
    while (csm_block_is_active(&client_tx))
    {
        csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

        int encoded;
        if (block_num == 0U)
        {
            encoded = csm_block_encode_set_request(&client_tx, &array, &request, block_size);
        }
        else
        {
            encoded = csm_block_encode_set_next(&client_tx, &array, block_size);
        }
        REQUIRE(encoded == 1);

        /* Verify block header */
        REQUIRE(array.buff[0] == 0xC1U); /* AXDR_SET_REQUEST */
        REQUIRE(array.buff[1] == 0x02U); /* with block */
        REQUIRE(array.buff[2] == 0x01U); /* invoke_id */

        /* Extract data portion (skip header) */
        uint32_t header_size = (block_num == 0U) ? 18U : 8U;
        uint32_t chunk_size = csm_array_written(&array) - header_size;

        REQUIRE(chunk_size <= block_size);

        /* Server receives this block */
        int is_last = (block_num == 2U) ? 1 : 0; /* 20 bytes / 8 = 3 blocks */
        REQUIRE(csm_block_receive_data(&server_rx, &array.buff[header_size], chunk_size, is_last) == 1);

        total_sent += chunk_size;
        block_num++;
    }

    REQUIRE(total_sent == data_len);
    REQUIRE(server_rx.active == 0U);
    REQUIRE(server_rx.last_block == 1U);

    /* Verify server received correct data */
    const uint8_t *server_data;
    uint32_t server_size;
    REQUIRE(csm_block_get_received(&server_rx, &server_data, &server_size) == 1);
    REQUIRE(server_size == data_len);
    REQUIRE(memcmp(server_data, original_data, data_len) == 0);

    /* ── Phase 2: Server sends GET blocks back ─────────────────────────── */

    /* Server-side: start block transfer with same data */
    csm_block_state server_tx;
    csm_block_init(&server_tx);
    REQUIRE(csm_block_start_server(&server_tx, 0x02U, server_data, server_size, block_size) == 1);

    /* Client-side: prepare to receive */
    csm_block_state client_rx;
    csm_block_init(&client_rx);
    REQUIRE(csm_block_start_get_receive(&client_rx, 0x02U, block_size) == 1);

    block_num = 0U;
    uint32_t total_received = 0U;

    while (csm_block_is_active(&server_tx))
    {
        /* Server encodes block */
        csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

        int encoded;
        if (block_num == 0U)
        {
            encoded = csm_block_encode_first(&server_tx, &array, block_size);
        }
        else
        {
            encoded = csm_block_encode_next(&server_tx, &array, block_size);
        }
        REQUIRE(encoded == 1);

        /* Verify block header */
        REQUIRE(array.buff[0] == 0xC4U); /* AXDR_GET_RESPONSE */
        REQUIRE(array.buff[1] == 0x04U); /* with block */

        /* Extract data portion (skip 8-byte header) */
        uint32_t chunk_size = csm_array_written(&array) - 8U;
        REQUIRE(chunk_size <= block_size);

        /* Client receives this block */
        int is_last = (array.buff[3] == 0x01U) ? 1 : 0;
        REQUIRE(csm_block_get_receive_data(&client_rx, &array.buff[8U], chunk_size, is_last) == 1);

        total_received += chunk_size;
        block_num++;
    }

    REQUIRE(total_received == data_len);

    /* Verify client received correct data */
    const uint8_t *client_data;
    uint32_t client_size;
    REQUIRE(csm_block_get_received_data(&client_rx, &client_data, &client_size) == 1);
    REQUIRE(client_size == data_len);
    REQUIRE(memcmp(client_data, original_data, data_len) == 0);
}

TEST_CASE("Round-trip: single block SET then single block GET", "[block_transfer][roundtrip]")
{
    /* Data fits in one block */
    static const uint8_t small_data[] = {0xAA, 0xBB, 0xCC};

    /* ── Phase 1: Client sends single SET block ────────────────────────── */

    csm_block_state client_tx;
    csm_block_init(&client_tx);
    REQUIRE(csm_block_start_client(&client_tx, 0x01U, small_data, sizeof(small_data), 0U) == 1);

    csm_block_state server_rx;
    csm_block_init(&server_rx);
    REQUIRE(csm_block_start_receive(&server_rx, 0x01U, 0U) == 1);

    uint8_t buf[64];
    csm_array array;
    csm_request request;
    memset(&request, 0, sizeof(request));
    request.db_request.logical_name.class_id = 0x0003U;
    request.db_request.logical_name.id = 2U;

    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    REQUIRE(csm_block_encode_set_request(&client_tx, &array, &request, sizeof(buf)) == 1);

    /* Single block - last_block should be set */
    REQUIRE(array.buff[3] == 0x01U); /* last_block */

    uint32_t chunk_size = csm_array_written(&array) - 18U;
    REQUIRE(csm_block_receive_data(&server_rx, &array.buff[18U], chunk_size, 1) == 1);
    REQUIRE(server_rx.active == 0U);

    const uint8_t *server_data;
    uint32_t server_size;
    REQUIRE(csm_block_get_received(&server_rx, &server_data, &server_size) == 1);
    REQUIRE(server_size == sizeof(small_data));
    REQUIRE(memcmp(server_data, small_data, sizeof(small_data)) == 0);

    /* ── Phase 2: Server sends single GET block back ───────────────────── */

    csm_block_state server_tx;
    csm_block_init(&server_tx);
    REQUIRE(csm_block_start_server(&server_tx, 0x02U, server_data, server_size, 0U) == 1);

    csm_block_state client_rx;
    csm_block_init(&client_rx);
    REQUIRE(csm_block_start_get_receive(&client_rx, 0x02U, 0U) == 1);

    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    REQUIRE(csm_block_encode_first(&server_tx, &array, sizeof(buf)) == 1);

    REQUIRE(array.buff[3] == 0x01U); /* last_block */

    chunk_size = csm_array_written(&array) - 8U;
    REQUIRE(csm_block_get_receive_data(&client_rx, &array.buff[8U], chunk_size, 1) == 1);
    REQUIRE(client_rx.active == 0U);

    const uint8_t *client_data;
    uint32_t client_size;
    REQUIRE(csm_block_get_received_data(&client_rx, &client_data, &client_size) == 1);
    REQUIRE(client_size == sizeof(small_data));
    REQUIRE(memcmp(client_data, small_data, sizeof(small_data)) == 0);
}
