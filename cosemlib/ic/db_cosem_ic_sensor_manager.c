/**
 * DLMS/COSEM Sensor Manager Interface Class handler (class_id = 67)
 *
 * Per Blue Book 4.5.9:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: active_variant (Unsigned32, static)
 * - Attr 3: active_schedule (Unsigned32, static)
 * - Attr 4: communication_window (structure, static)
 * - Attr 5: number_of_retries (Unsigned8, static)
 * - Attr 6: retry_delay_time (Unsigned16, static)
 * - Attr 7: repetition_delay (structure, static)
 * - Attr 8: sensor_objects (array of structures, static)
 * - Attr 9: schedule_objects (array of structures, static)
 * - Attr 10: variant_list (array of structures, static)
 * - Attr 11: data_protected (Boolean, static)
 * - Attr 12: data_protection_key (Octet-string, static)
 * - Attr 13: data_protection_password (Octet-string, static)
 * - Attr 14: data_protection_status (Enum, static)
 * - Attr 15: data_protection_error_code (Enum, static)
 * - Method 1: reset (null)
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

#define SENS_MGR_MAX_INSTANCES   4U
#define SENS_MGR_MAX_OBJECTS     8U
#define SENS_MGR_BUF_MAX         64U

typedef struct {
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t attribute_id;
} db_ic_sens_obj;

typedef struct {
    uint32_t active_variant;
    uint32_t active_schedule;
    uint8_t  comm_window_buf[SENS_MGR_BUF_MAX];
    uint8_t  comm_window_len;
    uint8_t  number_of_retries;
    uint16_t retry_delay_time;
    uint8_t  repetition_delay_buf[16];
    uint8_t  repetition_delay_len;
    db_ic_sens_obj sensor_objects[SENS_MGR_MAX_OBJECTS];
    uint8_t  sensor_object_count;
    db_ic_sens_obj schedule_objects[SENS_MGR_MAX_OBJECTS];
    uint8_t  schedule_object_count;
    uint8_t  variant_list_buf[SENS_MGR_BUF_MAX];
    uint8_t  variant_list_len;
    uint8_t  data_protected;
    uint8_t  data_protection_key[16];
    uint8_t  key_len;
    uint8_t  data_protection_password[16];
    uint8_t  password_len;
    uint8_t  data_protection_status;
    uint8_t  data_protection_error_code;
} db_ic_sens_mgr_data;

static db_ic_sens_mgr_data sens_mgr_pool[SENS_MGR_MAX_INSTANCES];
static uint8_t sens_mgr_pool_count = 0U;

static db_ic_inst_t sens_mgr_inst_tmp;

void db_ic_sensor_manager_reset_count(void) { sens_mgr_pool_count = 0U; }

static const db_ic_attr_descr sens_mgr_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET | DB_ACCESS_SET,  5, AXDR_TAG_UNSIGNED8 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  6, AXDR_TAG_UNSIGNED16 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  7, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  8, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET,                  9, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET,                 10, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET | DB_ACCESS_SET, 11, AXDR_TAG_BOOLEAN },
    { DB_ACCESS_GET | DB_ACCESS_SET, 12, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET, 13, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                 14, AXDR_TAG_ENUM },
    { DB_ACCESS_GET,                 15, AXDR_TAG_ENUM },
};

static const db_ic_method_descr sens_mgr_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
};

static const db_ic_object_descr sens_mgr_descr = {
    .attributes   = sens_mgr_attrs,
    .methods      = sens_mgr_methods,
    .class_id     = 67,
    .obis         = { 0, 0, 60, 8, 0, 255 },
    .attr_count   = 15,
    .method_count = 1,
    .version      = 0
};

static db_ic_inst_t *sens_mgr_create(const csm_obis_code *obis)
{
    (void) obis;
    if (sens_mgr_pool_count >= SENS_MGR_MAX_INSTANCES) { return NULL; }

    db_ic_sens_mgr_data *d = &sens_mgr_pool[sens_mgr_pool_count];
    memset(d, 0, sizeof(db_ic_sens_mgr_data));
    sens_mgr_pool_count++;

    memset(&sens_mgr_inst_tmp, 0, sizeof(db_ic_inst_t));
    sens_mgr_inst_tmp.descr   = &sens_mgr_descr;
    sens_mgr_inst_tmp.data    = d;
    sens_mgr_inst_tmp.version = 0U;
    return &sens_mgr_inst_tmp;
}

static csm_db_code sens_mgr_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                       uint8_t attr_id, uint8_t method_id,
                                       csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_sens_mgr_data *d = (db_ic_sens_mgr_data *)inst->data;

    if (op == IC_OP_GET)
    {
        switch (attr_id)
        {
        case 1U:
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
        case 2U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->active_variant);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 3U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->active_schedule);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 4U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_buff(out, d->comm_window_buf, d->comm_window_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 5U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
            valid = valid && csm_array_write_u8(out, d->number_of_retries);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 6U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
            valid = valid && csm_array_write_u16(out, d->retry_delay_time);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 7U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_buff(out, d->repetition_delay_buf, d->repetition_delay_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 8U:
        case 9U:
        {
            const db_ic_sens_obj *objs = (attr_id == 8U) ? d->sensor_objects : d->schedule_objects;
            uint8_t count = (attr_id == 8U) ? d->sensor_object_count : d->schedule_object_count;
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, count);
            for (uint8_t i = 0U; i < count && valid; i++)
            {
                valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
                valid = valid && csm_array_write_u8(out, 3U);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
                valid = valid && csm_array_write_u16(out, objs[i].class_id);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
                valid = valid && csm_array_write_u8(out, 6U);
                valid = valid && csm_array_write_u8(out, objs[i].obis.A);
                valid = valid && csm_array_write_u8(out, objs[i].obis.B);
                valid = valid && csm_array_write_u8(out, objs[i].obis.C);
                valid = valid && csm_array_write_u8(out, objs[i].obis.D);
                valid = valid && csm_array_write_u8(out, objs[i].obis.E);
                valid = valid && csm_array_write_u8(out, objs[i].obis.F);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
                valid = valid && csm_array_write_u8(out, objs[i].attribute_id);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 10U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, 0U);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 11U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_BOOLEAN);
            valid = valid && csm_array_write_u8(out, d->data_protected);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 12U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, d->key_len);
            if (d->key_len > 0U) { valid = valid && csm_array_write_buff(out, d->data_protection_key, d->key_len); }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 13U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, d->password_len);
            if (d->password_len > 0U) { valid = valid && csm_array_write_buff(out, d->data_protection_password, d->password_len); }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 14U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
            valid = valid && csm_array_write_u8(out, d->data_protection_status);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        case 15U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
            valid = valid && csm_array_write_u8(out, d->data_protection_error_code);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        default:
            break;
        }
    }
    else if (op == IC_OP_SET)
    {
        switch (attr_id)
        {
        case 2U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u32(in, &d->active_variant)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        case 3U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u32(in, &d->active_schedule)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        case 5U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED8) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &d->number_of_retries)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        case 6U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u16(in, &d->retry_delay_time)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        case 11U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_BOOLEAN) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &d->data_protected)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        default:
            break;
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

static const db_ic_class ic_sens_mgr = {
    .class_id  = 67,
    .name      = "Sensor Manager",
    .version   = 0,
    .create    = sens_mgr_create,
    .dispatch  = sens_mgr_dispatch
};

void db_ic_register_sensor_manager(void)
{
    db_ic_register(&ic_sens_mgr);
}
