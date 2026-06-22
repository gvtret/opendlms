/**
 * DLMS/COSEM Data Interface Class handler (class_id = 1)
 *
 * Per Blue Book 4.3:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: value (CHOICE, dynamic)
 * - Method 1: reset (data)
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

#define DATA_VALUE_MAX       64U
#define DATA_MAX_INSTANCES   8U
#define DATA_TAG_FLOAT32     23U
#define DATA_TAG_FLOAT64     24U

typedef struct {
    uint8_t buf[DATA_VALUE_MAX];
    uint8_t len;
} db_ic_data_val_t;

static db_ic_data_val_t data_val_pool[DATA_MAX_INSTANCES];
static uint8_t data_val_count = 0U;

static db_ic_inst_t data_inst_tmp;
static db_ic_inst_t data_inst_pool[DATA_MAX_INSTANCES];
static uint8_t data_inst_count = 0U;

void db_ic_data_reset_count(void) { data_val_count = 0U; data_inst_count = 0U; }

static const db_ic_attr_descr data_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_NULL },
};

static const db_ic_method_descr data_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
};

static const db_ic_object_descr data_descr = {
    .attributes   = data_attrs,
    .methods      = data_methods,
    .class_id     = 1,
    .obis         = { 0, 0, 0, 0, 0, 0 },
    .attr_count   = 2,
    .method_count = 1,
    .version      = 0
};

static uint8_t axdr_fixed_data_size(uint8_t tag)
{
    switch (tag)
    {
    case AXDR_TAG_NULL:       return 0U;
    case AXDR_TAG_BOOLEAN:    return 1U;
    case AXDR_TAG_INTEGER32:  return 4U;
    case AXDR_TAG_UNSIGNED32: return 4U;
    case AXDR_TAG_INTEGER8:   return 1U;
    case AXDR_TAG_INTEGER16:  return 2U;
    case AXDR_TAG_UNSIGNED8:  return 1U;
    case AXDR_TAG_UNSIGNED16: return 2U;
    case AXDR_TAG_INTEGER64:  return 8U;
    case AXDR_TAG_UNSIGNED64: return 8U;
    case AXDR_TAG_ENUM:       return 1U;
    case DATA_TAG_FLOAT32:    return 4U;
    case DATA_TAG_FLOAT64:    return 8U;
    default:                  return 0U;
    }
}

static int axdr_is_length_coded(uint8_t tag)
{
    return (tag == AXDR_TAG_OCTETSTRING) ||
           (tag == AXDR_TAG_VISIBLESTRING) ||
           (tag == AXDR_TAG_UTF8_STRING) ||
           (tag == AXDR_TAG_BITSTRING);
}

static db_ic_inst_t *data_create(const csm_obis_code *obis)
{
    (void) obis;

    if (data_val_count >= DATA_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_data_val_t *val = &data_val_pool[data_val_count];
    val->buf[0] = AXDR_TAG_NULL;
    val->len = 1U;
    data_val_count++;

    db_ic_inst_t *inst = &data_inst_tmp;
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr   = &data_descr;
    inst->data    = val;
    inst->version = 0U;
    return inst;
}

static csm_db_code data_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
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
            db_ic_data_val_t *val = (db_ic_data_val_t *)inst->data;
            int valid = csm_array_write_buff(out, val->buf, val->len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            db_ic_data_val_t *val = (db_ic_data_val_t *)inst->data;
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag))
            {
                return CSM_ERR_BAD_ENCODING;
            }

            val->buf[0] = tag;

            if (tag == AXDR_TAG_NULL)
            {
                val->len = 1U;
                return CSM_OK;
            }
            else if (axdr_is_length_coded(tag))
            {
                uint8_t len_byte = 0U;
                if (!csm_array_read_u8(in, &len_byte))
                {
                    return CSM_ERR_BAD_ENCODING;
                }
                val->buf[1] = len_byte;
                if (len_byte > 0U)
                {
                    if ((uint16_t)(2U + len_byte) > DATA_VALUE_MAX)
                    {
                        return CSM_ERR_BAD_ENCODING;
                    }
                    if (!csm_array_read_buff(in, &val->buf[2], len_byte))
                    {
                        return CSM_ERR_BAD_ENCODING;
                    }
                }
                val->len = 2U + len_byte;
            }
            else
            {
                uint8_t dsize = axdr_fixed_data_size(tag);
                if (dsize == 0U)
                {
                    return CSM_ERR_BAD_ENCODING;
                }
                if ((uint16_t)(1U + dsize) > DATA_VALUE_MAX)
                {
                    return CSM_ERR_BAD_ENCODING;
                }
                if (!csm_array_read_buff(in, &val->buf[1], dsize))
                {
                    return CSM_ERR_BAD_ENCODING;
                }
                val->len = 1U + dsize;
            }
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            db_ic_data_val_t *val = (db_ic_data_val_t *)inst->data;
            val->buf[0] = AXDR_TAG_NULL;
            val->len = 1U;
            return CSM_OK;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_data_class = {
    .class_id  = 1,
    .name      = "Data",
    .version   = 0,
    .create    = data_create,
    .dispatch  = data_dispatch
};

void db_ic_register_data(void)
{
    db_ic_register(&ic_data_class);
}
