/**
 * DLMS/COSEM Push Setup Interface Class handler (class_id = 40)
 *
 * Per Blue Book 4.4.8.2:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: push_object_list (array of structures, static)
 * - Attr 3: communication_window (structure, static)
 * - Attr 4: randomisation_interval (unsigned16, static)
 * - Attr 5: number_of_retries (unsigned8, static)
 * - Attr 6: retry_delay_time (unsigned16, static)
 * - Attr 7: repetition_delay (structure, static)
 * - Method 1: push (data)
 * - Method 2: reset (data)
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

#define PUSH_MAX_INSTANCES      4U
#define PUSH_MAX_OBJECTS        8U
#define PUSH_DATETIME_LEN       12U
#define PUSH_TAG_FLOAT32        23U
#define PUSH_TAG_FLOAT64        24U

typedef struct {
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t attribute_index;
} db_ic_push_obj_t;

typedef struct {
    uint8_t start[PUSH_DATETIME_LEN];
    uint8_t end[PUSH_DATETIME_LEN];
} db_ic_push_window_t;

typedef struct {
    uint8_t delay_min;
    uint8_t exponent;
} db_ic_push_rep_delay_t;

typedef struct {
    db_ic_push_obj_t objects[PUSH_MAX_OBJECTS];
    uint8_t object_count;
    db_ic_push_window_t window;
    uint16_t randomisation_interval;
    uint8_t number_of_retries;
    uint16_t retry_delay_time;
    db_ic_push_rep_delay_t repetition_delay;
    uint8_t push_active;
} db_ic_push_data;

static db_ic_push_data push_pool[PUSH_MAX_INSTANCES];
static uint8_t push_pool_count = 0U;

static db_ic_inst_t push_inst_tmp;
static db_ic_inst_t push_inst_pool[PUSH_MAX_INSTANCES];
static uint8_t push_inst_count = 0U;

static const db_ic_attr_descr push_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_UNSIGNED16 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  5, AXDR_TAG_UNSIGNED8 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  6, AXDR_TAG_UNSIGNED16 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  7, AXDR_TAG_STRUCTURE },
};

static const db_ic_method_descr push_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_NULL },
};

static const db_ic_object_descr push_descr = {
    .attributes   = push_attrs,
    .methods      = push_methods,
    .class_id     = 40,
    .obis         = { 0, 0, 25, 1, 0, 255 },
    .attr_count   = 7,
    .method_count = 2,
    .version      = 1
};

static uint8_t push_axdr_fixed_size(uint8_t tag)
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
    case PUSH_TAG_FLOAT32:    return 4U;
    case PUSH_TAG_FLOAT64:    return 8U;
    default:                  return 0U;
    }
}

static int push_axdr_is_length_coded(uint8_t tag)
{
    return (tag == AXDR_TAG_OCTETSTRING) ||
           (tag == AXDR_TAG_VISIBLESTRING) ||
           (tag == AXDR_TAG_UTF8_STRING) ||
           (tag == AXDR_TAG_BITSTRING);
}

static int push_read_axdr_skip(csm_array *in)
{
    uint8_t tag = 0xFFU;
    if (!csm_array_read_u8(in, &tag))
    {
        return FALSE;
    }

    if (tag == AXDR_TAG_NULL)
    {
        return TRUE;
    }
    else if (push_axdr_is_length_coded(tag))
    {
        uint8_t lbyte = 0U;
        if (!csm_array_read_u8(in, &lbyte))
        {
            return FALSE;
        }
        if (lbyte > 0U)
        {
            return csm_array_reader_jump(in, lbyte);
        }
        return TRUE;
    }
    else
    {
        uint8_t dsize = push_axdr_fixed_size(tag);
        if (dsize == 0U)
        {
            return FALSE;
        }
        return csm_array_reader_jump(in, dsize);
    }
}

static db_ic_inst_t *push_create(const csm_obis_code *obis)
{
    (void) obis;

    if (push_pool_count >= PUSH_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_push_data *pd = &push_pool[push_pool_count];
    memset(pd, 0, sizeof(db_ic_push_data));
    memset(pd->window.start, 0xFFU, PUSH_DATETIME_LEN);
    memset(pd->window.end, 0xFFU, PUSH_DATETIME_LEN);
    pd->window.start[4] = 0U;
    pd->window.start[5] = 0U;
    pd->window.end[4] = 23U;
    pd->window.end[5] = 59U;
    pd->randomisation_interval = 0U;
    pd->number_of_retries = 3U;
    pd->retry_delay_time = 30U;
    pd->repetition_delay.delay_min = 10U;
    pd->repetition_delay.exponent = 0U;
    pd->push_active = 0U;
    push_pool_count++;

    db_ic_inst_t *inst = &push_inst_tmp;
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr   = &push_descr;
    inst->data    = pd;
    inst->version = 1U;
    return inst;
}

static csm_db_code push_get_object_list(const db_ic_push_data *pd, csm_array *out)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
    valid = valid && csm_array_write_u8(out, pd->object_count);

    for (uint8_t i = 0U; i < pd->object_count && valid; i++)
    {
        valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
        valid = valid && csm_array_write_u8(out, 3U);
        valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
        valid = valid && csm_array_write_u16(out, pd->objects[i].class_id);
        valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
        valid = valid && csm_array_write_u8(out, 6U);
        valid = valid && csm_array_write_u8(out, pd->objects[i].obis.A);
        valid = valid && csm_array_write_u8(out, pd->objects[i].obis.B);
        valid = valid && csm_array_write_u8(out, pd->objects[i].obis.C);
        valid = valid && csm_array_write_u8(out, pd->objects[i].obis.D);
        valid = valid && csm_array_write_u8(out, pd->objects[i].obis.E);
        valid = valid && csm_array_write_u8(out, pd->objects[i].obis.F);
        valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
        valid = valid && csm_array_write_u8(out, pd->objects[i].attribute_index);
    }

    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code push_get_comm_window(const db_ic_push_data *pd, csm_array *out)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
    valid = valid && csm_array_write_u8(out, 2U);
    valid = valid && csm_array_write_u8(out, 25U);
    valid = valid && csm_array_write_u8(out, PUSH_DATETIME_LEN);
    valid = valid && csm_array_write_buff(out, pd->window.start, PUSH_DATETIME_LEN);
    valid = valid && csm_array_write_u8(out, 25U);
    valid = valid && csm_array_write_u8(out, PUSH_DATETIME_LEN);
    valid = valid && csm_array_write_buff(out, pd->window.end, PUSH_DATETIME_LEN);
    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code push_get_repetition_delay(const db_ic_push_data *pd,
                                               csm_array *out)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
    valid = valid && csm_array_write_u8(out, 2U);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
    valid = valid && csm_array_write_u8(out, pd->repetition_delay.delay_min);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
    valid = valid && csm_array_write_u8(out, pd->repetition_delay.exponent);
    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code push_set_object_list(db_ic_push_data *pd, csm_array *in)
{
    uint8_t tag = 0xFFU;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    uint8_t count = 0U;
    if (!csm_array_read_u8(in, &count) || count > PUSH_MAX_OBJECTS)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    pd->object_count = count;

    for (uint8_t i = 0U; i < count; i++)
    {
        uint8_t stag = 0xFFU;
        uint8_t flds = 0U;
        if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &flds) || flds < 3U)
        {
            return CSM_ERR_BAD_ENCODING;
        }

        uint8_t ctag = 0xFFU;
        if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u16(in, &pd->objects[i].class_id))
        {
            return CSM_ERR_BAD_ENCODING;
        }

        uint8_t otag = 0xFFU;
        uint8_t olen = 0U;
        if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &olen) || olen != 6U)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        {
            uint8_t ob[6] = {0U};
            if (!csm_array_read_buff(in, ob, 6U))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            pd->objects[i].obis.A = ob[0];
            pd->objects[i].obis.B = ob[1];
            pd->objects[i].obis.C = ob[2];
            pd->objects[i].obis.D = ob[3];
            pd->objects[i].obis.E = ob[4];
            pd->objects[i].obis.F = ob[5];
        }

        uint8_t atag = 0xFFU;
        if (!csm_array_read_u8(in, &atag) || atag != AXDR_TAG_UNSIGNED8)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &pd->objects[i].attribute_index))
        {
            return CSM_ERR_BAD_ENCODING;
        }
    }

    return CSM_OK;
}

static csm_db_code push_set_comm_window(db_ic_push_data *pd, csm_array *in)
{
    uint8_t tag = 0xFFU;
    uint8_t flds = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &flds) || flds < 2U)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    /* start time: date-time */
    uint8_t dt_tag = 0xFFU;
    uint8_t dt_len = 0U;
    if (!csm_array_read_u8(in, &dt_tag) || dt_tag != 25U)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &dt_len) || dt_len != PUSH_DATETIME_LEN)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_buff(in, pd->window.start, PUSH_DATETIME_LEN))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    /* end time: date-time */
    if (!csm_array_read_u8(in, &dt_tag) || dt_tag != 25U)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &dt_len) || dt_len != PUSH_DATETIME_LEN)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_buff(in, pd->window.end, PUSH_DATETIME_LEN))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    return CSM_OK;
}

