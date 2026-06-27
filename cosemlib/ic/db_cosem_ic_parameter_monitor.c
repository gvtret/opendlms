/**
 * DLMS/COSEM Parameter Monitor Interface Class handler (class_id = 65)
 *
 * Per Blue Book 4.5.7:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: current_page (Unsigned32, dynamic)
 * - Attr 3: page_size (Unsigned32, static)
 * - Attr 4: number_of_pages (Unsigned32, dynamic)
 * - Attr 5: monitor_parameter_list (array of structures, static)
 * - Method 1: add_entry (structure)
 * - Method 2: delete_entry (Unsigned16)
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

#define PARAM_MON_MAX_INSTANCES  4U
#define PARAM_MON_MAX_ENTRIES    8U

typedef struct {
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t attribute_id;
    uint8_t data[32];
    uint8_t data_len;
} db_ic_param_mon_entry;

typedef struct {
    uint32_t current_page;
    uint32_t page_size;
    db_ic_param_mon_entry entries[PARAM_MON_MAX_ENTRIES];
    uint8_t entry_count;
} db_ic_param_mon_data;

static db_ic_param_mon_data param_mon_pool[PARAM_MON_MAX_INSTANCES];
static uint8_t param_mon_pool_count = 0U;

static db_ic_inst_t param_mon_inst_tmp;

void db_ic_parameter_monitor_reset_count(void) { param_mon_pool_count = 0U; }

static const db_ic_attr_descr param_mon_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET,                  4, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET,                  5, AXDR_TAG_ARRAY },
};

static const db_ic_method_descr param_mon_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_UNSIGNED16 },
};

static const db_ic_object_descr param_mon_descr = {
    .attributes   = param_mon_attrs,
    .methods      = param_mon_methods,
    .class_id     = 65,
    .obis         = { 0, 0, 60, 6, 0, 255 },
    .attr_count   = 5,
    .method_count = 2,
    .version      = 0
};

static db_ic_inst_t *param_mon_create(const csm_obis_code *obis)
{
    (void) obis;
    if (param_mon_pool_count >= PARAM_MON_MAX_INSTANCES) { return NULL; }

    db_ic_param_mon_data *d = &param_mon_pool[param_mon_pool_count];
    memset(d, 0, sizeof(db_ic_param_mon_data));
    d->page_size = 1U;
    param_mon_pool_count++;

    memset(&param_mon_inst_tmp, 0, sizeof(db_ic_inst_t));
    param_mon_inst_tmp.descr   = &param_mon_descr;
    param_mon_inst_tmp.data    = d;
    param_mon_inst_tmp.version = 0U;
    return &param_mon_inst_tmp;
}

static int param_mon_read_entry(csm_array *in, db_ic_param_mon_entry *entry)
{
    uint8_t tag = 0xFFU;
    uint8_t fields = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) { return FALSE; }
    if (!csm_array_read_u8(in, &fields) || fields != 3U) { return FALSE; }

    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return FALSE; }
    if (!csm_array_read_u16(in, &entry->class_id)) { return FALSE; }

    uint8_t obis_len = 0U;
    uint8_t obis[6];
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) { return FALSE; }
    if (!csm_array_read_u8(in, &obis_len) || obis_len != sizeof(obis)) { return FALSE; }
    if (!csm_array_read_buff(in, obis, sizeof(obis))) { return FALSE; }
    entry->obis.A = obis[0];
    entry->obis.B = obis[1];
    entry->obis.C = obis[2];
    entry->obis.D = obis[3];
    entry->obis.E = obis[4];
    entry->obis.F = obis[5];

    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED8) { return FALSE; }
    if (!csm_array_read_u8(in, &entry->attribute_id)) { return FALSE; }
    entry->data_len = 0U;
    return TRUE;
}

static csm_db_code param_mon_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                        uint8_t attr_id, uint8_t method_id,
                                        csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_param_mon_data *d = (db_ic_param_mon_data *)inst->data;

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
            valid = valid && csm_array_write_u32(out, d->current_page);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->page_size);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, (uint32_t)d->entry_count);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 5U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, d->entry_count);
            for (uint8_t i = 0U; i < d->entry_count && valid; i++)
            {
                valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
                valid = valid && csm_array_write_u8(out, 3U);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
                valid = valid && csm_array_write_u16(out, d->entries[i].class_id);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
                valid = valid && csm_array_write_u8(out, 6U);
                valid = valid && csm_array_write_u8(out, d->entries[i].obis.A);
                valid = valid && csm_array_write_u8(out, d->entries[i].obis.B);
                valid = valid && csm_array_write_u8(out, d->entries[i].obis.C);
                valid = valid && csm_array_write_u8(out, d->entries[i].obis.D);
                valid = valid && csm_array_write_u8(out, d->entries[i].obis.E);
                valid = valid && csm_array_write_u8(out, d->entries[i].obis.F);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
                valid = valid && csm_array_write_u8(out, d->entries[i].attribute_id);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u32(in, &d->current_page)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 3U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u32(in, &d->page_size)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            if (d->entry_count >= PARAM_MON_MAX_ENTRIES) { return CSM_ERR_DATA_CONTENT_NOT_OK; }
            if (!param_mon_read_entry(in, &d->entries[d->entry_count])) { return CSM_ERR_BAD_ENCODING; }
            d->entry_count++;
            return CSM_OK;
        }
        else if (method_id == 2U)
        {
            /* delete_entry: read Unsigned16 index */
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return CSM_ERR_BAD_ENCODING; }
            uint16_t idx = 0U;
            if (!csm_array_read_u16(in, &idx)) { return CSM_ERR_BAD_ENCODING; }
            if (idx < d->entry_count)
            {
                for (uint8_t i = (uint8_t)idx; i < d->entry_count - 1U; i++)
                {
                    d->entries[i] = d->entries[i + 1U];
                }
                d->entry_count--;
                return CSM_OK;
            }
            return CSM_ERR_DATA_CONTENT_NOT_OK;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_param_mon = {
    .class_id  = 65,
    .name      = "Parameter Monitor",
    .version   = 0,
    .create    = param_mon_create,
    .dispatch  = param_mon_dispatch
};

void db_ic_register_parameter_monitor(void)
{
    db_ic_register(&ic_param_mon);
}
