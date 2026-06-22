/**
 * DLMS/COSEM Script Table Interface Class handler (class_id = 9)
 *
 * Per Blue Book 4.5.1:
 * - Attr 1: logical_name (octet-string)
 * - Attr 2: scripts (array of structures)
 * - Method 1: execute
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

#define SCRIPT_TABLE_MAX_INSTANCES  4U
#define SCRIPT_TABLE_MAX_SCRIPTS    8U
#define SCRIPT_TABLE_SCRIPT_SIZE    64U

typedef struct {
    uint8_t scripts[SCRIPT_TABLE_MAX_SCRIPTS][SCRIPT_TABLE_SCRIPT_SIZE];
    uint8_t script_count;
} db_ic_script_table_data_t;

static db_ic_script_table_data_t script_table_data_pool[SCRIPT_TABLE_MAX_INSTANCES];
static uint8_t script_table_data_count = 0U;

static db_ic_inst_t script_table_inst_pool[SCRIPT_TABLE_MAX_INSTANCES];
static uint8_t script_table_inst_count = 0U;

static const db_ic_attr_descr script_table_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  2, AXDR_TAG_ARRAY },
};

static const db_ic_method_descr script_table_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
};

static const db_ic_object_descr script_table_descr = {
    .attributes   = script_table_attrs,
    .methods      = script_table_methods,
    .class_id     = 9,
    .obis         = { 0, 0, 9, 0, 0, 0 },
    .attr_count   = 2,
    .method_count = 1,
    .version      = 0
};

static db_ic_inst_t *script_table_create(const csm_obis_code *obis)
{
    (void) obis;

    if (script_table_data_count >= SCRIPT_TABLE_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_script_table_data_t *data = &script_table_data_pool[script_table_data_count];
    data->script_count = 0U;
    script_table_data_count++;

    db_ic_inst_t *inst = &script_table_inst_pool[script_table_inst_count];
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr = &script_table_descr;
    inst->data  = data;
    inst->version = 0U;
    script_table_inst_count++;
    return inst;
}

static csm_db_code script_table_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
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
            db_ic_script_table_data_t *data = (db_ic_script_table_data_t *)inst->data;
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, data->script_count);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            return CSM_OK;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_script_table_class = {
    .class_id  = 9,
    .name      = "Script Table",
    .version   = 0,
    .create    = script_table_create,
    .dispatch  = script_table_dispatch
};

void db_ic_register_script_table(void)
{
    db_ic_register(&ic_script_table_class);
}