/**
 * \file test_integration.cpp
 * \brief Integration tests for OpenDLMS server stack
 *
 * 49 tests covering basic operations, ciphered sessions, service layer,
 * association management, and security setup.
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#include <cstring>
#include <cstdlib>

extern "C" {
#include "csm_array.h"
#include "csm_channel.h"
#include "csm_association.h"
#include "csm_config.h"
#include "csm_services.h"
#include "csm_definitions.h"
#include "csm_axdr_codec.h"
#include "csm_security.h"
#include "csm_ber.h"
#include "db_cosem_ic.h"
#include "os_util.h"
#include "server_config.h"
}

#include "catch.hpp"

extern "C" void csm_sys_init(void);

static csm_channel test_channels[NUMBER_OF_CHANNELS];
static csm_asso_state test_asso_states[NUMBER_OF_CHANNELS];
static csm_db_context_t test_db_ctx = { NULL, 0 };

static const csm_asso_config test_asso_configs[NUMBER_OF_CHANNELS] = {
    { { 0x00, 0x01 }, 0xFFFFFFFFU, 0 },
    { { 0x40, 0x01 }, 0xFFFFFFFFU, 0 }
};

static const csm_obis_code obis_data       = { 0, 0, 96, 1, 0, 255 };
static const csm_obis_code obis_register    = { 0, 0, 10, 0, 0, 255 };
static const csm_obis_code obis_clock       = { 0, 0, 1, 0, 0, 255 };
static const csm_obis_code obis_asso        = { 0, 0, 40, 0, 0, 255 };
static const csm_obis_code obis_profile     = { 0, 0, 96, 1, 1, 255 };
static const csm_obis_code obis_security    = { 0, 0, 43, 0, 0, 255 };
static const csm_obis_code obis_push        = { 0, 0, 25, 1, 0, 255 };
static const csm_obis_code obis_disconnect  = { 0, 0, 96, 3, 10, 255 };
static const csm_obis_code obis_ext_reg     = { 0, 0, 10, 1, 0, 255 };
static const csm_obis_code obis_demand_reg  = { 0, 0, 10, 2, 0, 255 };
static const csm_obis_code obis_nonexist    = { 0, 0, 99, 9, 9, 255 };

static csm_db_code test_db_access(csm_db_context_t *ctx, csm_array *in,
                                   csm_array *out, csm_request *request)
{
    (void) ctx;
    if (!in || !out || !request) return CSM_ERR_OBJECT_ERROR;

    db_ic_inst_t *inst = NULL;
    if (!db_ic_find(request->db_request.logical_name.class_id,
                    &request->db_request.logical_name.obis, &inst) || !inst)
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

    db_ic_op_t op;
    uint8_t attr_id = request->db_request.logical_name.id;
    uint8_t method_id = 0U;

    switch (request->db_request.service)
    {
    case SVC_GET:    op = IC_OP_GET;    break;
    case SVC_SET:    op = IC_OP_SET;    break;
    case SVC_ACTION: op = IC_OP_ACTION; method_id = attr_id; break;
    default: return CSM_ERR_OBJECT_ERROR;
    }

    return (csm_db_code) db_ic_dispatch(inst, op, attr_id, method_id, in, out);
}

static void test_stack_setup(void)
{
    csm_sys_init();
    db_ic_init();
    db_ic_register_all_builtins();

    db_ic_create_inst(1,  &obis_data,       NULL, NULL);
    db_ic_create_inst(3,  &obis_register,    NULL, NULL);
    db_ic_create_inst(4,  &obis_ext_reg,     NULL, NULL);
    db_ic_create_inst(5,  &obis_demand_reg,  NULL, NULL);
    db_ic_create_inst(7,  &obis_profile,     NULL, NULL);
    db_ic_create_inst(8,  &obis_clock,       NULL, NULL);
    db_ic_create_inst(15, &obis_asso,        NULL, NULL);
    db_ic_create_inst(40, &obis_push,        NULL, NULL);
    db_ic_create_inst(64, &obis_security,    NULL, NULL);
    db_ic_create_inst(70, &obis_disconnect,  NULL, NULL);

    csm_channel_init(test_channels, NUMBER_OF_CHANNELS,
                     test_asso_states, test_asso_configs, NUMBER_OF_CHANNELS);
    csm_services_init(test_db_access);
}

static int test_do_aarq(uint8_t auth_level, uint8_t ref)
{
    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_asso_state state;
    csm_asso_init(&state);
    state.auth_level = (enum csm_auth_level) auth_level;
    state.ref = (enum csm_referencing) ref;

    if (csm_asso_encoder(&state, &pkt, CSM_ASSO_AARQ) != TRUE)
    {
        return 0;
    }

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;

    return csm_channel_execute(&test_db_ctx, 0, &pkt);
}

static int test_establish_association(void)
{
    int ret = test_do_aarq(CSM_AUTH_LOWEST_LEVEL, LN_REF);
    REQUIRE(ret > 0);
    return ret;
}

static void test_build_get(csm_array *pkt, uint8_t invoke_id, uint16_t class_id,
                           const csm_obis_code *obis, uint8_t attr_id)
{
    csm_array_write_u8(pkt, 0xC0);
    csm_array_write_u8(pkt, 0x01);
    csm_array_write_u8(pkt, invoke_id);
    csm_array_write_u16(pkt, class_id);
    csm_array_write_u8(pkt, obis->A);
    csm_array_write_u8(pkt, obis->B);
    csm_array_write_u8(pkt, obis->C);
    csm_array_write_u8(pkt, obis->D);
    csm_array_write_u8(pkt, obis->E);
    csm_array_write_u8(pkt, obis->F);
    csm_array_write_u8(pkt, attr_id);
    csm_array_write_u8(pkt, 0x00);
}

static void test_build_set(csm_array *pkt, uint8_t invoke_id, uint16_t class_id,
                           const csm_obis_code *obis, uint8_t attr_id,
                           const uint8_t *data, uint8_t data_len)
{
    csm_array_write_u8(pkt, 0xC1);
    csm_array_write_u8(pkt, 0x01);
    csm_array_write_u8(pkt, invoke_id);
    csm_array_write_u16(pkt, class_id);
    csm_array_write_u8(pkt, obis->A);
    csm_array_write_u8(pkt, obis->B);
    csm_array_write_u8(pkt, obis->C);
    csm_array_write_u8(pkt, obis->D);
    csm_array_write_u8(pkt, obis->E);
    csm_array_write_u8(pkt, obis->F);
    csm_array_write_u8(pkt, attr_id);
    csm_array_write_u8(pkt, 0x00);
    csm_array_write_u8(pkt, 0x01);
    if (data && data_len > 0)
    {
        csm_array_write_buff(pkt, data, data_len);
    }
}

static void test_build_action(csm_array *pkt, uint8_t invoke_id, uint16_t class_id,
                              const csm_obis_code *obis, uint8_t method_id,
                              const uint8_t *data, uint8_t data_len)
{
    csm_array_write_u8(pkt, 0xC3);
    csm_array_write_u8(pkt, 0x01);
    csm_array_write_u8(pkt, invoke_id);
    csm_array_write_u16(pkt, class_id);
    csm_array_write_u8(pkt, obis->A);
    csm_array_write_u8(pkt, obis->B);
    csm_array_write_u8(pkt, obis->C);
    csm_array_write_u8(pkt, obis->D);
    csm_array_write_u8(pkt, obis->E);
    csm_array_write_u8(pkt, obis->F);
    csm_array_write_u8(pkt, method_id);
    if (data && data_len > 0)
    {
        csm_array_write_u8(pkt, 0x01);
        csm_array_write_buff(pkt, data, data_len);
    }
    else
    {
        csm_array_write_u8(pkt, 0x00);
    }
}

static int test_do_get(uint8_t invoke_id, uint16_t class_id,
                       const csm_obis_code *obis, uint8_t attr_id,
                       uint8_t *out_buf, uint32_t out_size)
{
    csm_array pkt;
    csm_array_init(&pkt, out_buf, out_size, 0, 0);
    test_build_get(&pkt, invoke_id, class_id, obis, attr_id);
    return csm_channel_execute(&test_db_ctx, 0, &pkt);
}

static int test_do_set(uint8_t invoke_id, uint16_t class_id,
                       const csm_obis_code *obis, uint8_t attr_id,
                       const uint8_t *data, uint8_t data_len,
                       uint8_t *out_buf, uint32_t out_size)
{
    csm_array pkt;
    csm_array_init(&pkt, out_buf, out_size, 0, 0);
    test_build_set(&pkt, invoke_id, class_id, obis, attr_id, data, data_len);
    return csm_channel_execute(&test_db_ctx, 0, &pkt);
}

static int test_do_action(uint8_t invoke_id, uint16_t class_id,
                          const csm_obis_code *obis, uint8_t method_id,
                          const uint8_t *data, uint8_t data_len,
                          uint8_t *out_buf, uint32_t out_size)
{
    csm_array pkt;
    csm_array_init(&pkt, out_buf, out_size, 0, 0);
    test_build_action(&pkt, invoke_id, class_id, obis, method_id, data, data_len);
    return csm_channel_execute(&test_db_ctx, 0, &pkt);
}

/* ========================= Basic Tests (10) ========================= */

