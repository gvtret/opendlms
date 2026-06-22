/**
 * DLMS/COSEM Limiter Interface Class handler (class_id = 71)
 *
 * Per Blue Book 4.5.9:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: monitored_value (structure, static)
 * - Attr 3: threshold_active (numeric, static)
 * - Attr 4: threshold_normal (numeric, static)
 * - Attr 5: threshold_emergency (numeric, static)
 * - Attr 6: min_over_duration (unsigned32, static)
 * - Attr 7: min_under_duration (unsigned32, static)
 * - Attr 8: emergency_profile_id (unsigned32, dynamic)
 * - Attr 9: emergency_profile_active (boolean, dynamic)
 * - Attr 10: emergency_profile_group_id_list (array of unsigned32, static)
 * - Attr 11: action_over (structure, static)
 * - Method 1: reset (no-response)
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

#define LIMITER_MAX_INSTANCES   4U
#define LIMITER_MAX_GROUPS      8U
#define LIMITER_TAG_FLOAT32     23U
#define LIMITER_TAG_FLOAT64     24U

typedef struct {
    /* monitored_value: {class_id, obis, attribute_index} */
    uint16_t mon_class_id;
    csm_obis_code mon_obis;
    uint8_t mon_attribute_id;

    /* threshold values stored as raw AXDR */
    uint8_t threshold_active_buf[32];
    uint8_t threshold_active_len;
    uint8_t threshold_normal_buf[32];
    uint8_t threshold_normal_len;
    uint8_t threshold_emergency_buf[32];
    uint8_t threshold_emergency_len;

    uint32_t min_over_duration;
    uint32_t min_under_duration;

    /* emergency profile */
    uint32_t emergency_profile_id;
    uint8_t  emergency_profile_active;

    /* emergency_profile_group_id_list */
    uint32_t group_ids[LIMITER_MAX_GROUPS];
    uint8_t  group_count;

    /* action_over: {script_id, script_parameter} */
    uint16_t action_script_id;
    uint8_t  action_script_param_buf[32];
    uint8_t  action_script_param_len;

    /* limiter state */
    uint8_t  over_threshold;
    uint8_t  under_threshold;
} db_ic_limiter_data;

static db_ic_limiter_data limiter_pool[LIMITER_MAX_INSTANCES];
static uint8_t limiter_pool_count = 0U;

static db_ic_inst_t limiter_inst_tmp;
static db_ic_inst_t limiter_inst_pool[LIMITER_MAX_INSTANCES];
static uint8_t limiter_inst_count = 0U;

void db_ic_limiter_reset_count(void) { limiter_pool_count = 0U; limiter_inst_count = 0U; }

static const db_ic_attr_descr limiter_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  2, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_NULL },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_NULL },
    { DB_ACCESS_GET | DB_ACCESS_SET,  5, AXDR_TAG_NULL },
    { DB_ACCESS_GET | DB_ACCESS_SET,  6, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  7, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  8, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  9, AXDR_TAG_BOOLEAN },
    { DB_ACCESS_GET,                 10, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET,                 11, AXDR_TAG_STRUCTURE },
};

static const db_ic_method_descr limiter_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
};

static const db_ic_object_descr limiter_descr = {
    .attributes   = limiter_attrs,
    .methods      = limiter_methods,
    .class_id     = 71,
    .obis         = { 0, 0, 96, 3, 11, 255 },
    .attr_count   = 11,
    .method_count = 1,
    .version      = 0
};

static uint8_t lim_axdr_fixed_size(uint8_t tag)
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
    case LIMITER_TAG_FLOAT32: return 4U;
    case LIMITER_TAG_FLOAT64: return 8U;
    default:                  return 0U;
    }
}

static int lim_axdr_is_length_coded(uint8_t tag)
{
    return (tag == AXDR_TAG_OCTETSTRING) ||
           (tag == AXDR_TAG_VISIBLESTRING) ||
           (tag == AXDR_TAG_UTF8_STRING) ||
           (tag == AXDR_TAG_BITSTRING);
}

