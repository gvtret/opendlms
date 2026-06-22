/**
 * DLMS/COSEM Arbitrator Interface Class handler (class_id = 68)
 *
 * Per Blue Book 4.5.10:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: target_device_address (Unsigned32, static)
 * - Attr 3: power_limit (structure, static)
 * - Attr 4: current_phase_list (octet-string, static)
 * - Attr 5: group1_limiter (structure, static)
 * - Attr 6: group2_limiter (structure, static)
 * - Method 1: request_action (structure)
 * - Method 2: reset (null)
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "db_cosem_ic.h"
#include "csm_config.h"
#include "csm_axdr_codec.h"
#include <string.h>

#define ARB_MAX_INSTANCES   4U
#define ARB_PHASE_MAX       16U
#define ARB_LIMITER_BUF     32U

typedef struct {
    uint32_t target_device_address;
    uint8_t  power_limit_buf[ARB_LIMITER_BUF];
    uint8_t  power_limit_len;
    uint8_t  current_phase_list[ARB_PHASE_MAX];
    uint8_t  phase_list_len;
    uint8_t  group1_limiter_buf[ARB_LIMITER_BUF];
    uint8_t  group1_limiter_len;
    uint8_t  group2_limiter_buf[ARB_LIMITER_BUF];
    uint8_t  group2_limiter_len;
} db_ic_arb_data;

static db_ic_arb_data arb_pool[ARB_MAX_INSTANCES];
static uint8_t arb_pool_count = 0U;

static db_ic_inst_t arb_inst_tmp;

static const db_ic_attr_descr arb_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  5, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET | DB_ACCESS_SET,  6, AXDR_TAG_STRUCTURE },
};

static const db_ic_method_descr arb_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_NULL },
};

static const db_ic_object_descr arb_descr = {
    .attributes   = arb_attrs,
    .methods      = arb_methods,
    .class_id     = 68,
    .obis         = { 0, 0, 60, 7, 0, 255 },
    .attr_count   = 6,
    .method_count = 2,
    .version      = 0
};

static db_ic_inst_t *arb_create(const csm_obis_code *obis)
{
    (void) obis;
    if (arb_pool_count >= ARB_MAX_INSTANCES) { return NULL; }

    db_ic_arb_data *d = &arb_pool[arb_pool_count];
    memset(d, 0, sizeof(db_ic_arb_data));
    arb_pool_count++;

    memset(&arb_inst_tmp, 0, sizeof(db_ic_inst_t));
    arb_inst_tmp.descr   = &arb_descr;
    arb_inst_tmp.data    = d;
    arb_inst_tmp.version = 0U;
    return &arb_inst_tmp;
}

static int arb_read_structure_raw(csm_array *in, uint8_t *buf, uint8_t max_len, uint8_t *out_len)
{
    uint8_t tag = 0xFFU;
    uint8_t flds = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) { return FALSE; }
    if (!csm_array_read_u8(in, &flds)) { return FALSE; }
    buf[0] = tag;
    buf[1] = flds;
    uint16_t pos = 2U;
    for (uint8_t f = 0U; f < flds; f++)
    {
        uint8_t ftag = 0xFFU;
        if (!csm_array_read_u8(in, &ftag)) { return FALSE; }
        if (pos >= max_len) { return FALSE; }
        buf[pos++] = ftag;
        if (ftag == AXDR_TAG_NULL) { continue; }
        if (ftag == AXDR_TAG_BOOLEAN || ftag == AXDR_TAG_UNSIGNED8 || ftag == AXDR_TAG_INTEGER8 || ftag == AXDR_TAG_ENUM)
        {
            if (pos >= max_len) { return FALSE; }
            if (!csm_array_read_u8(in, &buf[pos++])) { return FALSE; }
        }
        else if (ftag == AXDR_TAG_UNSIGNED16 || ftag == AXDR_TAG_INTEGER16)
        {
            if ((pos + 2U) > max_len) { return FALSE; }
            uint16_t v;
            if (!csm_array_read_u16(in, &v)) { return FALSE; }
            buf[pos++] = (uint8_t)(v & 0xFFU);
            buf[pos++] = (uint8_t)((v >> 8) & 0xFFU);
        }
        else if (ftag == AXDR_TAG_UNSIGNED32 || ftag == AXDR_TAG_INTEGER32)
        {
            if ((pos + 4U) > max_len) { return FALSE; }
            uint32_t v;
            if (!csm_array_read_u32(in, &v)) { return FALSE; }
            buf[pos++] = (uint8_t)(v & 0xFFU);
            buf[pos++] = (uint8_t)((v >> 8) & 0xFFU);
            buf[pos++] = (uint8_t)((v >> 16) & 0xFFU);
            buf[pos++] = (uint8_t)((v >> 24) & 0xFFU);
        }
        else
        {
            return FALSE;
        }
    }
    *out_len = (uint8_t)pos;
    return TRUE;
}

static csm_db_code arb_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                  uint8_t attr_id, uint8_t method_id,
                                  csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_arb_data *d = (db_ic_arb_data *)inst->data;

    if (op == IC_OP_GET)
    {
        if (attr_id == 1U)
        {
            const csm_obis_code *obis = inst->has_obis
                ? &inst->obis : &inst->descr->obis;
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, 6U);
            valid = valid && csm_array_write_u8(out, obis->A);
            valid = valid && csm_array_write_u8(out, obis->B);
            valid = valid && csm_array_write_u8(out, obis->C);
            valid = valid && csm_array_write_u8(out, obis->D);
            valid = valid && csm_array_write_u8(out, obis->E);
            valid = valid && csm_array_write_u8(out, obis->F);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 2U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->target_device_address);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_buff(out, d->power_limit_buf, d->power_limit_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, d->phase_list_len);
            if (d->phase_list_len > 0U)
            {
                valid = valid && csm_array_write_buff(out, d->current_phase_list, d->phase_list_len);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 5U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_buff(out, d->group1_limiter_buf, d->group1_limiter_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 6U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_buff(out, d->group2_limiter_buf, d->group2_limiter_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u32(in, &d->target_device_address)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 3U)
        {
            uint8_t len = 0U;
            if (!arb_read_structure_raw(in, d->power_limit_buf, ARB_LIMITER_BUF, &len)) { return CSM_ERR_BAD_ENCODING; }
            d->power_limit_len = len;
            return CSM_OK;
        }
        else if (attr_id == 4U)
        {
            uint8_t tag = 0xFFU;
            uint8_t len = 0U;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &len) || len > ARB_PHASE_MAX) { return CSM_ERR_BAD_ENCODING; }
            d->phase_list_len = len;
            if (len > 0U)
            {
                if (!csm_array_read_buff(in, d->current_phase_list, len)) { return CSM_ERR_BAD_ENCODING; }
            }
            return CSM_OK;
        }
        else if (attr_id == 5U)
        {
            uint8_t len = 0U;
            if (!arb_read_structure_raw(in, d->group1_limiter_buf, ARB_LIMITER_BUF, &len)) { return CSM_ERR_BAD_ENCODING; }
            d->group1_limiter_len = len;
            return CSM_OK;
        }
        else if (attr_id == 6U)
        {
            uint8_t len = 0U;
            if (!arb_read_structure_raw(in, d->group2_limiter_buf, ARB_LIMITER_BUF, &len)) { return CSM_ERR_BAD_ENCODING; }
            d->group2_limiter_len = len;
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            /* request_action: skip structure input */
            return CSM_OK;
        }
        else if (method_id == 2U)
        {
            /* reset */
            memset(d->current_phase_list, 0, ARB_PHASE_MAX);
            d->phase_list_len = 0U;
            return CSM_OK;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_arb = {
    .class_id  = 68,
    .name      = "Arbitrator",
    .version   = 0,
    .create    = arb_create,
    .dispatch  = arb_dispatch
};

void db_ic_register_arbitrator(void)
{
    db_ic_register(&ic_arb);
}
