/**
 * DLMS/COSEM Table Manager Interface Class handler (class_id = 8200)
 *
 * Per SPODUS / custom extension:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: table_list (array of structures, static)
 * - Attr 3: active_table_id (Unsigned32, static)
 * - Attr 4: table_data (octet-string, dynamic)
 * - Method 1: retrieve_entries_by_row (structure)
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

#define TABLE_MGR_MAX_INSTANCES  4U
#define TABLE_MGR_MAX_TABLES     8U
#define TABLE_MGR_DATA_MAX       256U

typedef struct {
    uint32_t table_id;
    uint8_t  name[16];
    uint8_t  name_len;
} db_ic_table_mgr_entry;

typedef struct {
    db_ic_table_mgr_entry tables[TABLE_MGR_MAX_TABLES];
    uint8_t table_count;
    uint32_t active_table_id;
    uint8_t  table_data[TABLE_MGR_DATA_MAX];
    uint16_t table_data_len;
} db_ic_table_mgr_data;

static db_ic_table_mgr_data table_mgr_pool[TABLE_MGR_MAX_INSTANCES];
static uint8_t table_mgr_pool_count = 0U;

static db_ic_inst_t table_mgr_inst_tmp;

void db_ic_table_manager_reset_count(void) { table_mgr_pool_count = 0U; }

static const db_ic_attr_descr table_mgr_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_OCTETSTRING },
};

static const db_ic_method_descr table_mgr_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_STRUCTURE },
};

static const db_ic_object_descr table_mgr_descr = {
    .attributes   = table_mgr_attrs,
    .methods      = table_mgr_methods,
    .class_id     = 8200,
    .obis         = { 0, 0, 96, 9, 0, 255 },
    .attr_count   = 4,
    .method_count = 1,
    .version      = 0
};

static db_ic_inst_t *table_mgr_create(const csm_obis_code *obis)
{
    (void) obis;
    if (table_mgr_pool_count >= TABLE_MGR_MAX_INSTANCES) { return NULL; }

    db_ic_table_mgr_data *d = &table_mgr_pool[table_mgr_pool_count];
    memset(d, 0, sizeof(db_ic_table_mgr_data));
    table_mgr_pool_count++;

    memset(&table_mgr_inst_tmp, 0, sizeof(db_ic_inst_t));
    table_mgr_inst_tmp.descr   = &table_mgr_descr;
    table_mgr_inst_tmp.data    = d;
    table_mgr_inst_tmp.version = 0U;
    return &table_mgr_inst_tmp;
}

static int table_mgr_read_row_selector(csm_array *in, uint32_t *from, uint32_t *count)
{
    uint8_t tag = 0xFFU;
    uint8_t fields = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) { return FALSE; }
    if (!csm_array_read_u8(in, &fields) || fields != 2U) { return FALSE; }
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return FALSE; }
    if (!csm_array_read_u32(in, from)) { return FALSE; }
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return FALSE; }
    if (!csm_array_read_u32(in, count)) { return FALSE; }
    return TRUE;
}

static csm_db_code table_mgr_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                        uint8_t attr_id, uint8_t method_id,
                                        csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_table_mgr_data *d = (db_ic_table_mgr_data *)inst->data;

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
            valid = valid && csm_array_write_u8(out, d->table_count);
            for (uint8_t i = 0U; i < d->table_count && valid; i++)
            {
                valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
                valid = valid && csm_array_write_u8(out, 2U);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
                valid = valid && csm_array_write_u32(out, d->tables[i].table_id);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
                valid = valid && csm_array_write_u8(out, d->tables[i].name_len);
                if (d->tables[i].name_len > 0U)
                {
                    valid = valid && csm_array_write_buff(out, d->tables[i].name,
                                                            d->tables[i].name_len);
                }
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->active_table_id);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            return csm_axdr_wr_octetstring(out, d->table_data, d->table_data_len)
                ? CSM_OK
                : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t tag = 0xFFU;
            uint8_t count = 0U;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &count) || count > TABLE_MGR_MAX_TABLES) { return CSM_ERR_BAD_ENCODING; }
            d->table_count = count;
            for (uint8_t i = 0U; i < count; i++)
            {
                uint8_t stag = 0xFFU;
                uint8_t flds = 0U;
                if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &flds) || flds < 2U) { return CSM_ERR_BAD_ENCODING; }

                uint8_t utag = 0xFFU;
                if (!csm_array_read_u8(in, &utag) || utag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u32(in, &d->tables[i].table_id)) { return CSM_ERR_BAD_ENCODING; }

                uint8_t otag = 0xFFU;
                uint8_t olen = 0U;
                if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &olen) || olen > 16U) { return CSM_ERR_BAD_ENCODING; }
                d->tables[i].name_len = olen;
                if (olen > 0U)
                {
                    if (!csm_array_read_buff(in, d->tables[i].name, olen)) { return CSM_ERR_BAD_ENCODING; }
                }
            }
            return CSM_OK;
        }
        else if (attr_id == 3U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u32(in, &d->active_table_id)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 4U)
        {
            uint32_t len = 0U;
            if (!csm_axdr_rd_octetstring(in, &len) || len > TABLE_MGR_DATA_MAX) { return CSM_ERR_BAD_ENCODING; }
            d->table_data_len = (uint16_t)len;
            if (len > 0U)
            {
                if (!csm_array_read_buff(in, d->table_data, len)) { return CSM_ERR_BAD_ENCODING; }
            }
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            uint32_t from = 0U;
            uint32_t count = 0U;
            if (!table_mgr_read_row_selector(in, &from, &count)) { return CSM_ERR_BAD_ENCODING; }
            (void)from;
            (void)count;
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, 0U);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_table_mgr = {
    .class_id  = 8200,
    .name      = "Table Manager",
    .version   = 0,
    .create    = table_mgr_create,
    .dispatch  = table_mgr_dispatch
};

void db_ic_register_table_manager(void)
{
    db_ic_register(&ic_table_mgr);
}