TEST_CASE("Integration_AarqHandshake", "[integration][basic]")
{
    test_stack_setup();
    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_asso_state state;
    csm_asso_init(&state);
    state.auth_level = CSM_AUTH_LOWEST_LEVEL;
    state.ref = LN_REF;

    REQUIRE(csm_asso_encoder(&state, &pkt, CSM_ASSO_AARQ) == TRUE);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0x61);
}

TEST_CASE("Integration_GetClockTime", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 8, &obis_clock, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[1] == 0x01);
    REQUIRE(buf[2] == 0x01);
    REQUIRE(buf[3] == 0x00);
}

TEST_CASE("Integration_GetObjectNotFound", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 1, &obis_nonexist, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xD8);
}

TEST_CASE("Integration_ReadAssociationObject", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 15, &obis_asso, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == 0x01);
}

TEST_CASE("Integration_SetDataValue", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t set_data[] = { 0x06, 0x00, 0x00, 0x00, 0x2A };
    uint8_t buf[1024];

    int ret = test_do_set(0x01, 1, &obis_data, 2,
                          set_data, sizeof(set_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 1, &obis_data, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == 0x06);
    REQUIRE(get_buf[5] == 0x00);
    REQUIRE(get_buf[6] == 0x00);
    REQUIRE(get_buf[7] == 0x00);
    REQUIRE(get_buf[8] == 0x2A);
}

TEST_CASE("Integration_ResetRegister", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 3, &obis_register, 1,
                             NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);
}

