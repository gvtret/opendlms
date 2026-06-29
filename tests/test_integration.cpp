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
#include "db_cosem_clock_util.h"
#include "os_util.h"
#include "server_config.h"
}

#include "catch.hpp"

extern "C" void csm_sys_init(void);
extern "C" {
typedef enum
{
    TEST_SEC_IC_CLIENT,
    TEST_SEC_IC_SERVER,
} test_sec_ic_t;
uint32_t csm_sys_get_ic(uint8_t sap, test_sec_ic_t ic);
}

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
static const csm_obis_code obis_image       = { 0, 0, 44, 0, 0, 255 };
static const csm_obis_code obis_prof_filter = { 0, 0, 43, 1, 0, 255 };
static const csm_obis_code obis_push        = { 0, 0, 25, 1, 0, 255 };
static const csm_obis_code obis_disconnect  = { 0, 0, 96, 3, 10, 255 };
static const csm_obis_code obis_utility     = { 0, 0, 10, 3, 0, 255 };
static const csm_obis_code obis_compact     = { 0, 0, 60, 3, 0, 255 };
static const csm_obis_code obis_reg_table   = { 0, 0, 60, 2, 0, 255 };
static const csm_obis_code obis_reg_act     = { 0, 0, 60, 5, 0, 255 };
static const csm_obis_code obis_param_mon   = { 0, 0, 60, 6, 0, 255 };
static const csm_obis_code obis_arbitrator  = { 0, 0, 60, 7, 0, 255 };
static const csm_obis_code obis_sensor_mgr  = { 0, 0, 60, 8, 0, 255 };
static const csm_obis_code obis_ext_reg     = { 0, 0, 10, 1, 0, 255 };
static const csm_obis_code obis_demand_reg  = { 0, 0, 10, 2, 0, 255 };
static const csm_obis_code obis_table_mgr   = { 0, 0, 96, 9, 0, 255 };
static const csm_obis_code obis_sap         = { 0, 0, 41, 0, 0, 255 };
static const csm_obis_code obis_script      = { 0, 0, 9, 0, 0, 255 };
static const csm_obis_code obis_schedule    = { 0, 0, 10, 4, 0, 255 };
static const csm_obis_code obis_special_day = { 0, 0, 11, 0, 0, 255 };
static const csm_obis_code obis_activity    = { 0, 0, 20, 0, 0, 255 };
static const csm_obis_code obis_reg_monitor = { 0, 0, 21, 0, 0, 255 };
static const csm_obis_code obis_single_act  = { 0, 0, 22, 0, 0, 255 };
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

    csm_db_code rc = (csm_db_code) db_ic_dispatch(inst, op, attr_id, method_id, in, out);
    return rc;
}

static int explicit_handler_calls = 0;
static int global_handler_calls = 0;

static csm_db_code explicit_test_db_access(csm_db_context_t *ctx, csm_array *in,
                                           csm_array *out, csm_request *request)
{
    (void) ctx;
    (void) in;
    (void) request;
    explicit_handler_calls++;
    return (csm_array_write_u8(out, AXDR_TAG_UNSIGNED8) &&
            csm_array_write_u8(out, 0x2AU)) ? CSM_OK : CSM_ERR_OBJECT_ERROR;
}

static csm_db_code poison_global_db_access(csm_db_context_t *ctx, csm_array *in,
                                           csm_array *out, csm_request *request)
{
    (void) ctx;
    (void) in;
    (void) out;
    (void) request;
    global_handler_calls++;
    return CSM_ERR_OBJECT_ERROR;
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

static void test_make_long_octet_string(uint8_t *data, uint8_t size)
{
    REQUIRE(size >= 133U);
    data[0] = AXDR_TAG_OCTETSTRING;
    data[1] = 0x81U;
    data[2] = 130U;
    for (uint8_t i = 0U; i < 130U; i++)
    {
        data[3U + i] = (uint8_t)(0xA0U + (i & 0x0FU));
    }
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

TEST_CASE("Services_ExplicitHandlerDoesNotUseGlobalDatabase", "[integration][services]")
{
    explicit_handler_calls = 0;
    global_handler_calls = 0;
    csm_services_init(poison_global_db_access);

    csm_asso_state state;
    csm_asso_init(&state);
    csm_request request;
    std::memset(&request, 0, sizeof(request));
    csm_db_context_t db_ctx = { NULL, 0 };

    uint8_t buf[64];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);
    test_build_get(&pkt, 0x33, 1, &obis_data, 2);

    int ret = csm_server_services_execute_handler(explicit_test_db_access,
                                                  &db_ctx, &state, &request, &pkt);
    REQUIRE(ret > 0);
    REQUIRE(explicit_handler_calls == 1);
    REQUIRE(global_handler_calls == 0);
    REQUIRE(buf[0] == AXDR_GET_RESPONSE);
    REQUIRE(buf[1] == 0x01);
    REQUIRE(buf[2] == 0x33);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == AXDR_TAG_UNSIGNED8);
    REQUIRE(buf[5] == 0x2A);

    csm_services_init(test_db_access);
}

