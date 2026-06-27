/**
 * DLMS/COSEM Compact Data Interface Class handler (class_id = 62)
 *
 * Per Blue Book 4.5.5:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: buffer (octet-string, dynamic)
 * - Attr 3: capture_objects (array of structures, static)
 * - Attr 4: capture_period (Unsigned32, static)
 * - Attr 5: number_of_entries (Unsigned32, dynamic)
 * - Attr 6: entries_in_use (Unsigned32, dynamic)
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

#define COMPACT_MAX_INSTANCES   4U
#define COMPACT_MAX_CAPTURE     8U
#define COMPACT_BUFFER_MAX      256U

typedef struct {
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t attribute_id;
} db_ic_compact_capture_obj;

typedef struct {
    db_ic_compact_capture_obj captures[COMPACT_MAX_CAPTURE];
    uint8_t capture_count;
    uint32_t capture_period;
    uint8_t  buffer[COMPACT_BUFFER_MAX];
    uint16_t buffer_len;
    uint32_t entries_in_use;
} db_ic_compact_data;

static db_ic_compact_data compact_pool[COMPACT_MAX_INSTANCES];
static uint8_t compact_pool_count = 0U;

static db_ic_inst_t compact_inst_tmp;

void db_ic_compact_data_reset_count(void) { compact_pool_count = 0U; }

static const db_ic_attr_descr compact_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET,                  5, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET,                  6, AXDR_TAG_UNSIGNED32 },
};

static const db_ic_method_descr compact_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_NULL },
};

static const db_ic_object_descr compact_descr = {
    .attributes   = compact_attrs,
    .methods      = compact_methods,
    .class_id     = 62,
    .obis         = { 0, 0, 60, 3, 0, 255 },
    .attr_count   = 6,
    .method_count = 2,
    .version      = 0
};

static db_ic_inst_t *compact_create(const csm_obis_code *obis)
{
    (void) obis;
    if (compact_pool_count >= COMPACT_MAX_INSTANCES) { return NULL; }

    db_ic_compact_data *d = &compact_pool[compact_pool_count];
    memset(d, 0, sizeof(db_ic_compact_data));
    compact_pool_count++;

    memset(&compact_inst_tmp, 0, sizeof(db_ic_inst_t));
    compact_inst_tmp.descr   = &compact_descr;
    compact_inst_tmp.data    = d;
    compact_inst_tmp.version = 0U;
    return &compact_inst_tmp;
}

static csm_db_code compact_capture(db_ic_compact_data *d)
{
    if (d->capture_count == 0U) { return CSM_ERR_DATA_CONTENT_NOT_OK; }

    uint8_t new_buffer[COMPACT_BUFFER_MAX];
    uint8_t empty_byte = 0U;
    csm_array capture_out;
    csm_array capture_in;

    memcpy(new_buffer, d->buffer, d->buffer_len);
    csm_array_init(&capture_out, new_buffer, COMPACT_BUFFER_MAX, d->buffer_len, 0U);
    csm_array_init(&capture_in, &empty_byte, 1U, 0U, 0U);

    for (uint8_t i = 0U; i < d->capture_count; i++)
    {
        db_ic_inst_t *captured = NULL;
        if (!db_ic_find(d->captures[i].class_id, &d->captures[i].obis, &captured))
        {
            return CSM_ERR_OBJECT_NOT_FOUND;
        }

        csm_db_code code = (csm_db_code)db_ic_dispatch(captured, IC_OP_GET,
                                                       d->captures[i].attribute_id, 0U,
                                                       &capture_in, &capture_out);
        if (code != CSM_OK) { return code; }
    }

    d->buffer_len = (uint16_t)csm_array_written(&capture_out);
    memcpy(d->buffer, new_buffer, d->buffer_len);
    d->entries_in_use++;
    return CSM_OK;
}

static csm_db_code compact_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                      uint8_t attr_id, uint8_t method_id,
                                      csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_compact_data *d = (db_ic_compact_data *)inst->data;

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
            return csm_axdr_wr_octetstring(out, d->buffer, d->buffer_len)
                ? CSM_OK
                : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, d->capture_count);
            for (uint8_t i = 0U; i < d->capture_count && valid; i++)
            {
                valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
                valid = valid && csm_array_write_u8(out, 3U);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
                valid = valid && csm_array_write_u16(out, d->captures[i].class_id);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
                valid = valid && csm_array_write_u8(out, 6U);
                valid = valid && csm_array_write_u8(out, d->captures[i].obis.A);
                valid = valid && csm_array_write_u8(out, d->captures[i].obis.B);
                valid = valid && csm_array_write_u8(out, d->captures[i].obis.C);
                valid = valid && csm_array_write_u8(out, d->captures[i].obis.D);
                valid = valid && csm_array_write_u8(out, d->captures[i].obis.E);
                valid = valid && csm_array_write_u8(out, d->captures[i].obis.F);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
                valid = valid && csm_array_write_u8(out, d->captures[i].attribute_id);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->capture_period);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 5U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, (uint32_t)d->entries_in_use);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 6U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->entries_in_use);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint32_t len = 0U;
            if (!csm_axdr_rd_octetstring(in, &len) || len > COMPACT_BUFFER_MAX) { return CSM_ERR_BAD_ENCODING; }
            d->buffer_len = (uint16_t)len;
            if (len > 0U)
            {
                if (!csm_array_read_buff(in, d->buffer, len)) { return CSM_ERR_BAD_ENCODING; }
            }
            return CSM_OK;
        }
        else if (attr_id == 3U)
        {
            uint8_t tag = 0xFFU;
            uint8_t count = 0U;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &count) || count > COMPACT_MAX_CAPTURE) { return CSM_ERR_BAD_ENCODING; }
            d->capture_count = count;
            for (uint8_t i = 0U; i < count; i++)
            {
                uint8_t stag = 0xFFU;
                uint8_t flds = 0U;
                if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &flds) || flds < 3U) { return CSM_ERR_BAD_ENCODING; }

                uint8_t ctag = 0xFFU;
                if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u16(in, &d->captures[i].class_id)) { return CSM_ERR_BAD_ENCODING; }

                uint8_t otag = 0xFFU;
                uint8_t olen = 0U;
                if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &olen) || olen != 6U) { return CSM_ERR_BAD_ENCODING; }
                {
                    uint8_t ob[6];
                    if (!csm_array_read_buff(in, ob, 6U)) { return CSM_ERR_BAD_ENCODING; }
                    d->captures[i].obis.A = ob[0];
                    d->captures[i].obis.B = ob[1];
                    d->captures[i].obis.C = ob[2];
                    d->captures[i].obis.D = ob[3];
                    d->captures[i].obis.E = ob[4];
                    d->captures[i].obis.F = ob[5];
                }

                uint8_t atag = 0xFFU;
                if (!csm_array_read_u8(in, &atag) || atag != AXDR_TAG_UNSIGNED8) { return CSM_ERR_BAD_ENCODING; }
                if (!csm_array_read_u8(in, &d->captures[i].attribute_id)) { return CSM_ERR_BAD_ENCODING; }
            }
            return CSM_OK;
        }
        else if (attr_id == 4U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u32(in, &d->capture_period)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            /* reset */
            d->buffer_len = 0U;
            d->entries_in_use = 0U;
            return CSM_OK;
        }
        else if (method_id == 2U)
        {
            return compact_capture(d);
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_compact = {
    .class_id  = 62,
    .name      = "Compact Data",
    .version   = 0,
    .create    = compact_create,
    .dispatch  = compact_dispatch
};

void db_ic_register_compact_data(void)
{
    db_ic_register(&ic_compact);
}