static int lim_read_axdr_value(csm_array *in, uint8_t *buf, uint8_t *len,
                                uint8_t max_len)
{
    uint8_t tag = 0xFFU;
    if (!csm_array_read_u8(in, &tag))
    {
        return FALSE;
    }

    buf[0] = tag;

    if (tag == AXDR_TAG_NULL)
    {
        *len = 1U;
        return TRUE;
    }
    else if (lim_axdr_is_length_coded(tag))
    {
        uint8_t lbyte = 0U;
        if (!csm_array_read_u8(in, &lbyte))
        {
            return FALSE;
        }
        if ((uint16_t)(2U + lbyte) > max_len)
        {
            return FALSE;
        }
        buf[1] = lbyte;
        if (lbyte > 0U)
        {
            if (!csm_array_read_buff(in, &buf[2], lbyte))
            {
                return FALSE;
            }
        }
        *len = 2U + lbyte;
        return TRUE;
    }
    else
    {
        uint8_t dsize = lim_axdr_fixed_size(tag);
        if (dsize == 0U)
        {
            return FALSE;
        }
        if ((uint16_t)(1U + dsize) > max_len)
        {
            return FALSE;
        }
        if (!csm_array_read_buff(in, &buf[1], dsize))
        {
            return FALSE;
        }
        *len = 1U + dsize;
        return TRUE;
    }
}

static db_ic_inst_t *limiter_create(const csm_obis_code *obis)
{
    (void) obis;

    if (limiter_pool_count >= LIMITER_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_limiter_data *ld = &limiter_pool[limiter_pool_count];
    memset(ld, 0, sizeof(db_ic_limiter_data));
    ld->threshold_active_buf[0] = AXDR_TAG_NULL;
    ld->threshold_active_len = 1U;
    ld->threshold_normal_buf[0] = AXDR_TAG_NULL;
    ld->threshold_normal_len = 1U;
    ld->threshold_emergency_buf[0] = AXDR_TAG_NULL;
    ld->threshold_emergency_len = 1U;
    ld->min_over_duration = 0U;
    ld->min_under_duration = 0U;
    ld->emergency_profile_id = 0U;
    ld->emergency_profile_active = 0U;
    ld->group_count = 0U;
    ld->action_script_id = 0U;
    ld->action_script_param_buf[0] = AXDR_TAG_NULL;
    ld->action_script_param_len = 1U;
    ld->over_threshold = 0U;
    ld->under_threshold = 0U;
    limiter_pool_count++;

    db_ic_inst_t *inst = &limiter_inst_tmp;
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr   = &limiter_descr;
    inst->data    = ld;
    inst->version = 0U;
    return inst;
}

static csm_db_code limiter_get_monitored_value(const db_ic_limiter_data *ld,
                                                 csm_array *out)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
    valid = valid && csm_array_write_u8(out, 3U);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
    valid = valid && csm_array_write_u16(out, ld->mon_class_id);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
    valid = valid && csm_array_write_u8(out, 6U);
    valid = valid && csm_array_write_u8(out, ld->mon_obis.A);
    valid = valid && csm_array_write_u8(out, ld->mon_obis.B);
    valid = valid && csm_array_write_u8(out, ld->mon_obis.C);
    valid = valid && csm_array_write_u8(out, ld->mon_obis.D);
    valid = valid && csm_array_write_u8(out, ld->mon_obis.E);
    valid = valid && csm_array_write_u8(out, ld->mon_obis.F);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
    valid = valid && csm_array_write_u8(out, ld->mon_attribute_id);
    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code limiter_set_monitored_value(db_ic_limiter_data *ld,
                                                 csm_array *in)
{
    uint8_t tag = 0xFFU;
    uint8_t flds = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE)
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
    if (!csm_array_read_u16(in, &ld->mon_class_id))
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
        ld->mon_obis.A = ob[0];
        ld->mon_obis.B = ob[1];
        ld->mon_obis.C = ob[2];
        ld->mon_obis.D = ob[3];
        ld->mon_obis.E = ob[4];
        ld->mon_obis.F = ob[5];
    }

    uint8_t atag = 0xFFU;
    if (!csm_array_read_u8(in, &atag) || atag != AXDR_TAG_UNSIGNED8)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &ld->mon_attribute_id))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    return CSM_OK;
}

