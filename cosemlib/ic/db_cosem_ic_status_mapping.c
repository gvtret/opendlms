/**
 * DLMS/COSEM Status Mapping Interface Class handler (class_id = 63)
 *
 * Per Blue Book 4.5.6:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: status_mapping (octet-string, static)
 * - Attr 3: status_mask (octet-string, static)
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

#define STATUS_MAP_MAX_INSTANCES  4U
#define STATUS_MAP_MAX_SIZE       32U

typedef struct {
    uint8_t status_mapping[STATUS_MAP_MAX_SIZE];
    uint8_t mapping_len;
    uint8_t status_mask[STATUS_MAP_MAX_SIZE];
    uint8_t mask_len;
} db_ic_status_map_data;

static db_ic_status_map_data status_pool[STATUS_MAP_MAX_INSTANCES];
static uint8_t status_pool_count = 0U;

static db_ic_inst_t status_inst_tmp;

static const db_ic_attr_descr status_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_OCTETSTRING },
};

static const db_ic_object_descr status_descr = {
    .attributes   = status_attrs,
    .methods      = NULL,
    .class_id     = 63,
    .obis         = { 0, 0, 60, 4, 0, 255 },
    .attr_count   = 3,
    .method_count = 0,
    .version      = 0
};

static db_ic_inst_t *status_create(const csm_obis_code *obis)
{
    (void) obis;
    if (status_pool_count >= STATUS_MAP_MAX_INSTANCES) { return NULL; }

    db_ic_status_map_data *d = &status_pool[status_pool_count];
    memset(d, 0, sizeof(db_ic_status_map_data));
    status_pool_count++;

    memset(&status_inst_tmp, 0, sizeof(db_ic_inst_t));
    status_inst_tmp.descr   = &status_descr;
    status_inst_tmp.data    = d;
    status_inst_tmp.version = 0U;
    return &status_inst_tmp;
}

static csm_db_code status_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                     uint8_t attr_id, uint8_t method_id,
                                     csm_array *in, csm_array *out)
{
    (void) method_id;
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_status_map_data *d = (db_ic_status_map_data *)inst->data;

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
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, d->mapping_len);
            if (d->mapping_len > 0U)
            {
                valid = valid && csm_array_write_buff(out, d->status_mapping, d->mapping_len);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, d->mask_len);
            if (d->mask_len > 0U)
            {
                valid = valid && csm_array_write_buff(out, d->status_mask, d->mask_len);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t tag = 0xFFU;
            uint8_t len = 0U;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &len) || len > STATUS_MAP_MAX_SIZE) { return CSM_ERR_BAD_ENCODING; }
            d->mapping_len = len;
            if (len > 0U)
            {
                if (!csm_array_read_buff(in, d->status_mapping, len)) { return CSM_ERR_BAD_ENCODING; }
            }
            return CSM_OK;
        }
        else if (attr_id == 3U)
        {
            uint8_t tag = 0xFFU;
            uint8_t len = 0U;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &len) || len > STATUS_MAP_MAX_SIZE) { return CSM_ERR_BAD_ENCODING; }
            d->mask_len = len;
            if (len > 0U)
            {
                if (!csm_array_read_buff(in, d->status_mask, len)) { return CSM_ERR_BAD_ENCODING; }
            }
            return CSM_OK;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_status = {
    .class_id  = 63,
    .name      = "Status Mapping",
    .version   = 0,
    .create    = status_create,
    .dispatch  = status_dispatch
};

void db_ic_register_status_mapping(void)
{
    db_ic_register(&ic_status);
}