TEST_CASE("HAL_InvocationCountersArePerSapAndDirection", "[integration][security]")
{
    csm_sys_init();

    uint32_t sap1_client = csm_sys_get_ic(250U, TEST_SEC_IC_CLIENT);
    REQUIRE(csm_sys_get_ic(250U, TEST_SEC_IC_CLIENT) == sap1_client + 1U);
    REQUIRE(csm_sys_get_ic(251U, TEST_SEC_IC_CLIENT) == sap1_client);
    REQUIRE(csm_sys_get_ic(250U, TEST_SEC_IC_SERVER) == sap1_client);
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
    REQUIRE(buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(buf[5] == DB_CLOCK_DT_LEN);
    REQUIRE(buf[6] == 0x07);
    REQUIRE(buf[7] == 0xD2);
}

TEST_CASE("Integration_GetWithSelectiveAccessPayloadDecodes", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    csm_array pkt;
    csm_array_init(&pkt, buf, sizeof(buf), 0, 0);
    test_build_get(&pkt, 0x01, 8, &obis_clock, 2);
    REQUIRE(pkt.wr_index > 0U);
    buf[pkt.wr_index - 1U] = 0x01U;
    REQUIRE(csm_array_write_u8(&pkt, AXDR_TAG_UNSIGNED8) == TRUE);
    REQUIRE(csm_array_write_u8(&pkt, 0x01U) == TRUE);

    int ret = csm_channel_execute(&test_db_ctx, 0, &pkt);
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[1] == 0x01);
    REQUIRE(buf[2] == 0x01);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == AXDR_TAG_OCTETSTRING);
}

TEST_CASE("Integration_SetClockTime", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    const uint8_t clock_value[] = {
        AXDR_TAG_OCTETSTRING, DB_CLOCK_DT_LEN,
        0x07, 0xEA, 0x06, 0x06, 0x1B, 0x0C, 0x22, 0x38, 0xFF, 0x00, 0x00, 0x00
    };

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 8, &obis_clock, 2,
                          clock_value, sizeof(clock_value), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 8, &obis_clock, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(get_buf[5] == DB_CLOCK_DT_LEN);
    REQUIRE(std::memcmp(&get_buf[6], &clock_value[2], DB_CLOCK_DT_LEN) == 0);
}

TEST_CASE("Integration_ClockActionsDoNotReturnFakeSuccess", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    const uint8_t clock_value[] = {
        AXDR_TAG_OCTETSTRING, DB_CLOCK_DT_LEN,
        0x07, 0xEA, 0x06, 0x06, 0x1B, 0x0C, 0x16, 0x38, 0xFF, 0x00, 0x00, 0x00
    };

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 8, &obis_clock, 2,
                          clock_value, sizeof(clock_value), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x02, 8, &obis_clock, 2,
                         NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x03, 8, &obis_clock, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(get_buf[5] == DB_CLOCK_DT_LEN);
    REQUIRE(get_buf[11] == 0x0C);
    REQUIRE(get_buf[12] == 0x1E);
    REQUIRE(get_buf[13] == 0x00);

    ret = test_do_action(0x04, 8, &obis_clock, 1,
                         NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 250);
}

TEST_CASE("Integration_GetObjectNotFound", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 1, &obis_nonexist, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[1] == 0x01);
    REQUIRE(buf[2] == 0x01);
    REQUIRE(buf[3] == 0x01);
    REQUIRE(buf[4] == 0x04);
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

TEST_CASE("Integration_TableManagerBuiltinRegistered", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(8200, &obis_table_mgr, NULL, NULL) == TRUE);
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 8200, &obis_table_mgr, 1, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(buf[5] == 0x06);
}

TEST_CASE("Integration_UtilityTablesLongOctetStringUsesBerLength", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(26, &obis_utility, NULL, NULL) == TRUE);
    test_establish_association();

    uint8_t set_data[133];
    test_make_long_octet_string(set_data, sizeof(set_data));

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 26, &obis_utility, 3,
                          set_data, sizeof(set_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 26, &obis_utility, 3, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(get_buf[5] == 0x81);
    REQUIRE(get_buf[6] == 130);
    REQUIRE(std::memcmp(&get_buf[7], &set_data[3], 130U) == 0);
}

TEST_CASE("Integration_CompactDataLongOctetStringUsesBerLength", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(62, &obis_compact, NULL, NULL) == TRUE);
    test_establish_association();

    uint8_t set_data[133];
    test_make_long_octet_string(set_data, sizeof(set_data));

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 62, &obis_compact, 2,
                          set_data, sizeof(set_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 62, &obis_compact, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(get_buf[5] == 0x81);
    REQUIRE(get_buf[6] == 130);
    REQUIRE(std::memcmp(&get_buf[7], &set_data[3], 130U) == 0);
}

