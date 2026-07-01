/**
 * DLMS/COSEM Activity Calendar Interface Class handler (class_id = 20)
 *
 * Per Blue Book 4.5.2:
 * - Attr 1: logical_name (octet-string)
 * - Attr 2: passive_calendar (octet-string)
 * - Attr 3: active_calendar (octet-string)
 * - Attrs 4-10: day_schedules
 * - Method 1: activate_passive_calendar
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

#define ACTIVITY_CAL_MAX_INSTANCES  4U
#define ACTIVITY_CAL_NAME_LEN       32U

typedef struct {
    uint8_t passive_calendar[ACTIVITY_CAL_NAME_LEN];
    uint8_t passive_len;
    uint8_t active_calendar[ACTIVITY_CAL_NAME_LEN];
    uint8_t active_len;
} db_ic_activity_cal_data_t;

static db_ic_activity_cal_data_t activity_cal_data_pool[ACTIVITY_CAL_MAX_INSTANCES];
static uint8_t activity_cal_data_count = 0U;

static db_ic_inst_t activity_cal_inst_pool[ACTIVITY_CAL_MAX_INSTANCES];
static uint8_t activity_cal_inst_count = 0U;

void db_ic_activity_cal_reset_count(void) { activity_cal_data_count = 0U; activity_cal_inst_count = 0U; }

static const db_ic_attr_descr activity_cal_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  3, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  4, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  5, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  6, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  7, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  8, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  9, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                 10, AXDR_TAG_STRUCTURE },
};

static const db_ic_method_descr activity_cal_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
};

static const db_ic_object_descr activity_cal_descr = {
    .attributes   = activity_cal_attrs,
    .methods      = activity_cal_methods,
    .class_id     = 20,
    .obis         = { 0, 0, 20, 0, 0, 0 },
    .attr_count   = 10,
    .method_count = 1,
    .version      = 0
};

static db_ic_inst_t *activity_cal_create(const csm_obis_code *obis)
{
    (void) obis;

    if (activity_cal_data_count >= ACTIVITY_CAL_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_activity_cal_data_t *data = &activity_cal_data_pool[activity_cal_data_count];
    data->passive_calendar[0] = AXDR_TAG_NULL;
    data->passive_len = 1U;
    data->active_calendar[0] = AXDR_TAG_NULL;
    data->active_len = 1U;
    activity_cal_data_count++;

    db_ic_inst_t *inst = &activity_cal_inst_pool[activity_cal_inst_count];
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr = &activity_cal_descr;
    inst->data  = data;
    inst->version = 0U;
    activity_cal_inst_count++;
    return inst;
}

static csm_db_code activity_cal_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
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
            db_ic_activity_cal_data_t *data = (db_ic_activity_cal_data_t *)inst->data;
            int valid = csm_array_write_buff(out, data->passive_calendar, data->passive_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            db_ic_activity_cal_data_t *data = (db_ic_activity_cal_data_t *)inst->data;
            int valid = csm_array_write_buff(out, data->active_calendar, data->active_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            db_ic_activity_cal_data_t *data = (db_ic_activity_cal_data_t *)inst->data;
            uint8_t tag = 0xFFU;
            uint8_t len = 0U;
            uint8_t calendar[ACTIVITY_CAL_NAME_LEN];

            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u8(in, &len) || len > (ACTIVITY_CAL_NAME_LEN - 2U))
            {
                return CSM_ERR_BAD_ENCODING;
            }

            calendar[0] = AXDR_TAG_OCTETSTRING;
            calendar[1] = len;
            if (len > 0U &&
                !csm_array_read_buff(in, &calendar[2], len))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (csm_array_unread(in) != 0U)
            {
                return CSM_ERR_BAD_ENCODING;
            }

            memcpy(data->passive_calendar, calendar, (uint8_t)(2U + len));
            data->passive_len = (uint8_t)(2U + len);
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            db_ic_activity_cal_data_t *data = (db_ic_activity_cal_data_t *)inst->data;
            memcpy(data->active_calendar, data->passive_calendar, data->passive_len);
            data->active_len = data->passive_len;
            return CSM_OK;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_activity_cal_class = {
    .class_id  = 20,
    .name      = "Activity Calendar",
    .version   = 0,
    .create    = activity_cal_create,
    .dispatch  = activity_cal_dispatch
};

void db_ic_register_activity_calendar(void)
{
    db_ic_register(&ic_activity_cal_class);
}
