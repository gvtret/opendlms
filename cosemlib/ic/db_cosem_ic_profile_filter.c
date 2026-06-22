/**
 * DLMS/COSEM Profile Filter Interface Class handler (class_id = 31)
 *
 * Per Blue Book 4.4.7:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: filter_enable (Unsigned32, static)
 * - Attr 3: filter_list (array of structures, static)
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

#define PROF_FILTER_MAX_INSTANCES  4U
#define PROF_FILTER_MAX_ENTRIES    8U

typedef struct {
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t attribute_id;
} db_ic_prof_filter_entry;

typedef struct {
    uint32_t filter_enable;
    db_ic_prof_filter_entry entries[PROF_FILTER_MAX_ENTRIES];
    uint8_t entry_count;
} db_ic_prof_filter_data;

static db_ic_prof_filter_data prof_filter_pool[PROF_FILTER_MAX_INSTANCES];
static uint8_t prof_filter_pool_count = 0U;

static db_ic_inst_t prof_filter_inst_tmp;

static const db_ic_attr_descr prof_filter_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_ARRAY },
};

static const db_ic_method_descr prof_filter_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_STRUCTURE },
};

static const db_ic_object_descr prof_filter_descr = {
    .attributes   = prof_filter_attrs,
    .methods      = prof_filter_methods,
    .class_id     = 31,
    .obis         = { 0, 0, 43, 1, 0, 255 },
    .attr_count   = 3,
    .method_count = 1,
    .version      = 0
};

static db_ic_inst_t *prof_filter_create(const csm_obis_code *obis)
{
    (void) obis;
    if (prof_filter_pool_count >= PROF_FILTER_MAX_INSTANCES) { return NULL; }

    db_ic_prof_filter_data *d = &prof_filter_pool[prof_filter_pool_count];
    memset(d, 0, sizeof(db_ic_prof_filter_data));
    prof_filter_pool_count++;

    memset(&prof_filter_inst_tmp, 0, sizeof(db_ic_inst_t));
    prof_filter_inst_tmp.descr   = &prof_filter_descr;
    prof_filter_inst_tmp.data    = d;
    prof_filter_inst_tmp.version = 0U;
    return &prof_filter_inst_tmp;
}

static csm_db_code prof_filter_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                          uint8_t attr_id, uint8_t method_id,
                                          csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_prof_filter_data *d = (db_ic_prof_filter_data *)inst->data;

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
            valid = valid && csm_array_write_u32(out, d->filter_enable);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
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
            if (!csm_array_read_u32(in, &d->filter_enable)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 3U)
        {
            uint8_t tag = 0xFFU;
            uint8_t count = 0U;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &count) || count > PROF_FILTER_MAX_ENTRIES) { return CSM_ERR_BAD_ENCODING; }
            d->entry_count = count;
            for (uint8_t i = 0U; i < count; i++)
            {
                uint8_t stag = 0xFFU;
                uint8_t flds = 0U;
                if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &flds) || flds < 3U) { return CSM_ERR_BAD_ENCODING; }

                uint8_t ctag = 0xFFU;
                if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u16(in, &d->entries[i].class_id)) { return CSM_ERR_BAD_ENCODING; }

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
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            /* retrieve_entries_by_row: skip structure input, return empty array */
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, 0U);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_prof_filter = {
    .class_id  = 31,
    .name      = "Profile Filter",
    .version   = 0,
    .create    = prof_filter_create,
    .dispatch  = prof_filter_dispatch
};

void db_ic_register_profile_filter(void)
{
    db_ic_register(&ic_prof_filter);
}
