/**
 * DLMS/COSEM Single Action Schedule Interface Class handler (class_id = 22)
 *
 * Per Blue Book 4.5.4:
 * - Attr 1: logical_name (octet-string)
 * - Attr 2: executed_script (structure)
 * - Attr 3: type (enum)
 * - Attr 4: execution_time
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

#define SINGLE_ACTION_MAX_INSTANCES  4U
#define SINGLE_ACTION_SCRIPT_SIZE   16U
#define SINGLE_ACTION_TIME_SIZE     12U

typedef struct {
    uint8_t executed_script[SINGLE_ACTION_SCRIPT_SIZE];
    uint8_t script_len;
    uint8_t type;
    uint8_t execution_time[SINGLE_ACTION_TIME_SIZE];
    uint8_t time_len;
} db_ic_single_action_data_t;

static db_ic_single_action_data_t single_action_data_pool[SINGLE_ACTION_MAX_INSTANCES];
static uint8_t single_action_data_count = 0U;

static db_ic_inst_t single_action_inst_pool[SINGLE_ACTION_MAX_INSTANCES];
static uint8_t single_action_inst_count = 0U;

static const db_ic_attr_descr single_action_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  2, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  3, AXDR_TAG_ENUM },
    { DB_ACCESS_GET,                  4, AXDR_TAG_STRUCTURE },
};

static const db_ic_method_descr single_action_methods[] = {
    { 0, 0, 0 },
};

static const db_ic_object_descr single_action_descr = {
    .attributes   = single_action_attrs,
    .methods      = single_action_methods,
    .class_id     = 22,
    .obis         = { 0, 0, 22, 0, 0, 0 },
    .attr_count   = 4,
    .method_count = 0,
    .version      = 0
};

static db_ic_inst_t *single_action_create(const csm_obis_code *obis)
{
    (void) obis;

    if (single_action_data_count >= SINGLE_ACTION_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_single_action_data_t *data = &single_action_data_pool[single_action_data_count];
    data->executed_script[0] = AXDR_TAG_NULL;
    data->script_len = 1U;
    data->type = 0U;
    data->execution_time[0] = AXDR_TAG_NULL;
    data->time_len = 1U;
    single_action_data_count++;

    db_ic_inst_t *inst = &single_action_inst_pool[single_action_inst_count];
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr = &single_action_descr;
    inst->data  = data;
    inst->version = 0U;
    single_action_inst_count++;
    return inst;
}

static csm_db_code single_action_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
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
            db_ic_single_action_data_t *data = (db_ic_single_action_data_t *)inst->data;
            int valid = csm_array_write_buff(out, data->executed_script, data->script_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            db_ic_single_action_data_t *data = (db_ic_single_action_data_t *)inst->data;
            int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
            valid = valid && csm_array_write_u8(out, data->type);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            db_ic_single_action_data_t *data = (db_ic_single_action_data_t *)inst->data;
            int valid = csm_array_write_buff(out, data->execution_time, data->time_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_single_action_class = {
    .class_id  = 22,
    .name      = "Single Action Schedule",
    .version   = 0,
    .create    = single_action_create,
    .dispatch  = single_action_dispatch
};

void db_ic_register_single_action_schedule(void)
{
    db_ic_register(&ic_single_action_class);
}