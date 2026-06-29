/**
 * \file test_cosemlib.cpp
 * \brief Tests for cosemlib umbrella header and basic API
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "catch.hpp"
#include "cosemlib.h"
#include "csm_ber.h"
#include "os_util.h"
#include <cstring>

extern "C" void csm_sys_init();

/* ── Umbrella header tests ───────────────────────────────────────────────── */

TEST_CASE("cosemlib.h compiles and provides version info", "[cosemlib]")
{
    REQUIRE(COSEMLIB_VERSION_MAJOR == 1);
    REQUIRE(COSEMLIB_VERSION_MINOR == 1);
    REQUIRE(COSEMLIB_VERSION_PATCH == 0);
    REQUIRE(strcmp(COSEMLIB_VERSION_STRING, "1.1.0") == 0);
}

TEST_CASE("cosemlib.h provides all core types", "[cosemlib]")
{
    /* Verify all core types are accessible via umbrella header */
    csm_block_state bs;
    csm_block_init(&bs);
    REQUIRE(bs.direction == CSM_BLOCK_DIR_NONE);

    csm_array arr;
    uint8_t buf[32];
    csm_array_init(&arr, buf, sizeof(buf), 0, 0);
    REQUIRE(csm_array_written(&arr) == 0);

    csm_response resp;
    csm_client_init(NULL, &resp);
    REQUIRE(resp.type == 0U);
}

TEST_CASE("client service helpers reject null inputs", "[cosemlib][services]")
{
    uint8_t buf[] = { AXDR_GET_RESPONSE, 0x01U, 0x01U, 0x00U };
    csm_array arr;
    csm_array_init(&arr, buf, sizeof(buf), sizeof(buf), 0);

    csm_response resp;
    csm_client_init(NULL, &resp);

    REQUIRE(csm_client_decode(NULL, &arr) == 0);
    REQUIRE(csm_client_decode(&resp, NULL) == 0);
    csm_client_init(NULL, NULL);
    REQUIRE(csm_client_has_more_data(NULL) == 0);
}

TEST_CASE("cosemlib.h provides server/client API", "[cosemlib]")
{
    /* Verify server/client types are forward-declared (opaque, pointer-only) */
    csm_server *server = NULL;
    csm_client *client = NULL;
    (void)server;
    (void)client;

    /* Types compile — actual functionality tested in integration tests */
    REQUIRE(true);
}

/* ── Basic API smoke tests ───────────────────────────────────────────────── */

TEST_CASE("csm_array basic operations", "[cosemlib][array]")
{
    uint8_t buf[64];
    csm_array arr;
    csm_array_init(&arr, buf, sizeof(buf), 0, 0);

    /* Write some data */
    REQUIRE(csm_array_write_u8(&arr, 0xC0) == 1);
    REQUIRE(csm_array_write_u8(&arr, 0x01) == 1);
    REQUIRE(csm_array_write_u16(&arr, 0x1234) == 1);
    REQUIRE(csm_array_written(&arr) == 4);

    /* Read it back */
    uint8_t u8;
    uint16_t u16;
    arr.rd_index = 0;
    REQUIRE(csm_array_read_u8(&arr, &u8) == 1);
    REQUIRE(u8 == 0xC0);
    REQUIRE(csm_array_read_u8(&arr, &u8) == 1);
    REQUIRE(u8 == 0x01);
    REQUIRE(csm_array_read_u16(&arr, &u16) == 1);
    REQUIRE(u16 == 0x1234);
}

TEST_CASE("os_util big-endian 64-bit helpers round-trip", "[cosemlib][os_util]")
{
    uint8_t buf[8];
    PUT_BE64(buf, 0x0102030405060708ULL);

    REQUIRE(buf[0] == 0x01U);
    REQUIRE(buf[1] == 0x02U);
    REQUIRE(buf[2] == 0x03U);
    REQUIRE(buf[3] == 0x04U);
    REQUIRE(buf[4] == 0x05U);
    REQUIRE(buf[5] == 0x06U);
    REQUIRE(buf[6] == 0x07U);
    REQUIRE(buf[7] == 0x08U);
    REQUIRE(GET_BE64(buf) == 0x0102030405060708ULL);
}