TEST_CASE("Integration_MultipleGets", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret;

    ret = test_do_get(0x01, 8, &obis_clock, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);

    ret = test_do_get(0x02, 1, &obis_data, 1, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);

    ret = test_do_get(0x03, 3, &obis_register, 1, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

TEST_CASE("Integration_ExceptionResponse", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_array_write_u8(&pkt, 0xC0);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u16(&pkt, 999);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u8(&pkt, 0xFF);
    csm_array_write_u8(&pkt, 2);
    csm_array_write_u8(&pkt, 0x00);

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xD8);
}

TEST_CASE("Integration_BlockTransfer", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 7, &obis_profile, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
}

TEST_CASE("Integration_SelectiveAccess", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 7, &obis_profile, 3, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
}

/* ========================= Ciphered Tests (10) ========================= */

TEST_CASE("Integration_CipheredAarqHandshake", "[integration][ciphered]")
{
    test_stack_setup();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_asso_state state;
    csm_asso_init(&state);
    state.auth_level = CSM_AUTH_HIGH_LEVEL_GMAC;
    state.ref = LN_REF_WITH_CYPHERING;

    REQUIRE(csm_asso_encoder(&state, &pkt, CSM_ASSO_AARQ) == TRUE);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0x61);
}

TEST_CASE("Integration_CipheredGetClockTime", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000001U);
    csm_array_writer_jump(&pkt, 12U);

    test_build_get(&pkt, 0x01, 8, &obis_clock, 2);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_CipheredGetClockTimeSuite2", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 2;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000001U);
    csm_array_writer_jump(&pkt, 12U);

    test_build_get(&pkt, 0x01, 8, &obis_clock, 2);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_ClientCipherGetClockTime", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.encryption = 1;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000001U);
    csm_array_writer_jump(&pkt, 12U);

    uint8_t enc_data[] = { 0xC0, 0x01, 0x01, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x02, 0x00 };
    csm_array_write_buff(&pkt, enc_data, sizeof(enc_data));
    csm_array_writer_jump(&pkt, 12U);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_ClientCipherAccessBatchGet", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.encryption = 1;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000002U);
    csm_array_writer_jump(&pkt, 12U);

    csm_array_writer_jump(&pkt, 16U);
    csm_array_writer_jump(&pkt, 12U);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_EventNotificationCipherUnsolicited", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000001U);
    csm_array_writer_jump(&pkt, 12U);

    csm_array_write_u8(&pkt, 0x08);
    csm_array_writer_jump(&pkt, 16U);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_CipheredTamperedTag", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000001U);
    csm_array_writer_jump(&pkt, 12U);

    uint8_t fake_tag[12] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    csm_array_write_buff(&pkt, fake_tag, sizeof(fake_tag));

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret == 0);
}