static csm_db_code push_set_repetition_delay(db_ic_push_data *pd, csm_array *in)
{
    uint8_t tag = 0xFFU;
    uint8_t flds = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &flds) || flds < 2U)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    uint8_t dtag = 0xFFU;
    if (!csm_array_read_u8(in, &dtag) || dtag != AXDR_TAG_UNSIGNED8)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &pd->repetition_delay.delay_min))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    if (!csm_array_read_u8(in, &dtag) || dtag != AXDR_TAG_UNSIGNED8)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &pd->repetition_delay.exponent))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    return CSM_OK;
}

static csm_db_code push_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                   uint8_t attr_id, uint8_t method_id,
                                   csm_array *in, csm_array *out)
{
    (void) method_id;

    if ((inst == NULL) || (inst->data == NULL))
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

    db_ic_push_data *pd = (db_ic_push_data *)inst->data;

    if (op == IC_OP_GET)
    {
        switch (attr_id)
        {
        case 1U:
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

        case 2U:
            return push_get_object_list(pd, out);

        case 3U:
            return push_get_comm_window(pd, out);

        case 4U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
            valid = valid && csm_array_write_u16(out, pd->randomisation_interval);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 5U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
            valid = valid && csm_array_write_u8(out, pd->number_of_retries);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 6U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
            valid = valid && csm_array_write_u16(out, pd->retry_delay_time);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 7U:
            return push_get_repetition_delay(pd, out);

        default:
            break;
        }
    }
    else if (op == IC_OP_SET)
    {
        switch (attr_id)
        {
        case 2U:
            return push_set_object_list(pd, in);

        case 3U:
            return push_set_comm_window(pd, in);

        case 4U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u16(in, &pd->randomisation_interval))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 5U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED8)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u8(in, &pd->number_of_retries))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 6U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u16(in, &pd->retry_delay_time))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 7U:
            return push_set_repetition_delay(pd, in);

        default:
            break;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        switch (method_id)
        {
        case 1U:
            /* push: set push_active flag */
            pd->push_active = 1U;
            return CSM_OK;

        case 2U:
            /* reset: clear push state */
            pd->push_active = 0U;
            return CSM_OK;

        default:
            break;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_push = {
    .class_id  = 40,
    .name      = "Push Setup",
    .version   = 1,
    .create    = push_create,
    .dispatch  = push_dispatch
};

void db_ic_register_push_setup(void)
{
    db_ic_register(&ic_push);
}