static csm_db_code limiter_get_group_id_list(const db_ic_limiter_data *ld,
                                               csm_array *out)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
    valid = valid && csm_array_write_u8(out, ld->group_count);

    for (uint8_t i = 0U; i < ld->group_count && valid; i++)
    {
        valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
        valid = valid && csm_array_write_u32(out, ld->group_ids[i]);
    }

    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code limiter_set_group_id_list(db_ic_limiter_data *ld,
                                               csm_array *in)
{
    uint8_t tag = 0xFFU;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    uint8_t count = 0U;
    if (!csm_array_read_u8(in, &count) || count > LIMITER_MAX_GROUPS)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    ld->group_count = count;

    for (uint8_t i = 0U; i < count; i++)
    {
        uint8_t gtag = 0xFFU;
        if (!csm_array_read_u8(in, &gtag) || gtag != AXDR_TAG_UNSIGNED32)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u32(in, &ld->group_ids[i]))
        {
            return CSM_ERR_BAD_ENCODING;
        }
    }

    return CSM_OK;
}

static csm_db_code limiter_get_action_over(const db_ic_limiter_data *ld,
                                             csm_array *out)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
    valid = valid && csm_array_write_u8(out, 2U);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
    valid = valid && csm_array_write_u16(out, ld->action_script_id);
    valid = valid && csm_array_write_buff(out, ld->action_script_param_buf,
                                            ld->action_script_param_len);
    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code limiter_set_action_over(db_ic_limiter_data *ld,
                                             csm_array *in)
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

    uint8_t stag = 0xFFU;
    if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_UNSIGNED16)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u16(in, &ld->action_script_id))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    if (!lim_read_axdr_value(in, ld->action_script_param_buf,
                              &ld->action_script_param_len, 32U))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    return CSM_OK;
}

static csm_db_code limiter_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                      uint8_t attr_id, uint8_t method_id,
                                      csm_array *in, csm_array *out)
{
    (void) method_id;

    if ((inst == NULL) || (inst->data == NULL))
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

    db_ic_limiter_data *ld = (db_ic_limiter_data *)inst->data;

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
            return limiter_get_monitored_value(ld, out);

        case 3U:
            return csm_array_write_buff(out, ld->threshold_active_buf,
                                          ld->threshold_active_len)
                   ? CSM_OK : CSM_ERR_BAD_ENCODING;

        case 4U:
            return csm_array_write_buff(out, ld->threshold_normal_buf,
                                          ld->threshold_normal_len)
                   ? CSM_OK : CSM_ERR_BAD_ENCODING;

        case 5U:
            return csm_array_write_buff(out, ld->threshold_emergency_buf,
                                          ld->threshold_emergency_len)
                   ? CSM_OK : CSM_ERR_BAD_ENCODING;

        case 6U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, ld->min_over_duration);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 7U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, ld->min_under_duration);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 8U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, ld->emergency_profile_id);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 9U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_BOOLEAN);
            valid = valid && csm_array_write_u8(out, ld->emergency_profile_active);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 10U:
            return limiter_get_group_id_list(ld, out);

        case 11U:
            return limiter_get_action_over(ld, out);

        default:
            break;
        }
    }
    else if (op == IC_OP_SET)
    {
        switch (attr_id)
        {
        case 2U:
            return limiter_set_monitored_value(ld, in);

        case 3U:
            return lim_read_axdr_value(in, ld->threshold_active_buf,
                                        &ld->threshold_active_len, 32U)
                   ? CSM_OK : CSM_ERR_BAD_ENCODING;

        case 4U:
            return lim_read_axdr_value(in, ld->threshold_normal_buf,
                                        &ld->threshold_normal_len, 32U)
                   ? CSM_OK : CSM_ERR_BAD_ENCODING;

        case 5U:
            return lim_read_axdr_value(in, ld->threshold_emergency_buf,
                                        &ld->threshold_emergency_len, 32U)
                   ? CSM_OK : CSM_ERR_BAD_ENCODING;

        case 6U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u32(in, &ld->min_over_duration))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 7U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u32(in, &ld->min_under_duration))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 8U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u32(in, &ld->emergency_profile_id))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 9U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_BOOLEAN)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u8(in, &ld->emergency_profile_active))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 10U:
            return limiter_set_group_id_list(ld, in);

        case 11U:
            return limiter_set_action_over(ld, in);

        default:
            break;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            /* reset: clear limiter state */
            ld->over_threshold = 0U;
            ld->under_threshold = 0U;
            return CSM_OK;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_limiter = {
    .class_id  = 71,
    .name      = "Limiter",
    .version   = 0,
    .create    = limiter_create,
    .dispatch  = limiter_dispatch
};

void db_ic_register_limiter(void)
{
    db_ic_register(&ic_limiter);
}