TEST_CASE("Integration_CipheredReplayProtection", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000000U);
    csm_array_writer_jump(&pkt, 12U);

    csm_array_writer_jump(&pkt, 12U);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_CipheredGbtGetClockTime", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000001U);
    csm_array_writer_jump(&pkt, 12U);

    csm_array_write_u8(&pkt, 0xC0);
    csm_array_write_u8(&pkt, 0x02);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u32(&pkt, 0x00000001U);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_CipheredIcPersistedAcrossRestart", "[integration][ciphered]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf1[1024];
    csm_array pkt1;
    csm_array_init(&pkt1, buf1, sizeof(buf1), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt1, sc.sh_byte);
    csm_array_write_u32(&pkt1, 0x00000001U);
    csm_array_writer_jump(&pkt1, 12U);
    csm_array_writer_jump(&pkt1, 12U);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt1);
    REQUIRE(ret >= 0);

    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf2[1024];
    csm_array pkt2;
    csm_array_init(&pkt2, buf2, sizeof(buf2), 0, 0);

    csm_array_write_u8(&pkt2, sc.sh_byte);
    csm_array_write_u32(&pkt2, 0x00000002U);
    csm_array_writer_jump(&pkt2, 12U);
    csm_array_writer_jump(&pkt2, 12U);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    ret = csm_channel_execute(&test_db_ctx, 0, &pkt2);
    REQUIRE(ret >= 0);
}

/* ========================= Service Tests (10) ========================= */

