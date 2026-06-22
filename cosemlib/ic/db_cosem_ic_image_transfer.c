/**
 * DLMS/COSEM Image Transfer Interface Class handler (class_id = 18)
 *
 * Per Blue Book 4.4.1:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: image_transfer_status (enum, dynamic)
 * - Attr 3: image_blocks_transferred (unsigned32, dynamic)
 * - Attr 4: image_block_size (unsigned32, static)
 * - Attr 5: image_transferred_blocks_status (octet-string, dynamic)
 * - Attr 6: image_first_not_transferred_block_number (unsigned32, dynamic)
 * - Attr 7: image_transfer_enabled (boolean, static)
 * - Method 1: image_block_transfer (data)
 * - Method 2: image_transfer_init (structure)
 * - Method 3: image_transfer_start (unsigned32)
 * - Method 4: image_transfer_stop (null)
 * - Method 5: image_verify (structure)
 * - Method 6: image_activate (null)
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

#define IMG_MAX_INSTANCES       2U
#define IMG_BLOCK_STATUS_MAX    32U

/* Image transfer status enum values */
#define IMG_STATUS_IDLE                 0U
#define IMG_STATUS_INITIATED            1U
#define IMG_STATUS_INITIALIZATION_OK    2U
#define IMG_STATUS_INITIALIZATION_ERROR 3U
#define IMG_STATUS_VERIFICATION_OK      4U
#define IMG_STATUS_VERIFICATION_ERROR   5U
#define IMG_STATUS_ACTIVATION_OK        6U
#define IMG_STATUS_ACTIVATION_ERROR     7U

typedef struct {
    uint8_t  transfer_status;
    uint32_t blocks_transferred;
    uint32_t block_size;
    uint8_t  transferred_blocks_status[IMG_BLOCK_STATUS_MAX];
    uint8_t  transferred_blocks_len;
    uint32_t first_not_transferred;
    uint8_t  transfer_enabled;
} db_ic_image_data;

static db_ic_image_data image_pool[IMG_MAX_INSTANCES];
static uint8_t image_pool_count = 0U;

static db_ic_inst_t image_inst_tmp;

static const db_ic_attr_descr image_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  2, AXDR_TAG_ENUM },
    { DB_ACCESS_GET,                  3, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET,                  5, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  6, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  7, AXDR_TAG_BOOLEAN },
};

static const db_ic_method_descr image_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_ACTION, 3, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_ACTION, 4, AXDR_TAG_NULL },
    { DB_ACCESS_ACTION, 5, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_ACTION, 6, AXDR_TAG_NULL },
};

static const db_ic_object_descr image_descr = {
    .attributes   = image_attrs,
    .methods      = image_methods,
    .class_id     = 18,
    .obis         = { 0, 0, 44, 0, 0, 255 },
    .attr_count   = 7,
    .method_count = 6,
    .version      = 0
};

static db_ic_inst_t *image_create(const csm_obis_code *obis)
{
    (void) obis;
    if (image_pool_count >= IMG_MAX_INSTANCES) { return NULL; }

    db_ic_image_data *d = &image_pool[image_pool_count];
    memset(d, 0, sizeof(db_ic_image_data));
    d->transfer_status = IMG_STATUS_IDLE;
    d->block_size = 50U;
    d->transfer_enabled = 1U;
    image_pool_count++;

    memset(&image_inst_tmp, 0, sizeof(db_ic_inst_t));
    image_inst_tmp.descr   = &image_descr;
    image_inst_tmp.data    = d;
    image_inst_tmp.version = 0U;
    return &image_inst_tmp;
}

static csm_db_code image_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                    uint8_t attr_id, uint8_t method_id,
                                    csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_image_data *d = (db_ic_image_data *)inst->data;

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
            int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
            valid = valid && csm_array_write_u8(out, d->transfer_status);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->blocks_transferred);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->block_size);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 5U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, d->transferred_blocks_len);
            if (d->transferred_blocks_len > 0U)
            {
                valid = valid && csm_array_write_buff(out, d->transferred_blocks_status,
                                                        d->transferred_blocks_len);
            }
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 6U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, d->first_not_transferred);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 7U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_BOOLEAN);
            valid = valid && csm_array_write_u8(out, d->transfer_enabled);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 4U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u32(in, &d->block_size)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 7U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_BOOLEAN) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &d->transfer_enabled)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        switch (method_id)
        {
        case 1U: /* image_block_transfer: skip input data, acknowledge */
            return CSM_OK;
        case 2U: /* image_transfer_init: skip structure input */
            d->transfer_status = IMG_STATUS_INITIALIZATION_OK;
            d->blocks_transferred = 0U;
            d->first_not_transferred = 0U;
            return CSM_OK;
        case 3U: /* image_transfer_start */
            d->transfer_status = IMG_STATUS_INITIATED;
            return CSM_OK;
        case 4U: /* image_transfer_stop */
            d->transfer_status = IMG_STATUS_IDLE;
            return CSM_OK;
        case 5U: /* image_verify */
            d->transfer_status = IMG_STATUS_VERIFICATION_OK;
            return CSM_OK;
        case 6U: /* image_activate */
            d->transfer_status = IMG_STATUS_ACTIVATION_OK;
            return CSM_OK;
        default:
            break;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_image = {
    .class_id  = 18,
    .name      = "Image Transfer",
    .version   = 0,
    .create    = image_create,
    .dispatch  = image_dispatch
};

void db_ic_register_image_transfer(void)
{
    db_ic_register(&ic_image);
}
