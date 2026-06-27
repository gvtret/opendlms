/**
 * DLMS/COSEM Register Table Interface Class handler (class_id = 61)
 *
 * Per Blue Book 4.5.4:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: register_activations (array of structures, static)
 * - Attr 3: register_class_id (Unsigned16, static)
 * - Attr 4: register_active_index (Unsigned16, static)
 * - Method 1: reset (null)
 * - Method 2: capture (null)
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

#define REG_TABLE_MAX_INSTANCES  4U
#define REG_TABLE_MAX_ENTRIES    8U

typedef struct {
    csm_obis_code obis;
    uint8_t attribute_id;
} db_ic_reg_table_entry;

typedef struct {
    db_ic_reg_table_entry entries[REG_TABLE_MAX_ENTRIES];
    uint8_t entry_count;
    uint16_t class_id;
    uint16_t active_index;
} db_ic_reg_table_data;

static db_ic_reg_table_data reg_table_pool[REG_TABLE_MAX_INSTANCES];
static uint8_t reg_table_pool_count = 0U;

static db_ic_inst_t reg_table_inst_tmp;

void db_ic_register_table_reset_count(void) { reg_table_pool_count = 0U; }

static const db_ic_attr_descr reg_table_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_UNSIGNED16 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_UNSIGNED16 },
};

static const db_ic_method_descr reg_table_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_NULL },
};

static const db_ic_object_descr reg_table_descr = {
    .attributes   = reg_table_attrs,
    .methods      = reg_table_methods,
    .class_id     = 61,
    .obis         = { 0, 0, 60, 2, 0, 255 },
    .attr_count   = 4,
    .method_count = 2,
    .version      = 0
};

static db_ic_inst_t *reg_table_create(const csm_obis_code *obis)
{
    (void) obis;
    if (reg_table_pool_count >= REG_TABLE_MAX_INSTANCES) { return NULL; }

    db_ic_reg_table_data *d = &reg_table_pool[reg_table_pool_count];
    memset(d, 0, sizeof(db_ic_reg_table_data));
    reg_table_pool_count++;

    memset(&reg_table_inst_tmp, 0, sizeof(db_ic_inst_t));
    reg_table_inst_tmp.descr   = &reg_table_descr;
    reg_table_inst_tmp.data    = d;
    reg_table_inst_tmp.version = 0U;
    return &reg_table_inst_tmp;
}

static csm_db_code reg_table_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                        uint8_t attr_id, uint8_t method_id,
                                        csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_reg_table_data *d = (db_ic_reg_table_data *)inst->data;

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
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, d->entry_count);
            for (uint8_t i = 0U; i < d->entry_count && valid; i++)
            {
                valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
                valid = valid && csm_array_write_u8(out, 2U);
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
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
            valid = valid && csm_array_write_u16(out, d->class_id);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
            valid = valid && csm_array_write_u16(out, d->active_index);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t tag = 0xFFU;
            uint8_t count = 0U;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &count) || count > REG_TABLE_MAX_ENTRIES) { return CSM_ERR_BAD_ENCODING; }
            d->entry_count = count;
            for (uint8_t i = 0U; i < count; i++)
            {
                uint8_t stag = 0xFFU;
                uint8_t flds = 0U;
                if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &flds) || flds < 2U) { return CSM_ERR_BAD_ENCODING; }

                uint8_t otag = 0xFFU;
                uint8_t olen = 0U;
                if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &olen) || olen != 6U) { return CSM_ERR_BAD_ENCODING; }
                {
                    uint8_t ob[6];
                    if (!csm_array_read_buff(in, ob, 6U)) { return CSM_ERR_BAD_ENCODING; }
                    d->entries[i].obis.A = ob[0];
                    d->entries[i].obis.B = ob[1];
                    d->entries[i].obis.C = ob[2];
                    d->entries[i].obis.D = ob[3];
                    d->entries[i].obis.E = ob[4];
                    d->entries[i].obis.F = ob[5];
                }

                uint8_t atag = 0xFFU;
                if (!csm_array_read_u8(in, &atag) || atag != AXDR_TAG_UNSIGNED8) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &d->entries[i].attribute_id)) { return CSM_ERR_BAD_ENCODING; }
            }
            return CSM_OK;
        }
        else if (attr_id == 3U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u16(in, &d->class_id)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 4U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u16(in, &d->active_index)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            /* reset */
            d->entry_count = 0U;
            d->active_index = 0U;
            return CSM_OK;
        }
        else if (method_id == 2U)
        {
            if ((d->class_id == 0U) || (d->entry_count == 0U) ||
                (d->active_index >= d->entry_count))
            {
                return CSM_ERR_DATA_CONTENT_NOT_OK;
            }

            db_ic_inst_t *target = NULL;
            db_ic_reg_table_entry *entry = &d->entries[d->active_index];
            if (!db_ic_find(d->class_id, &entry->obis, &target) || target == NULL)
            {
                return CSM_ERR_OBJECT_NOT_FOUND;
            }

            uint8_t capture_buf[128];
            csm_array capture_out;
            csm_array_init(&capture_out, capture_buf, sizeof(capture_buf), 0U, 0U);
            return (csm_db_code) db_ic_dispatch(target, IC_OP_GET,
                                                entry->attribute_id, 0U,
                                                NULL, &capture_out);
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_reg_table = {
    .class_id  = 61,
    .name      = "Register Table",
    .version   = 0,
    .create    = reg_table_create,
    .dispatch  = reg_table_dispatch
};

void db_ic_register_register_table(void)
{
    db_ic_register(&ic_reg_table);
}