TEST_CASE("Integration_SetBlockTransfer", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t set_data[] = { 0x09, 0x03, 0x41, 0x42, 0x43 };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 1, &obis_data, 2,
                          set_data, sizeof(set_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
}

TEST_CASE("Integration_RegisterGetSet", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 3, &obis_register, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x02, 3, &obis_register, 3, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
}

TEST_CASE("Integration_ExtendedRegisterGetSet", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 4, &obis_ext_reg, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x02, 4, &obis_ext_reg, 3, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

TEST_CASE("Integration_DemandRegisterActions", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 5, &obis_demand_reg, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);

    ret = test_do_action(0x02, 5, &obis_demand_reg, 1, NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
}

TEST_CASE("Integration_ProfileGenericCaptureSelectiveAccess", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 7, &obis_profile, 2, NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);

    ret = test_do_get(0x02, 7, &obis_profile, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

TEST_CASE("Integration_ProfileGenericGetBlockTransfer", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 7, &obis_profile, 7, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);

    ret = test_do_get(0x02, 7, &obis_profile, 8, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

TEST_CASE("Integration_DataDeviceId", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t device_data[] = { 0x09, 0x04, 0x01, 0x02, 0x03, 0x04 };
    uint8_t set_buf[1024];
    int ret = test_do_set(0x01, 1, &obis_data, 2,
                          device_data, sizeof(device_data), set_buf, sizeof(set_buf));
    REQUIRE(ret > 0);
    REQUIRE(set_buf[0] == 0xC5);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 1, &obis_data, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[4] == 0x09);
}

TEST_CASE("Integration_ProfilePowerCapture", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 7, &obis_profile, 4, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
}

TEST_CASE("Integration_GbtServerWrap", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_array_write_u8(&pkt, 0xC0);
    csm_array_write_u8(&pkt, 0x02);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u32(&pkt, 0x00000001U);

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_GbtServerMultiBlockFragmentation", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_array_write_u8(&pkt, 0xC0);
    csm_array_write_u8(&pkt, 0x02);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u32(&pkt, 0x00000001U);

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);

    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);
    csm_array_write_u8(&pkt, 0xC0);
    csm_array_write_u8(&pkt, 0x02);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u32(&pkt, 0x00000002U);

    ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

/* ========================= Association Tests (10) ========================= */

TEST_CASE("Integration_AssociationObjectListIncludesIc", "[integration][assoc]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 15, &obis_asso, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == 0x01);
}

TEST_CASE("Integration_AssocObjectListSelByClass", "[integration][assoc]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 15, &obis_asso, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

TEST_CASE("Integration_AssocObjectListSelByObject", "[integration][assoc]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 15, &obis_asso, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

TEST_CASE("Integration_AssocLnV3SecuritySetupRef", "[integration][assoc]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 15, &obis_asso, 4, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);

    ret = test_do_get(0x02, 15, &obis_asso, 5, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

TEST_CASE("Integration_AssocLnV3UserManagement", "[integration][assoc]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 15, &obis_asso, 10, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[4] == 0x01);
}

TEST_CASE("Integration_AssocLnV3AddRemoveObject", "[integration][assoc]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t add_data[] = {
        0x02, 0x02,
        0x12, 0x00, 0x01,
        0x09, 0x06, 0x00, 0x00, 0x60, 0x01, 0x00, 0xFF
    };
    uint8_t buf[1024];
    int ret = test_do_action(0x01, 15, &obis_asso, 3,
                             add_data, sizeof(add_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);

    ret = test_do_get(0x02, 15, &obis_asso, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);

    uint8_t rm_data[] = {
        0x02, 0x02,
        0x12, 0x00, 0x01,
        0x09, 0x06, 0x00, 0x00, 0x60, 0x01, 0x00, 0xFF
    };
    ret = test_do_action(0x03, 15, &obis_asso, 4,
                         rm_data, sizeof(rm_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
}

TEST_CASE("Integration_AssocSnV4ObjectList", "[integration][assoc]")
{
    test_stack_setup();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_asso_state state;
    csm_asso_init(&state);
    state.auth_level = CSM_AUTH_LOWEST_LEVEL;
    state.ref = SN_REF;

    REQUIRE(csm_asso_encoder(&state, &pkt, CSM_ASSO_AARQ) == TRUE);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x02;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0x61);
}

TEST_CASE("Integration_AssocSnV4ObjectListSelByBaseName", "[integration][assoc]")
{
    test_stack_setup();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_asso_state state;
    csm_asso_init(&state);
    state.auth_level = CSM_AUTH_LOWEST_LEVEL;
    state.ref = SN_REF;

    REQUIRE(csm_asso_encoder(&state, &pkt, CSM_ASSO_AARQ) == TRUE);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x02;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0x61);
}

TEST_CASE("Integration_Ed4ConformanceNegotiation", "[integration][assoc]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 15, &obis_asso, 7, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

TEST_CASE("Integration_FrameworkConformanceNegotiation", "[integration][assoc]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 15, &obis_asso, 7, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
}

/* ========================= Security Tests (9) ========================= */

TEST_CASE("Integration_AccessBatchGet", "[integration][security]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_array_write_u8(&pkt, 0xC0);
    csm_array_write_u8(&pkt, 0x03);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u8(&pkt, 0x01);
    csm_array_write_u8(&pkt, 0x00);
    csm_array_write_u16(&pkt, 1);
    csm_array_write_u8(&pkt, obis_data.A);
    csm_array_write_u8(&pkt, obis_data.B);
    csm_array_write_u8(&pkt, obis_data.C);
    csm_array_write_u8(&pkt, obis_data.D);
    csm_array_write_u8(&pkt, obis_data.E);
    csm_array_write_u8(&pkt, obis_data.F);
    csm_array_write_u8(&pkt, 1);

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}

TEST_CASE("Integration_SecuritySetupDirectDb", "[integration][security]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 64, &obis_security, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
}

TEST_CASE("Integration_SecuritySetupKeyTransfer", "[integration][security]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t key_data[] = { 0x09, 0x10, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                           0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
                           0x0E, 0x0F };
    uint8_t buf[1024];
    int ret = test_do_action(0x01, 64, &obis_security, 2,
                             key_data, sizeof(key_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);
}

TEST_CASE("Integration_SecuritySetupKeyTransferSuite2", "[integration][security]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t key_data[] = { 0x09, 0x20,
                           0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                           0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                           0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F };
    uint8_t buf[1024];
    int ret = test_do_action(0x01, 64, &obis_security, 3,
                             key_data, sizeof(key_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
}

TEST_CASE("Integration_SecuritySetupSetSuite2ViaCoSem", "[integration][security]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t set_data[] = { 0x11, 0x02 };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 64, &obis_security, 3,
                          set_data, sizeof(set_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 64, &obis_security, 3, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[4] == 0x11);
    REQUIRE(get_buf[5] == 0x02);
}

TEST_CASE("Integration_SecuritySetupGetPolicy", "[integration][security]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 64, &obis_security, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[4] == 0x11);
}

TEST_CASE("Integration_SecuritySetupSecurityActivate", "[integration][security]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 64, &obis_security, 1,
                             NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
}

TEST_CASE("Integration_Hls5GmacHandshake", "[integration][security]")
{
    test_stack_setup();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_asso_state state;
    csm_asso_init(&state);
    state.auth_level = CSM_AUTH_HIGH_LEVEL_GMAC;
    state.ref = LN_REF_WITH_CYPHERING;

    REQUIRE(csm_asso_encoder(&state, &pkt, CSM_ASSO_AARQ) == TRUE);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0x61);
}

TEST_CASE("Integration_GeneralGloGetClockTime", "[integration][security]")
{
    test_stack_setup();
    test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING);

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);

    csm_sec_control_byte sc;
    sc.sh_byte = 0x00;
    sc.sh_bit_field.encryption = 1;
    sc.sh_bit_field.authentication = 1;
    sc.sh_bit_field.security_suite = 0;

    csm_array_write_u8(&pkt, sc.sh_byte);
    csm_array_write_u32(&pkt, 0x00000001U);

    uint8_t fake_system_title[8] = { 0x4D, 0x4D, 0x4D, 0x00, 0x00, 0xBC, 0x61, 0x4E };
    csm_array_write_buff(&pkt, fake_system_title, 8);
    csm_array_writer_jump(&pkt, 16U);
    csm_array_writer_jump(&pkt, 12U);

    test_channels[0].request.llc.ssap = 0x00;
    test_channels[0].request.llc.dsap = 0x01;
    test_channels[0].request.channel_id = 1;

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret >= 0);
}
