/**
 * DLMS/COSEM Register Activation Interface Class handler (class_id = 6)
 *
 * Per Blue Book 4.3.5:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: register_activation_object_list (array of structures, static)
 * - Attr 3: register_activation_object_list_index (array of Unsigned16, static)
 * - Attr 4: mask_list (array of structures, static)
 * - Method 1: add_register (structure)
 * - Method 2: add_mask (structure)
 * - Method 3: delete_mask (Unsigned16)
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

#define REG_ACT_MAX_INSTANCES   4U
#define REG_ACT_MAX_OBJECTS     8U
#define REG_ACT_MAX_MASKS       8U

typedef struct {
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t attribute_id;
} db_ic_reg_act_obj;

typedef struct {
    uint16_t mask_id;
    uint16_t object_index;
} db_ic_reg_act_mask_entry;

typedef struct {
    uint16_t mask_id;
    db_ic_reg_act_mask_entry entries[4];
    uint8_t entry_count;
} db_ic_reg_act_mask;

typedef struct {
    db_ic_reg_act_obj objects[REG_ACT_MAX_OBJECTS];
    uint8_t object_count;
    uint16_t indices[REG_ACT_MAX_OBJECTS];
    uint8_t index_count;
    db_ic_reg_act_mask masks[REG_ACT_MAX_MASKS];
    uint8_t mask_count;
} db_ic_reg_act_data;

static db_ic_reg_act_data reg_act_pool[REG_ACT_MAX_INSTANCES];
static uint8_t reg_act_pool_count = 0U;

static db_ic_inst_t reg_act_inst_tmp;

void db_ic_register_activation_reset_count(void) { reg_act_pool_count = 0U; }

static const db_ic_attr_descr reg_act_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  2, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET,                  4, AXDR_TAG_ARRAY },
};

static const db_ic_method_descr reg_act_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_ACTION, 3, AXDR_TAG_UNSIGNED16 },
};

static const db_ic_object_descr reg_act_descr = {
    .attributes   = reg_act_attrs,
    .methods      = reg_act_methods,
    .class_id     = 6,
    .obis         = { 0, 0, 60, 5, 0, 255 },
    .attr_count   = 4,
    .method_count = 3,
    .version      = 0
};

static db_ic_inst_t *reg_act_create(const csm_obis_code *obis)
{
    (void) obis;
    if (reg_act_pool_count >= REG_ACT_MAX_INSTANCES) { return NULL; }

    db_ic_reg_act_data *d = &reg_act_pool[reg_act_pool_count];
    memset(d, 0, sizeof(db_ic_reg_act_data));
    reg_act_pool_count++;

    memset(&reg_act_inst_tmp, 0, sizeof(db_ic_inst_t));
    reg_act_inst_tmp.descr   = &reg_act_descr;
    reg_act_inst_tmp.data    = d;
    reg_act_inst_tmp.version = 0U;
    return &reg_act_inst_tmp;
}

static int reg_act_read_object(csm_array *in, db_ic_reg_act_obj *obj)
{
    uint8_t tag = 0xFFU;
    uint8_t fields = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) { return FALSE; }
    if (!csm_array_read_u8(in, &fields) || fields != 3U) { return FALSE; }

    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return FALSE; }
    if (!csm_array_read_u16(in, &obj->class_id)) { return FALSE; }

    uint8_t obis_len = 0U;
    uint8_t obis[6];
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) { return FALSE; }
    if (!csm_array_read_u8(in, &obis_len) || obis_len != sizeof(obis)) { return FALSE; }
    if (!csm_array_read_buff(in, obis, sizeof(obis))) { return FALSE; }
    obj->obis.A = obis[0];
    obj->obis.B = obis[1];
    obj->obis.C = obis[2];
    obj->obis.D = obis[3];
    obj->obis.E = obis[4];
    obj->obis.F = obis[5];

    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED8) { return FALSE; }
    if (!csm_array_read_u8(in, &obj->attribute_id)) { return FALSE; }
    return TRUE;
}

static int reg_act_read_mask(csm_array *in, db_ic_reg_act_mask *mask)
{
    uint8_t tag = 0xFFU;
    uint8_t fields = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) { return FALSE; }
    if (!csm_array_read_u8(in, &fields) || fields != 2U) { return FALSE; }

    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return FALSE; }
    if (!csm_array_read_u16(in, &mask->mask_id)) { return FALSE; }

    uint8_t entry_count = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY) { return FALSE; }
    if (!csm_array_read_u8(in, &entry_count) || entry_count > 4U) { return FALSE; }
    mask->entry_count = entry_count;

    for (uint8_t i = 0U; i < entry_count; i++)
    {
        uint8_t entry_fields = 0U;
        if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) { return FALSE; }
        if (!csm_array_read_u8(in, &entry_fields) || entry_fields != 2U) { return FALSE; }

        if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return FALSE; }
        if (!csm_array_read_u16(in, &mask->entries[i].mask_id)) { return FALSE; }

        if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return FALSE; }
        if (!csm_array_read_u16(in, &mask->entries[i].object_index)) { return FALSE; }
    }

    return TRUE;
}

static csm_db_code reg_act_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                      uint8_t attr_id, uint8_t method_id,
                                      csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_reg_act_data *d = (db_ic_reg_act_data *)inst->data;

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
            valid = valid && csm_array_write_u8(out, d->object_count);
            for (uint8_t i = 0U; i < d->object_count && valid; i++)
            {
                valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
                valid = valid && csm_array_write_u8(out, 3U);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
                valid = valid && csm_array_write_u16(out, d->objects[i].class_id);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
                valid = valid && csm_array_write_u8(out, 6U);
                valid = valid && csm_array_write_u8(out, d->objects[i].obis.A);
                valid = valid && csm_array_write_u8(out, d->objects[i].obis.B);
                valid = valid && csm_array_write_u8(out, d->objects[i].obis.C);
                valid = valid && csm_array_write_u8(out, d->objects[i].obis.D);
                valid = valid && csm_array_write_u8(out, d->objects[i].obis.E);
                valid = valid && csm_array_write_u8(out, d->objects[i].obis.F);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
                valid = valid && csm_array_write_u8(out, d->objects[i].attribute_id);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, d->index_count);
            for (uint8_t i = 0U; i < d->index_count && valid; i++)
            {
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
                valid = valid && csm_array_write_u16(out, d->indices[i]);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
            valid = valid && csm_array_write_u8(out, d->mask_count);
            for (uint8_t i = 0U; i < d->mask_count && valid; i++)
            {
                valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
                valid = valid && csm_array_write_u8(out, 2U);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
                valid = valid && csm_array_write_u16(out, d->masks[i].mask_id);
                valid = valid && csm_array_write_u8(out, AXDR_TAG_ARRAY);
                valid = valid && csm_array_write_u8(out, d->masks[i].entry_count);
                for (uint8_t j = 0U; j < d->masks[i].entry_count && valid; j++)
                {
                    valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
                    valid = valid && csm_array_write_u8(out, 2U);
                    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
                    valid = valid && csm_array_write_u16(out, d->masks[i].entries[j].mask_id);
                    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
                    valid = valid && csm_array_write_u16(out, d->masks[i].entries[j].object_index);
                }
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        switch (method_id)
        {
        case 1U:
            if (d->object_count >= REG_ACT_MAX_OBJECTS) { return CSM_ERR_DATA_CONTENT_NOT_OK; }
            if (!reg_act_read_object(in, &d->objects[d->object_count])) { return CSM_ERR_BAD_ENCODING; }
            d->indices[d->index_count] = d->object_count;
            d->object_count++;
            d->index_count++;
            return CSM_OK;

        case 2U:
            if (d->mask_count >= REG_ACT_MAX_MASKS) { return CSM_ERR_DATA_CONTENT_NOT_OK; }
            if (!reg_act_read_mask(in, &d->masks[d->mask_count])) { return CSM_ERR_BAD_ENCODING; }
            d->mask_count++;
            return CSM_OK;

        case 3U: /* delete_mask: read Unsigned16, remove from mask list */
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) { return CSM_ERR_BAD_ENCODING; }
            uint16_t mask_id = 0U;
            if (!csm_array_read_u16(in, &mask_id)) { return CSM_ERR_BAD_ENCODING; }
            for (uint8_t i = 0U; i < d->mask_count; i++)
            {
                if (d->masks[i].mask_id == mask_id)
                {
                    for (uint8_t j = i; j < d->mask_count - 1U; j++)
                    {
                        d->masks[j] = d->masks[j + 1U];
                    }
                    d->mask_count--;
                    return CSM_OK;
                }
            }
            return CSM_ERR_DATA_CONTENT_NOT_OK;
        }
        default:
            break;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_reg_act = {
    .class_id  = 6,
    .name      = "Register Activation",
    .version   = 0,
    .create    = reg_act_create,
    .dispatch  = reg_act_dispatch
};

void db_ic_register_register_activation(void)
{
    db_ic_register(&ic_reg_act);
}