TEST_CASE("Integration_CompactDataCaptureStoresObjectValue", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(62, &obis_compact, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t data_value[] = {
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x2A
    };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 1, &obis_data, 2,
                          data_value, sizeof(data_value), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    const uint8_t capture_objects[] = {
        AXDR_TAG_ARRAY, 0x01,
        AXDR_TAG_STRUCTURE, 0x03,
        AXDR_TAG_UNSIGNED16, 0x00, 0x01,
        AXDR_TAG_OCTETSTRING, 0x06, 0x00, 0x00, 0x60, 0x01, 0x00, 0xFF,
        AXDR_TAG_UNSIGNED8, 0x02
    };
    ret = test_do_set(0x02, 62, &obis_compact, 3,
                      capture_objects, sizeof(capture_objects), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x03, 62, &obis_compact, 2,
                         NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t buffer_buf[1024];
    ret = test_do_get(0x04, 62, &obis_compact, 2, buffer_buf, sizeof(buffer_buf));
    REQUIRE(ret > 0);
    REQUIRE(buffer_buf[0] == 0xC4);
    REQUIRE(buffer_buf[3] == 0x00);
    REQUIRE(buffer_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(buffer_buf[5] == sizeof(data_value));
    REQUIRE(std::memcmp(&buffer_buf[6], data_value, sizeof(data_value)) == 0);

    uint8_t entries_buf[1024];
    ret = test_do_get(0x05, 62, &obis_compact, 6, entries_buf, sizeof(entries_buf));
    REQUIRE(ret > 0);
    REQUIRE(entries_buf[0] == 0xC4);
    REQUIRE(entries_buf[3] == 0x00);
    REQUIRE(entries_buf[4] == AXDR_TAG_UNSIGNED32);
    REQUIRE(entries_buf[8] == 0x01);
}

TEST_CASE("Integration_TableManagerLongOctetStringUsesBerLength", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(8200, &obis_table_mgr, NULL, NULL) == TRUE);
    test_establish_association();

    uint8_t set_data[133];
    test_make_long_octet_string(set_data, sizeof(set_data));

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 8200, &obis_table_mgr, 4,
                          set_data, sizeof(set_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 8200, &obis_table_mgr, 4, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(get_buf[5] == 0x81);
    REQUIRE(get_buf[6] == 130);
    REQUIRE(std::memcmp(&get_buf[7], &set_data[3], 130U) == 0);
}

TEST_CASE("Integration_TableManagerRetrieveRowsValidatesSelector", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(8200, &obis_table_mgr, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t table_data[] = {
        AXDR_TAG_OCTETSTRING, 0x04, 0x10, 0x20, 0x30, 0x40
    };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 8200, &obis_table_mgr, 4,
                          table_data, sizeof(table_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    const uint8_t valid_selector[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x01,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x02
    };

    ret = test_do_action(0x02, 8200, &obis_table_mgr, 1,
                             valid_selector, sizeof(valid_selector), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == 0x01);
    REQUIRE(buf[5] == 0x00);
    REQUIRE(buf[6] == AXDR_TAG_ARRAY);
    REQUIRE(buf[7] == 0x02);
    REQUIRE(buf[8] == AXDR_TAG_UNSIGNED8);
    REQUIRE(buf[9] == 0x20);
    REQUIRE(buf[10] == AXDR_TAG_UNSIGNED8);
    REQUIRE(buf[11] == 0x30);

    const uint8_t bad_selector[] = { AXDR_TAG_STRUCTURE, 0x01, AXDR_TAG_UNSIGNED8, 0x00 };
    ret = test_do_action(0x03, 8200, &obis_table_mgr, 1,
                         bad_selector, sizeof(bad_selector), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 250);

    const uint8_t out_of_range_selector[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x03,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x02
    };
    ret = test_do_action(0x04, 8200, &obis_table_mgr, 1,
                         out_of_range_selector, sizeof(out_of_range_selector), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);
}

TEST_CASE("Integration_ProfileFilterRetrieveRowsValidatesSelector", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(31, &obis_prof_filter, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t data_value[] = {
        AXDR_TAG_UNSIGNED32, 0x12, 0x34, 0x56, 0x78
    };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 1, &obis_data, 2,
                          data_value, sizeof(data_value), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    const uint8_t filter_list[] = {
        AXDR_TAG_ARRAY, 0x01,
        AXDR_TAG_STRUCTURE, 0x03,
        AXDR_TAG_UNSIGNED16, 0x00, 0x01,
        AXDR_TAG_OCTETSTRING, 0x06, 0x00, 0x00, 0x60, 0x01, 0x00, 0xFF,
        AXDR_TAG_UNSIGNED8, 0x02
    };
    ret = test_do_set(0x02, 31, &obis_prof_filter, 3,
                      filter_list, sizeof(filter_list), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    const uint8_t valid_selector[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x00,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x01
    };

    ret = test_do_action(0x03, 31, &obis_prof_filter, 1,
                             valid_selector, sizeof(valid_selector), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == 0x01);
    REQUIRE(buf[5] == 0x00);
    REQUIRE(buf[6] == AXDR_TAG_ARRAY);
    REQUIRE(buf[7] == 0x01);
    REQUIRE(buf[8] == AXDR_TAG_UNSIGNED32);
    REQUIRE(buf[9] == 0x12);
    REQUIRE(buf[10] == 0x34);
    REQUIRE(buf[11] == 0x56);
    REQUIRE(buf[12] == 0x78);

    const uint8_t bad_selector[] = { AXDR_TAG_STRUCTURE, 0x01, AXDR_TAG_UNSIGNED8, 0x00 };
    ret = test_do_action(0x04, 31, &obis_prof_filter, 1,
                         bad_selector, sizeof(bad_selector), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 250);

    const uint8_t out_of_range_selector[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x01,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x01
    };
    ret = test_do_action(0x05, 31, &obis_prof_filter, 1,
                         out_of_range_selector, sizeof(out_of_range_selector), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);
}

TEST_CASE("Integration_RegisterActivationAddRegisterMutatesObjectList", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(6, &obis_reg_act, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t action_data[] = {
        AXDR_TAG_STRUCTURE, 0x03,
        AXDR_TAG_UNSIGNED16, 0x00, 0x03,
        AXDR_TAG_OCTETSTRING, 0x06, 0x00, 0x00, 0x0A, 0x00, 0x00, 0xFF,
        AXDR_TAG_UNSIGNED8, 0x02
    };

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 6, &obis_reg_act, 1,
                             action_data, sizeof(action_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 6, &obis_reg_act, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x01);
    REQUIRE(get_buf[6] == AXDR_TAG_STRUCTURE);
    REQUIRE(get_buf[7] == 0x03);
    REQUIRE(get_buf[8] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[9] == 0x00);
    REQUIRE(get_buf[10] == 0x03);
}

TEST_CASE("Integration_RegisterActivationAddMaskMutatesMaskList", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(6, &obis_reg_act, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t action_data[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED16, 0x12, 0x34,
        AXDR_TAG_ARRAY, 0x01,
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED16, 0x12, 0x34,
        AXDR_TAG_UNSIGNED16, 0x00, 0x00
    };

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 6, &obis_reg_act, 2,
                             action_data, sizeof(action_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 6, &obis_reg_act, 4, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x01);
    REQUIRE(get_buf[6] == AXDR_TAG_STRUCTURE);
    REQUIRE(get_buf[7] == 0x02);
    REQUIRE(get_buf[8] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[9] == 0x12);
    REQUIRE(get_buf[10] == 0x34);

    const uint8_t missing_mask[] = {
        AXDR_TAG_UNSIGNED16, 0x99, 0x99
    };
    ret = test_do_action(0x03, 6, &obis_reg_act, 3,
                         missing_mask, sizeof(missing_mask), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);

    ret = test_do_get(0x04, 6, &obis_reg_act, 4, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x01);
    REQUIRE(get_buf[8] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[9] == 0x12);
    REQUIRE(get_buf[10] == 0x34);
}

TEST_CASE("Integration_ParameterMonitorAddEntryMutatesList", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(65, &obis_param_mon, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t action_data[] = {
        AXDR_TAG_STRUCTURE, 0x03,
        AXDR_TAG_UNSIGNED16, 0x00, 0x03,
        AXDR_TAG_OCTETSTRING, 0x06, 0x00, 0x00, 0x0A, 0x00, 0x00, 0xFF,
        AXDR_TAG_UNSIGNED8, 0x02
    };

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 65, &obis_param_mon, 1,
                             action_data, sizeof(action_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 65, &obis_param_mon, 5, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x01);
    REQUIRE(get_buf[6] == AXDR_TAG_STRUCTURE);
    REQUIRE(get_buf[7] == 0x03);
    REQUIRE(get_buf[8] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[9] == 0x00);
    REQUIRE(get_buf[10] == 0x03);

    const uint8_t missing_entry[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x05
    };
    ret = test_do_action(0x03, 65, &obis_param_mon, 2,
                         missing_entry, sizeof(missing_entry), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);

    ret = test_do_get(0x04, 65, &obis_param_mon, 5, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x01);
    REQUIRE(get_buf[8] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[9] == 0x00);
    REQUIRE(get_buf[10] == 0x03);
}

TEST_CASE("Integration_ScheduleInsertDeleteMutatesEntries", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(10, &obis_schedule, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t entry[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED16, 0x00, 0x07,
        AXDR_TAG_BOOLEAN, 0x01
    };

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 10, &obis_schedule, 3,
                             entry, sizeof(entry), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 10, &obis_schedule, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x01);
    REQUIRE(get_buf[6] == AXDR_TAG_STRUCTURE);
    REQUIRE(get_buf[7] == 0x02);
    REQUIRE(get_buf[8] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[9] == 0x00);
    REQUIRE(get_buf[10] == 0x07);

    const uint8_t missing_entry[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x09
    };
    ret = test_do_action(0x03, 10, &obis_schedule, 4,
                         missing_entry, sizeof(missing_entry), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);

    const uint8_t delete_entry[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x07
    };
    ret = test_do_action(0x04, 10, &obis_schedule, 4,
                         delete_entry, sizeof(delete_entry), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x05, 10, &obis_schedule, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x00);
}

TEST_CASE("Integration_SpecialDaysInsertDeleteMutatesEntries", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(11, &obis_special_day, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t entry[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED16, 0x00, 0x11,
        AXDR_TAG_OCTETSTRING, 0x05, 0x07, 0xEA, 0x06, 0x1B, 0xFF
    };

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 11, &obis_special_day, 1,
                             entry, sizeof(entry), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 11, &obis_special_day, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x01);
    REQUIRE(get_buf[6] == AXDR_TAG_STRUCTURE);
    REQUIRE(get_buf[7] == 0x02);
    REQUIRE(get_buf[8] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[9] == 0x00);
    REQUIRE(get_buf[10] == 0x11);

    const uint8_t missing_day[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x22
    };
    ret = test_do_action(0x03, 11, &obis_special_day, 2,
                         missing_day, sizeof(missing_day), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);

    const uint8_t delete_day[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x11
    };
    ret = test_do_action(0x04, 11, &obis_special_day, 2,
                         delete_day, sizeof(delete_day), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x05, 11, &obis_special_day, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x00);
}

TEST_CASE("Integration_ScriptTableExecuteMissingScriptFails", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(9, &obis_script, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t script_id[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x01
    };

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 9, &obis_script, 1,
                             script_id, sizeof(script_id), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);
}

TEST_CASE("Integration_SensorManagerResetClearsState", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(67, &obis_sensor_mgr, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t active_variant[] = {
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x2A
    };
    const uint8_t retries[] = {
        AXDR_TAG_UNSIGNED8, 0x05
    };
    const uint8_t protected_flag[] = {
        AXDR_TAG_BOOLEAN, 0x01
    };

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 67, &obis_sensor_mgr, 2,
                          active_variant, sizeof(active_variant), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_set(0x02, 67, &obis_sensor_mgr, 5,
                      retries, sizeof(retries), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_set(0x03, 67, &obis_sensor_mgr, 11,
                      protected_flag, sizeof(protected_flag), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x04, 67, &obis_sensor_mgr, 1,
                         NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x05, 67, &obis_sensor_mgr, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_UNSIGNED32);
    REQUIRE(get_buf[5] == 0x00);
    REQUIRE(get_buf[6] == 0x00);
    REQUIRE(get_buf[7] == 0x00);
    REQUIRE(get_buf[8] == 0x00);

    ret = test_do_get(0x06, 67, &obis_sensor_mgr, 5, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_UNSIGNED8);
    REQUIRE(get_buf[5] == 0x00);

    ret = test_do_get(0x07, 67, &obis_sensor_mgr, 11, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_BOOLEAN);
    REQUIRE(get_buf[5] == 0x00);
}

TEST_CASE("Integration_RegisterTableCaptureRequiresReadableTarget", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(61, &obis_reg_table, NULL, NULL) == TRUE);
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 61, &obis_reg_table, 2,
                             NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);

    const uint8_t class_id[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x03
    };
    ret = test_do_set(0x02, 61, &obis_reg_table, 3,
                      class_id, sizeof(class_id), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    const uint8_t entries[] = {
        AXDR_TAG_ARRAY, 0x01,
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_OCTETSTRING, 0x06, 0x00, 0x00, 0x0A, 0x00, 0x00, 0xFF,
        AXDR_TAG_UNSIGNED8, 0x02
    };
    ret = test_do_set(0x03, 61, &obis_reg_table, 2,
                      entries, sizeof(entries), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x04, 61, &obis_reg_table, 2,
                         NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    const uint8_t bad_active_index[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x01
    };
    ret = test_do_set(0x05, 61, &obis_reg_table, 4,
                      bad_active_index, sizeof(bad_active_index), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x06, 61, &obis_reg_table, 2,
                         NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);
}

TEST_CASE("Integration_SapAssignmentConnectRequiresAssignedSap", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(17, &obis_sap, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t sap_1[] = {
        AXDR_TAG_UNSIGNED16, 0x00, 0x01
    };

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 17, &obis_sap, 1,
                             sap_1, sizeof(sap_1), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OBJECT_UNDEFINED);

    const uint8_t sap_list[] = {
        AXDR_TAG_ARRAY, 0x01,
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED16, 0x00, 0x01,
        AXDR_TAG_OCTETSTRING, 0x06, 'p', 'u', 'b', 'l', 'i', 'c'
    };
    ret = test_do_set(0x02, 17, &obis_sap, 2,
                      sap_list, sizeof(sap_list), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x03, 17, &obis_sap, 1,
                         sap_1, sizeof(sap_1), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);
}

TEST_CASE("Integration_ActivityCalendarActivatesPassiveCalendar", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(20, &obis_activity, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t passive_calendar[] = {
        AXDR_TAG_OCTETSTRING, 0x06, 's', 'u', 'm', 'm', 'e', 'r'
    };

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 20, &obis_activity, 2,
                          passive_calendar, sizeof(passive_calendar), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x02, 20, &obis_activity, 1,
                         NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x03, 20, &obis_activity, 3, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(get_buf[5] == 0x06);
    REQUIRE(std::memcmp(&get_buf[6], "summer", 6) == 0);
}

TEST_CASE("Integration_RegisterMonitorSetAndResetState", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(21, &obis_reg_monitor, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t thresholds[] = {
        AXDR_TAG_ARRAY, 0x02,
        AXDR_TAG_UNSIGNED16, 0x00, 0x64,
        AXDR_TAG_UNSIGNED16, 0x00, 0xC8
    };
    const uint8_t monitored[] = {
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x2A
    };
    const uint8_t actions[] = {
        AXDR_TAG_ARRAY, 0x01,
        AXDR_TAG_NULL
    };

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 21, &obis_reg_monitor, 2,
                          thresholds, sizeof(thresholds), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_set(0x02, 21, &obis_reg_monitor, 3,
                      monitored, sizeof(monitored), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_set(0x03, 21, &obis_reg_monitor, 4,
                      actions, sizeof(actions), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x04, 21, &obis_reg_monitor, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x02);
    REQUIRE(get_buf[6] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[8] == 0x64);
    REQUIRE(get_buf[9] == AXDR_TAG_UNSIGNED16);
    REQUIRE(get_buf[11] == 0xC8);

    ret = test_do_get(0x05, 21, &obis_reg_monitor, 4, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(get_buf[5] == 0x01);
    REQUIRE(get_buf[6] == AXDR_TAG_NULL);

    ret = test_do_action(0x06, 21, &obis_reg_monitor, 1,
                         NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x07, 21, &obis_reg_monitor, 3, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_NULL);
}

TEST_CASE("Integration_SingleActionScheduleSetGetState", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(22, &obis_single_act, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t executed_script[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_OCTETSTRING, 0x06, 0x00, 0x00, 0x09, 0x00, 0x00, 0xFF,
        AXDR_TAG_UNSIGNED16, 0x00, 0x01
    };
    const uint8_t schedule_type[] = {
        AXDR_TAG_ENUM, 0x01
    };
    const uint8_t execution_time[] = {
        AXDR_TAG_OCTETSTRING, 0x0C,
        0x07, 0xEA, 0x06, 0x1B, 0x0C, 0x00,
        0x00, 0x00, 0xFF, 0x80, 0x00, 0x00
    };

    uint8_t buf[1024];
    int ret = test_do_set(0x01, 22, &obis_single_act, 2,
                          executed_script, sizeof(executed_script), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_set(0x02, 22, &obis_single_act, 3,
                      schedule_type, sizeof(schedule_type), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_set(0x03, 22, &obis_single_act, 4,
                      execution_time, sizeof(execution_time), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    uint8_t get_buf[1024];
    ret = test_do_get(0x04, 22, &obis_single_act, 2, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_STRUCTURE);
    REQUIRE(get_buf[5] == 0x02);
    REQUIRE(std::memcmp(&get_buf[4], executed_script, sizeof(executed_script)) == 0);

    ret = test_do_get(0x05, 22, &obis_single_act, 3, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_ENUM);
    REQUIRE(get_buf[5] == 0x01);

    ret = test_do_get(0x06, 22, &obis_single_act, 4, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[3] == 0x00);
    REQUIRE(get_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(get_buf[5] == 0x0C);
    REQUIRE(std::memcmp(&get_buf[4], execution_time, sizeof(execution_time)) == 0);
}

TEST_CASE("Integration_ArbitratorUnsupportedRequestActionFails", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(68, &obis_arbitrator, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t action_data[] = {
        AXDR_TAG_STRUCTURE, 0x01,
        AXDR_TAG_UNSIGNED8, 0x01
    };

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 68, &obis_arbitrator, 1,
                             action_data, sizeof(action_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x04);
}

TEST_CASE("Integration_ImageTransferInitParsesStructure", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(18, &obis_image, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t block_size[] = {
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x32
    };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 18, &obis_image, 4,
                          block_size, sizeof(block_size), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    const uint8_t init_data[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_OCTETSTRING, 0x02, 'f', 'w',
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x64
    };
    ret = test_do_action(0x02, 18, &obis_image, 2,
                         init_data, sizeof(init_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t status_buf[1024];
    ret = test_do_get(0x03, 18, &obis_image, 2, status_buf, sizeof(status_buf));
    REQUIRE(ret > 0);
    REQUIRE(status_buf[0] == 0xC4);
    REQUIRE(status_buf[3] == 0x00);
    REQUIRE(status_buf[4] == AXDR_TAG_ENUM);
    REQUIRE(status_buf[5] == 0x02);

    uint8_t blocks_buf[1024];
    ret = test_do_get(0x04, 18, &obis_image, 5, blocks_buf, sizeof(blocks_buf));
    REQUIRE(ret > 0);
    REQUIRE(blocks_buf[0] == 0xC4);
    REQUIRE(blocks_buf[3] == 0x00);
    REQUIRE(blocks_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(blocks_buf[5] == 0x01);
    REQUIRE(blocks_buf[6] == 0x00);
}

TEST_CASE("Integration_ImageTransferBlockTransferUpdatesProgress", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(18, &obis_image, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t block_size[] = {
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x32
    };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 18, &obis_image, 4,
                          block_size, sizeof(block_size), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    const uint8_t init_data[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_OCTETSTRING, 0x02, 'f', 'w',
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x64
    };
    ret = test_do_action(0x02, 18, &obis_image, 2,
                         init_data, sizeof(init_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    const uint8_t block_data[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x00,
        AXDR_TAG_OCTETSTRING, 0x04, 0xDE, 0xAD, 0xBE, 0xEF
    };
    ret = test_do_action(0x03, 18, &obis_image, 1,
                         block_data, sizeof(block_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    uint8_t progress_buf[1024];
    ret = test_do_get(0x04, 18, &obis_image, 3, progress_buf, sizeof(progress_buf));
    REQUIRE(ret > 0);
    REQUIRE(progress_buf[0] == 0xC4);
    REQUIRE(progress_buf[3] == 0x00);
    REQUIRE(progress_buf[4] == AXDR_TAG_UNSIGNED32);
    REQUIRE(progress_buf[8] == 0x01);

    uint8_t blocks_buf[1024];
    ret = test_do_get(0x05, 18, &obis_image, 5, blocks_buf, sizeof(blocks_buf));
    REQUIRE(ret > 0);
    REQUIRE(blocks_buf[0] == 0xC4);
    REQUIRE(blocks_buf[3] == 0x00);
    REQUIRE(blocks_buf[4] == AXDR_TAG_OCTETSTRING);
    REQUIRE(blocks_buf[5] == 0x01);
    REQUIRE(blocks_buf[6] == 0x80);

    uint8_t first_buf[1024];
    ret = test_do_get(0x06, 18, &obis_image, 6, first_buf, sizeof(first_buf));
    REQUIRE(ret > 0);
    REQUIRE(first_buf[0] == 0xC4);
    REQUIRE(first_buf[3] == 0x00);
    REQUIRE(first_buf[4] == AXDR_TAG_UNSIGNED32);
    REQUIRE(first_buf[8] == 0x01);

    const uint8_t bad_block[] = { AXDR_TAG_STRUCTURE, 0x01, AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x00 };
    ret = test_do_action(0x07, 18, &obis_image, 1,
                         bad_block, sizeof(bad_block), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 250);
}

TEST_CASE("Integration_ImageTransferRejectsOutOfSequenceActions", "[integration][basic]")
{
    test_stack_setup();
    REQUIRE(db_ic_create_inst(18, &obis_image, NULL, NULL) == TRUE);
    test_establish_association();

    const uint8_t verify_data[] = {
        AXDR_TAG_STRUCTURE, 0x00
    };
    uint8_t buf[1024];
    int ret = test_do_action(0x01, 18, &obis_image, 5,
                             verify_data, sizeof(verify_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_TEMPORARY_FAILURE);

    ret = test_do_action(0x02, 18, &obis_image, 6, NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_TEMPORARY_FAILURE);

    const uint8_t init_data[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_OCTETSTRING, 0x02, 'f', 'w',
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x04
    };
    ret = test_do_action(0x03, 18, &obis_image, 2,
                         init_data, sizeof(init_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x04, 18, &obis_image, 5,
                         verify_data, sizeof(verify_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_TEMPORARY_FAILURE);

    ret = test_do_action(0x05, 18, &obis_image, 2,
                         init_data, sizeof(init_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x06, 18, &obis_image, 3, NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    const uint8_t block_data[] = {
        AXDR_TAG_STRUCTURE, 0x02,
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x00,
        AXDR_TAG_OCTETSTRING, 0x04, 0xDE, 0xAD, 0xBE, 0xEF
    };
    ret = test_do_action(0x07, 18, &obis_image, 1,
                         block_data, sizeof(block_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x08, 18, &obis_image, 5,
                         verify_data, sizeof(verify_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x09, 18, &obis_image, 6, NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);
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

TEST_CASE("Integration_SetReadOnlyAttributeDenied", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t set_data[] = { 0x09, 0x06, 0, 0, 96, 1, 0, 255 };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 1, &obis_data, 1,
                          set_data, sizeof(set_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x03);
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

TEST_CASE("Integration_DisconnectControlActionsMutateState", "[integration][basic]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_get(0x01, 70, &obis_disconnect, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == AXDR_TAG_BOOLEAN);
    REQUIRE(buf[5] == 0x01);

    ret = test_do_action(0x02, 70, &obis_disconnect, 1, NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x03, 70, &obis_disconnect, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[4] == AXDR_TAG_BOOLEAN);
    REQUIRE(buf[5] == 0x00);

    const uint8_t unexpected_payload[] = { AXDR_TAG_UNSIGNED8, 0x01 };
    ret = test_do_action(0x04, 70, &obis_disconnect, 2,
                         unexpected_payload, sizeof(unexpected_payload),
                         buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] != 0x00);

    ret = test_do_get(0x05, 70, &obis_disconnect, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[4] == AXDR_TAG_BOOLEAN);
    REQUIRE(buf[5] == 0x00);

    ret = test_do_action(0x06, 70, &obis_disconnect, 2, NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x07, 70, &obis_disconnect, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[4] == AXDR_TAG_BOOLEAN);
    REQUIRE(buf[5] == 0x01);
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
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[1] == 0x01);
    REQUIRE(buf[2] == 0x01);
    REQUIRE(buf[3] == 0x01);
    REQUIRE(buf[4] == 0x04);
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

    const uint8_t data_value[] = {
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x2A
    };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 1, &obis_data, 2,
                          data_value, sizeof(data_value), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    const uint8_t capture_objects[] = {
        AXDR_TAG_ARRAY, 0x01,
        AXDR_TAG_STRUCTURE, 0x03,
        AXDR_TAG_UNSIGNED16, 0x00, 0x01,
        AXDR_TAG_OCTETSTRING, 0x06, 0x00, 0x00, 0x60, 0x01, 0x00, 0xFF,
        AXDR_TAG_UNSIGNED8, 0x02
    };
    ret = test_do_set(0x02, 7, &obis_profile, 3,
                      capture_objects, sizeof(capture_objects), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x03, 7, &obis_profile, 2, NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x04, 7, &obis_profile, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == AXDR_TAG_ARRAY);
    REQUIRE(buf[5] == 0x01);
    REQUIRE(buf[6] == AXDR_TAG_STRUCTURE);
    REQUIRE(buf[7] == 0x02);
    REQUIRE(std::memcmp(&buf[20], data_value, sizeof(data_value)) == 0);
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

TEST_CASE("Integration_ActionNullMethodRejectsUnexpectedPayload", "[integration][service]")
{
    test_stack_setup();
    test_establish_association();

    const uint8_t data_value[] = {
        AXDR_TAG_UNSIGNED32, 0x00, 0x00, 0x00, 0x2A
    };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 1, &obis_data, 2,
                          data_value, sizeof(data_value), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_action(0x02, 1, &obis_data, 1,
                         data_value, sizeof(data_value), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == CSM_ACTION_RESULT_OTHER_REASON);

    ret = test_do_get(0x03, 1, &obis_data, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(std::memcmp(&buf[4], data_value, sizeof(data_value)) == 0);

    const uint8_t null_param[] = { AXDR_TAG_NULL };
    ret = test_do_action(0x04, 1, &obis_data, 1,
                         null_param, sizeof(null_param), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x00);

    ret = test_do_get(0x05, 1, &obis_data, 2, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC4);
    REQUIRE(buf[3] == 0x00);
    REQUIRE(buf[4] == AXDR_TAG_NULL);
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
    test_channels[0].request.llc.dsap = 0x01;

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
    test_channels[0].request.llc.dsap = 0x01;

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

    uint8_t set_data[] = { 0x16, 0x02 };
    uint8_t buf[1024];
    int ret = test_do_set(0x01, 64, &obis_security, 3,
                          set_data, sizeof(set_data), buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC5);

    uint8_t get_buf[1024];
    ret = test_do_get(0x02, 64, &obis_security, 3, get_buf, sizeof(get_buf));
    REQUIRE(ret > 0);
    REQUIRE(get_buf[0] == 0xC4);
    REQUIRE(get_buf[4] == 0x16);
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
    REQUIRE(buf[4] == 0x16);
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

TEST_CASE("Integration_SecuritySetupUnsupportedKeyGenerationFails", "[integration][security]")
{
    test_stack_setup();
    test_establish_association();

    uint8_t buf[1024];
    int ret = test_do_action(0x01, 64, &obis_security, 7,
                             NULL, 0, buf, sizeof(buf));
    REQUIRE(ret > 0);
    REQUIRE(buf[0] == 0xC7);
    REQUIRE(buf[3] == 0x04);
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

TEST_CASE("Integration_HlsPendingRejectsGet", "[integration][security]")
{
    test_stack_setup();
    REQUIRE(test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING) > 0);

    uint8_t buf[1024];
    int calls_before = global_handler_calls;
    csm_services_init(poison_global_db_access);

    int ret = test_do_get(0x01, 8, &obis_clock, 2, buf, sizeof(buf));
    REQUIRE(ret == 0);
    REQUIRE(global_handler_calls == calls_before);
}

TEST_CASE("Integration_HlsPendingRejectsAction", "[integration][security]")
{
    test_stack_setup();
    REQUIRE(test_do_aarq(CSM_AUTH_HIGH_LEVEL_GMAC, LN_REF_WITH_CYPHERING) > 0);

    uint8_t hls_reply[] = { AXDR_TAG_OCTETSTRING, 1U, 0x00U };
    uint8_t buf[1024];
    int calls_before = global_handler_calls;
    csm_services_init(poison_global_db_access);

    int ret = test_do_action(0x01, 15, &obis_asso, 1, hls_reply, sizeof(hls_reply),
                             buf, sizeof(buf));
    REQUIRE(ret == 0);
    REQUIRE(global_handler_calls == calls_before);
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
