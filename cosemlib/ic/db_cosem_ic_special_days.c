/**
 * DLMS/COSEM Special Days Table Interface Class handler (class_id = 11)
 *
 * Per Blue Book 4.5.3:
 * - Attr 1: logical_name (octet-string)
 * - Attr 2: entries (array: day_id, date)
 * - Method 1: insert
 * - Method 2: delete
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

#define SPECIAL_DAYS_MAX_INSTANCES  4U
#define SPECIAL_DAYS_MAX_ENTRIES    16U
#define SPECIAL_DAYS_ENTRY_SIZE     12U

typedef struct {
    uint8_t entries[SPECIAL_DAYS_MAX_ENTRIES][SPECIAL_DAYS_ENTRY_SIZE];
    uint8_t entry_size[SPECIAL_DAYS_MAX_ENTRIES];
    uint16_t day_id[SPECIAL_DAYS_MAX_ENTRIES];
    uint8_t entry_count;
} db_ic_special_days_data_t;

static db_ic_special_days_data_t special_days_data_pool[SPECIAL_DAYS_MAX_INSTANCES];
static uint8_t special_days_data_count = 0U;

static db_ic_inst_t special_days_inst_pool[SPECIAL_DAYS_MAX_INSTANCES];
static uint8_t special_days_inst_count = 0U;

void db_ic_special_days_reset_count(void) { special_days_data_count = 0U; special_days_inst_count = 0U; }

static const db_ic_attr_descr special_days_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  2, AXDR_TAG_ARRAY },
};

static const db_ic_method_descr special_days_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_UNSIGNED16 },
};

static const db_ic_object_descr special_days_descr = {
    .attributes   = special_days_attrs,
    .methods      = special_days_methods,
    .class_id     = 11,
    .obis         = { 0, 0, 11, 0, 0, 0 },
    .attr_count   = 2,
    .method_count = 2,
    .version      = 0
};

static db_ic_inst_t *special_days_create(const csm_obis_code *obis)
{
    (void) obis;

    if (special_days_data_count >= SPECIAL_DAYS_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_special_days_data_t *data = &special_days_data_pool[special_days_data_count];
    memset(data, 0, sizeof(db_ic_special_days_data_t));
    special_days_data_count++;

    db_ic_inst_t *inst = &special_days_inst_pool[special_days_inst_count];
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr = &special_days_descr;
    inst->data  = data;
    inst->version = 0U;
    special_days_inst_count++;
    return inst;
}

static int special_days_parse_day_id(csm_array *in, uint16_t *day_id)
{
    csm_array tmp = *in;
    uint8_t tag = 0U;
    uint8_t fields = 0U;

    if (!csm_array_read_u8(&tmp, &tag) || tag != AXDR_TAG_STRUCTURE)
    {
        return FALSE;
    }
    if (!csm_array_read_u8(&tmp, &fields) || fields == 0U)
    {
        return FALSE;
    }
    if (!csm_array_read_u8(&tmp, &tag) || tag != AXDR_TAG_UNSIGNED16)
    {
        return FALSE;
    }
    return csm_array_read_u16(&tmp, day_id);
}

static csm_db_code special_days_insert_entry(db_ic_special_days_data_t *data, csm_array *in)
{
    uint16_t day_id = 0U;
    uint32_t entry_size = csm_array_unread(in);

    if ((entry_size == 0U) || (entry_size > SPECIAL_DAYS_ENTRY_SIZE) ||
        !special_days_parse_day_id(in, &day_id))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    for (uint8_t i = 0U; i < data->entry_count; i++)
    {
        if (data->day_id[i] == day_id)
        {
            memcpy(data->entries[i], csm_array_rd_data(in), entry_size);
            data->entry_size[i] = (uint8_t)entry_size;
            return csm_array_reader_jump(in, entry_size)
                ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }

    if (data->entry_count >= SPECIAL_DAYS_MAX_ENTRIES)
    {
        return CSM_ERR_DATA_CONTENT_NOT_OK;
    }

    uint8_t idx = data->entry_count++;
    data->day_id[idx] = day_id;
    data->entry_size[idx] = (uint8_t)entry_size;
    memcpy(data->entries[idx], csm_array_rd_data(in), entry_size);
    return csm_array_reader_jump(in, entry_size) ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code special_days_delete_entry(db_ic_special_days_data_t *data, csm_array *in)
{
    uint8_t tag = 0U;
    uint16_t day_id = 0U;

    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16 ||
        !csm_array_read_u16(in, &day_id) || csm_array_unread(in) != 0U)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    for (uint8_t i = 0U; i < data->entry_count; i++)
    {
        if (data->day_id[i] == day_id)
        {
            for (uint8_t j = i; (j + 1U) < data->entry_count; j++)
            {
                data->day_id[j] = data->day_id[j + 1U];
                data->entry_size[j] = data->entry_size[j + 1U];
                memcpy(data->entries[j], data->entries[j + 1U],
                       SPECIAL_DAYS_ENTRY_SIZE);
            }
            data->entry_count--;
            return CSM_OK;
        }
    }

    return CSM_ERR_DATA_CONTENT_NOT_OK;
}

static csm_db_code special_days_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                         uint8_t attr_id, uint8_t method_id,
                                         csm_array *in, csm_array *out)
{
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
            db_ic_special_days_data_t *data = (db_ic_special_days_data_t *)inst->data;
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, data->entry_count);
            for (uint8_t i = 0U; valid && i < data->entry_count; i++)
            {
                valid = csm_array_write_buff(out, data->entries[i], data->entry_size[i]);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        db_ic_special_days_data_t *data = (db_ic_special_days_data_t *)inst->data;
        if (method_id == 1U)
        {
            return special_days_insert_entry(data, in);
        }
        else if (method_id == 2U)
        {
            return special_days_delete_entry(data, in);
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_special_days_class = {
    .class_id  = 11,
    .name      = "Special Days",
    .version   = 0,
    .create    = special_days_create,
    .dispatch  = special_days_dispatch
};

void db_ic_register_special_days(void)
{
    db_ic_register(&ic_special_days_class);
}