TEST_CASE("BER length encoding uses standard short and long forms", "[cosemlib][ber]")
{
    uint8_t buf[16];
    csm_array arr;
    csm_array_init(&arr, buf, sizeof(buf), 0, 0);

    REQUIRE(csm_ber_write_len(&arr, 127U) == 1);
    REQUIRE(csm_ber_write_len(&arr, 128U) == 1);
    REQUIRE(csm_ber_write_len(&arr, 255U) == 1);
    REQUIRE(csm_ber_write_len(&arr, 256U) == 1);

    const uint8_t expected[] = {
        0x7F,
        0x81, 0x80,
        0x81, 0xFF,
        0x82, 0x01, 0x00
    };
    REQUIRE(csm_array_written(&arr) == sizeof(expected));
    REQUIRE(std::memcmp(buf, expected, sizeof(expected)) == 0);

    arr.rd_index = 0;
    ber_length len;
    REQUIRE(csm_ber_read_len(&arr, &len) == 1);
    REQUIRE(len.length == 127U);
    REQUIRE(csm_ber_read_len(&arr, &len) == 1);
    REQUIRE(len.length == 128U);
    REQUIRE(csm_ber_read_len(&arr, &len) == 1);
    REQUIRE(len.length == 255U);
    REQUIRE(csm_ber_read_len(&arr, &len) == 1);
    REQUIRE(len.length == 256U);
}

TEST_CASE("BER helpers reject null and truncated inputs", "[cosemlib][ber]")
{
    uint8_t buf[] = {0x60, 0x85, 0x74, 0x05, 0x08, 0x01};
    csm_array arr;
    csm_array_init(&arr, buf, sizeof(buf), sizeof(buf), 0);
    ber_length len;
    csm_ber ber;
    static const uint8_t oid_header[] = {0x60, 0x85, 0x74, 0x05, 0x08};
    ber_object_identifier oid = { oid_header, sizeof(oid_header), 0U, 0U };

    REQUIRE(csm_ber_read_len(NULL, &len) == 0);
    REQUIRE(csm_ber_read_len(&arr, NULL) == 0);
    REQUIRE(csm_ber_decode(NULL, &arr) == 0);
    REQUIRE(csm_ber_decode(&ber, NULL) == 0);
    REQUIRE(csm_ber_decode_object_identifier(NULL, &arr) == 0);
    REQUIRE(csm_ber_decode_object_identifier(&oid, NULL) == 0);
    REQUIRE(csm_ber_decode_object_identifier(&oid, &arr) == 0);
}

TEST_CASE("csm_block_state lifecycle", "[cosemlib][block_transfer]")
{
    csm_block_state state;
    csm_block_init(&state);

    REQUIRE(state.direction == CSM_BLOCK_DIR_NONE);
    REQUIRE(state.active == 0U);
    REQUIRE(state.data == nullptr);

    /* Start a server→client transfer */
    static const uint8_t data[] = {1, 2, 3, 4, 5};
    REQUIRE(csm_block_start_server(&state, 0x01, data, sizeof(data), 0) == 1);
    REQUIRE(state.active == 1U);
    REQUIRE(state.direction == CSM_BLOCK_DIR_SERVER_TO_CLIENT);

    /* Encode in one block */
    uint8_t buf[32];
    csm_array arr;
    csm_array_init(&arr, buf, sizeof(buf), 0, 0);
    REQUIRE(csm_block_encode_first(&state, &arr, sizeof(buf)) == 1);
    REQUIRE(state.active == 0U);
    REQUIRE(csm_array_written(&arr) == 8 + sizeof(data));
}

TEST_CASE("csm_channel_ctx API", "[cosemlib][channel]")
{
    csm_channel channels[2];
    csm_asso_state assos[2];
    csm_asso_config configs[2];

    configs[0].llc.ssap = 0;
    configs[0].llc.dsap = 1;
    configs[1].llc.ssap = 1;
    configs[1].llc.dsap = 0;

    csm_channel_ctx ctx;
    csm_channel_ctx_init(&ctx, channels, 2, assos, configs, 2);

    REQUIRE(ctx.channels == channels);
    REQUIRE(ctx.channel_size == 2);
    REQUIRE(ctx.asso_states == assos);
    REQUIRE(ctx.asso_configs == configs);
    REQUIRE(ctx.asso_size == 2);
    REQUIRE(ctx.db_handler == nullptr);

    /* New channel */
    uint8_t ch = csm_channel_new_ctx(&ctx);
    REQUIRE(ch == 1U);
    REQUIRE(channels[0].request.channel_id == 1U);

    /* Disconnect */
    csm_channel_disconnect_ctx(&ctx, ch);
    REQUIRE(channels[0].request.channel_id == INVALID_CHANNEL_ID);
}
