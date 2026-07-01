/**
 * \file test_cosemlib.cpp
 * \brief Tests for cosemlib umbrella header and basic API
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "catch.hpp"
#include "cosemlib.h"
#include "csm_axdr_codec.h"
#include "csm_ber.h"
#include "csm_keyring.h"
#include "csm_model_catalog.h"
#include "csm_model_instance.h"
#include "csm_model_object_list.h"
#include "csm_security_suite.h"
#include "os_util.h"
#include <cstring>
#include <string>

extern "C" void csm_sys_init();

static uint32_t axdr_callback_calls = 0U;

static void count_axdr_callback(uint8_t type, uint32_t size, uint8_t *data)
{
    (void) type;
    (void) size;
    (void) data;
    axdr_callback_calls++;
}

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

    csm_request req;
    memset(&req, 0, sizeof(req));
    REQUIRE(svc_request_encoder(NULL, &arr) == FALSE);
    REQUIRE(svc_request_encoder(&req, NULL) == FALSE);
    REQUIRE(csm_server_services_execute_handler(
        reinterpret_cast<csm_db_access_handler>(1), NULL, NULL, &req, &arr) == 0);
    REQUIRE(csm_server_services_execute_handler(
        reinterpret_cast<csm_db_access_handler>(1), NULL,
        reinterpret_cast<csm_asso_state *>(1), NULL, &arr) == 0);

    csm_object_t object;
    memset(&object, 0, sizeof(object));
    REQUIRE(csm_client_encode_selective_access_by_range(NULL, &object, &arr, &arr) == FALSE);
    REQUIRE(csm_client_encode_selective_access_by_range(&arr, NULL, &arr, &arr) == FALSE);
    REQUIRE(csm_client_encode_selective_access_by_range(&arr, &object, NULL, &arr) == FALSE);
    REQUIRE(csm_client_encode_selective_access_by_range(&arr, &object, &arr, NULL) == FALSE);
}

TEST_CASE("client decode preserves state on unknown response", "[cosemlib][services]")
{
    uint8_t buf[] = {0xAAU, 0x01U, 0x02U};
    csm_array arr;
    csm_array_init(&arr, buf, sizeof(buf), sizeof(buf), 0U);

    csm_response resp;
    csm_client_init(NULL, &resp);
    resp.service = SVC_GET;
    resp.invoke_id = 7U;

    REQUIRE(csm_client_decode(&resp, &arr) == FALSE);
    REQUIRE(arr.rd_index == 0U);
    REQUIRE(resp.service == SVC_GET);
    REQUIRE(resp.invoke_id == 7U);
}

TEST_CASE("client block response rejects corrupted array bounds", "[cosemlib][services]")
{
    uint8_t buf[] = {
        AXDR_GET_RESPONSE,
        0x02U,
        0x01U,
        0x01U,
        0x00U, 0x00U, 0x00U, 0x01U,
        0xAAU
    };
    csm_array arr;
    csm_array_init(&arr, buf, sizeof(buf), sizeof(buf), 0U);
    arr.rd_index = sizeof(buf) + 1U;

    csm_response resp;
    csm_client_init(NULL, &resp);

    REQUIRE(csm_client_decode(&resp, &arr) == FALSE);
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

    csm_array corrupt = {};
    corrupt.size = 7U;
    corrupt.wr_index = 7U;
    REQUIRE(csm_ber_decode_object_identifier(&oid, &corrupt) == 0);
}

TEST_CASE("keyring rejects invalid counts", "[cosemlib][keyring]")
{
    csm_keyring kr;
    csm_keyring_init(&kr);
    uint8_t key[16] = {};

    REQUIRE(csm_keyring_add(&kr, 1U, key, sizeof(key)) == 0);
    REQUIRE(csm_keyring_find(&kr, 1U) != nullptr);

    kr.count = CSM_KEYRING_MAX_KEYS + 1U;
    REQUIRE(csm_keyring_add(&kr, 2U, key, sizeof(key)) == -1);
    REQUIRE(csm_keyring_find(&kr, 2U) == nullptr);
}

TEST_CASE("security suite support predicate is boolean", "[cosemlib][security]")
{
    csm_cipher_id cipher;
    csm_mac_id mac;

    REQUIRE(csm_sec_suite_get_algorithms(0U, &cipher, &mac) == 0);
    REQUIRE(cipher == CSM_CIPHER_AES_GCM);
    REQUIRE(mac == CSM_MAC_AES_GMAC);

    REQUIRE(csm_sec_suite_get_algorithms(8U, &cipher, &mac) == 0);
    REQUIRE(cipher == CSM_CIPHER_KUZNYECHIK_GCM);
    REQUIRE(mac == CSM_MAC_STREEBOG_256_CMAC);

    REQUIRE(csm_sec_suite_get_algorithms(6U, &cipher, &mac) == -1);
    REQUIRE(csm_sec_suite_get_algorithms(0U, nullptr, &mac) == -1);
    REQUIRE(csm_sec_suite_get_algorithms(0U, &cipher, nullptr) == -1);

    REQUIRE(csm_sec_suite_is_supported(0U) == 1);
    REQUIRE(csm_sec_suite_is_supported(5U) == 1);
    REQUIRE(csm_sec_suite_is_supported(8U) == 1);
    REQUIRE(csm_sec_suite_is_supported(9U) == 1);
    REQUIRE(csm_sec_suite_is_supported(6U) == 0);
    REQUIRE(csm_sec_suite_is_supported(7U) == 0);
    REQUIRE(csm_sec_suite_is_supported(10U) == 0);
}

TEST_CASE("catalog parser handles final line and rejects overlong lines", "[cosemlib][catalog]")
{
    REQUIRE(csm_model_catalog_load_yaml(nullptr) == FALSE);

    const char yaml[] =
        "catalog:\n"
        "  - class_id: 8\n"
        "    logical_name: \"0.0.1.0.0.255\"\n"
        "    version: 0";

    REQUIRE(csm_model_catalog_parse_buffer(yaml, sizeof(yaml) - 1U) == TRUE);
    REQUIRE(csm_model_catalog_count() == 1);

    const csm_object_t *obj = csm_model_catalog_get(0);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->class_id == 8U);
    REQUIRE(obj->version == 0U);
    REQUIRE(obj->obis.C == 1U);
    REQUIRE(obj->obis.F == 255U);

    const std::string overlong =
        "catalog:\n"
        "  - class_id: 1\n"
        "    logical_name: \"" + std::string(150, '1') + "\"\n";
    REQUIRE(csm_model_catalog_parse_buffer(overlong.c_str(), overlong.size()) == FALSE);
    REQUIRE(csm_model_catalog_count() == 0);
}

TEST_CASE("catalog parser rejects overflowing numeric fields", "[cosemlib][catalog]")
{
    const char class_overflow[] =
        "catalog:\n"
        "  - class_id: 42949672960\n"
        "    logical_name: \"0.0.1.0.0.255\"\n"
        "    version: 0\n";
    REQUIRE(csm_model_catalog_parse_buffer(class_overflow, sizeof(class_overflow) - 1U) == FALSE);
    REQUIRE(csm_model_catalog_count() == 0);

    const char obis_overflow[] =
        "catalog:\n"
        "  - class_id: 8\n"
        "    logical_name: \"0.0.42949672960.0.0.255\"\n"
        "    version: 0\n";
    REQUIRE(csm_model_catalog_parse_buffer(obis_overflow, sizeof(obis_overflow) - 1U) == FALSE);
    REQUIRE(csm_model_catalog_count() == 0);
}

TEST_CASE("object list import round-trips exported access rights", "[cosemlib][model]")
{
    csm_model_instance_reset();

    const csm_obis_code clock = {0U, 0U, 1U, 0U, 0U, 255U};
    const csm_obis_code association = {0U, 0U, 40U, 0U, 0U, 255U};
    REQUIRE(csm_model_instance_add(8U, &clock, 0U) == TRUE);
    REQUIRE(csm_model_instance_add(15U, &association, 1U) == TRUE);

    uint8_t buf[128];
    csm_array out;
    csm_array_init(&out, buf, sizeof(buf), 0U, 0U);
    REQUIRE(csm_model_export_object_list(&out) == CSM_OK);
    const uint32_t written = csm_array_written(&out);

    csm_model_instance_reset();
    csm_array in;
    csm_array_init(&in, buf, sizeof(buf), written, 0U);
    REQUIRE(csm_model_import_object_list(&in) == CSM_OK);

    REQUIRE(csm_model_instance_count() == 2);
    REQUIRE(csm_model_instance_find(8U, &clock) != nullptr);
    REQUIRE(csm_model_instance_find(15U, &association) != nullptr);

    buf[written] = 0x00U;
    csm_array tampered;
    csm_array_init(&tampered, buf, sizeof(buf), written + 1U, 0U);
    REQUIRE(csm_model_import_object_list(&tampered) == CSM_ERR_BAD_ENCODING);
    REQUIRE(csm_model_instance_count() == 0);
}

TEST_CASE("AXDR boolean writer uses boolean tag", "[cosemlib][axdr]")
{
    uint8_t buf[4] = {};
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    REQUIRE(csm_axdr_wr_boolean(&array, 1U) == TRUE);
    REQUIRE(csm_array_written(&array) == 2U);
    REQUIRE(buf[0] == AXDR_TAG_BOOLEAN);
    REQUIRE(buf[1] == 1U);
}

TEST_CASE("AXDR read helpers preserve position on failure", "[cosemlib][axdr]")
{
    uint8_t octet_buf[] = {AXDR_TAG_BOOLEAN, 0x01U};
    csm_array array;
    uint32_t size = 0U;

    csm_array_init(&array, octet_buf, sizeof(octet_buf), sizeof(octet_buf), 0U);
    REQUIRE(csm_axdr_rd_octetstring(&array, &size) == FALSE);
    REQUIRE(array.rd_index == 0U);
    REQUIRE(csm_axdr_rd_octetstring(&array, nullptr) == FALSE);
    REQUIRE(array.rd_index == 0U);
    REQUIRE(csm_axdr_rd_null(&array) == FALSE);
    REQUIRE(array.rd_index == 0U);

    uint8_t block_buf[] = {0x01U, 0x00U};
    csm_array_init(&array, block_buf, sizeof(block_buf), sizeof(block_buf), 0U);
    REQUIRE(csm_axdr_decode_block(&array, &size) == FALSE);
    REQUIRE(array.rd_index == 0U);
    REQUIRE(csm_axdr_decode_block(&array, nullptr) == FALSE);
    REQUIRE(array.rd_index == 0U);
}

TEST_CASE("AXDR decode rejects excessive nesting", "[cosemlib][axdr]")
{
    uint8_t buf[80] = {};
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);

    for (uint32_t i = 0U; i < 34U; i++)
    {
        REQUIRE(csm_array_write_u8(&array, AXDR_TAG_STRUCTURE) == TRUE);
        REQUIRE(csm_array_write_u8(&array, 1U) == TRUE);
    }
    REQUIRE(csm_array_write_u8(&array, AXDR_TAG_NULL) == TRUE);

    array.rd_index = 0U;
    REQUIRE(csm_axdr_decode_tags(&array, nullptr) == FALSE);
}

TEST_CASE("AXDR decode does not callback on truncated primitive payload", "[cosemlib][axdr]")
{
    uint8_t buf[] = { AXDR_TAG_OCTETSTRING, 0x04U, 0xAAU, 0xBBU };
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), sizeof(buf), 0U);

    axdr_callback_calls = 0U;
    REQUIRE(csm_axdr_decode_tags(&array, count_axdr_callback) == FALSE);
    REQUIRE(axdr_callback_calls == 0U);
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

    uint8_t pkt_buf[4] = {};
    csm_array pkt;
    csm_array_init(&pkt, pkt_buf, sizeof(pkt_buf), sizeof(pkt_buf), 0U);
    REQUIRE(csm_channel_execute_ctx(&ctx, nullptr, 2U, &pkt) == FALSE);
    REQUIRE(csm_channel_execute_ctx(&ctx, nullptr, 0U, nullptr) == FALSE);

    csm_channel_ctx incomplete;
    csm_channel_ctx_init(&incomplete, nullptr, 1U, nullptr, configs, 1U);
    REQUIRE(csm_channel_execute_ctx(&incomplete, nullptr, 0U, &pkt) == FALSE);

    csm_request req;
    memset(&req, 0, sizeof(req));
    REQUIRE(csm_channel_hls_pass3_ctx(&ctx, &pkt, nullptr) == FALSE);
    REQUIRE(csm_channel_hls_pass3_ctx(&ctx, nullptr, &req) == FALSE);
    REQUIRE(csm_channel_hls_pass4_ctx(&ctx, &pkt, nullptr) == FALSE);
    REQUIRE(csm_channel_hls_pass4_ctx(&ctx, nullptr, &req) == FALSE);

    req.channel_id = 3U;
    REQUIRE(csm_channel_hls_pass3_ctx(&ctx, &pkt, &req) == FALSE);
    REQUIRE(csm_channel_hls_pass4_ctx(&ctx, &pkt, &req) == FALSE);
}
