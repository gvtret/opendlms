/**
 * DLMS/COSEM Register Monitor Interface Class handler (class_id = 21)
 *
 * Per Blue Book 4.5.3:
 * - Attr 1: logical_name (octet-string)
 * - Attr 2: thresholds (array)
 * - Attr 3: monitored_value (structure)
 * - Attr 4: actions (array)
 * - Method 1: reset
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 */

#include "db_cosem_ic.h"
#include "csm_config.h"
#include "csm_axdr_codec.h"
#include <string.h>

#define REG_MONITOR_MAX_INSTANCES  4U
#define REG_MONITOR_MAX_THRESHOLDS 8U
#define REG_MONITOR_VALUE_SIZE     16U
#define REG_MONITOR_MAX_ACTIONS    8U
#define REG_MONITOR_ACTION_SIZE    32U

typedef struct {
    uint8_t thresholds[REG_MONITOR_MAX_THRESHOLDS][REG_MONITOR_VALUE_SIZE];
    uint8_t threshold_count;
    uint8_t monitored_value[REG_MONITOR_VALUE_SIZE];
    uint8_t monitored_len;
    uint8_t actions[REG_MONITOR_MAX_ACTIONS][REG_MONITOR_ACTION_SIZE];
    uint8_t action_count;
} db_ic_reg_monitor_data_t;

static db_ic_reg_monitor_data_t reg_monitor_data_pool[REG_MONITOR_MAX_INSTANCES];
static uint8_t reg_monitor_data_count = 0U;

static db_ic_inst_t reg_monitor_inst_pool[REG_MONITOR_MAX_INSTANCES];
static uint8_t reg_monitor_inst_count = 0U;

static const db_ic_attr_descr reg_monitor_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  2, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET,                  3, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  4, AXDR_TAG_ARRAY },
};

static const db_ic_method_descr reg_monitor_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
};

static const db_ic_object_descr reg_monitor_descr = {
    .attributes   = reg_monitor_attrs,
    .methods      = reg_monitor_methods,
    .class_id     = 21,
    .obis         = { 0, 0, 21, 0, 0, 0 },
    .attr_count   = 4,
    .method_count = 1,
    .version      = 0
};

static db_ic_inst_t *reg_monitor_create(const csm_obis_code *obis)
{
    (void) obis;

    if (reg_monitor_data_count >= REG_MONITOR_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_reg_monitor_data_t *data = &reg_monitor_data_pool[reg_monitor_data_count];
    data->threshold_count = 0U;
    data->monitored_value[0] = AXDR_TAG_NULL;
    data->monitored_len = 1U;
    data->action_count = 0U;
    reg_monitor_data_count++;

    db_ic_inst_t *inst = &reg_monitor_inst_pool[reg_monitor_inst_count];
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr = &reg_monitor_descr;
    inst->data  = data;
    inst->version = 0U;
    reg_monitor_inst_count++;
    return inst;
}

static csm_db_code reg_monitor_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                        uint8_t attr_id, uint8_t method_id,
                                        csm_array *in, csm_array *out)
{
    (void) method_id;

    if ((inst == NULL) || (inst->data == NULL))
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

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
            db_ic_reg_monitor_data_t *data = (db_ic_reg_monitor_data_t *)inst->data;
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, data->threshold_count);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            db_ic_reg_monitor_data_t *data = (db_ic_reg_monitor_data_t *)inst->data;
            int valid = csm_array_write_buff(out, data->monitored_value, data->monitored_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            db_ic_reg_monitor_data_t *data = (db_ic_reg_monitor_data_t *)inst->data;
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, data->action_count);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            db_ic_reg_monitor_data_t *data = (db_ic_reg_monitor_data_t *)inst->data;
            data->monitored_value[0] = AXDR_TAG_NULL;
            data->monitored_len = 1U;
            return CSM_OK;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_reg_monitor_class = {
    .class_id  = 21,
    .name      = "Register Monitor",
    .version   = 0,
    .create    = reg_monitor_create,
    .dispatch  = reg_monitor_dispatch
};

void db_ic_register_register_monitor(void)
{
    db_ic_register(&ic_reg_monitor_class);
}