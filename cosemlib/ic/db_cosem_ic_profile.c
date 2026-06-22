/**
 * DLMS/COSEM Profile Generic Interface Class handler (class_id = 7)
 *
 * Per IEC 62056-6-2 ED4 / Blue Book 4.3.6:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: buffer (array of structures, dynamic)
 * - Attr 3: capture_objects (array of structures, static)
 * - Attr 4: capture_period (unsigned32, static)
 * - Attr 5: sort_method (enum, static)
 * - Attr 6: sort_object (structure, static)
 * - Attr 7: number_of_entries (unsigned32, dynamic)
 * - Attr 8: entries_in_use (unsigned32, dynamic)
 * - Method 1: reset (data)
 * - Method 2: capture (data)
 * - Method 3: read_entries_by_range (access-selector)
 * - Method 4: read_entries_by_index (access-selector)
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

#define PG_MAX_INSTANCES    4U
#define PG_MAX_CAPTURE      8U
#define PG_MAX_ENTRIES      256U
#define PG_VALUE_SIZE       64U
#define PG_DATETIME_LEN     12U
#define PG_TAG_FLOAT32      23U
#define PG_TAG_FLOAT64      24U

/* Sort method enumeration */
#define PG_SORT_FIFO            0U
#define PG_SORT_LIFO            1U
#define PG_SORT_BY_TIMESTAMP    2U
#define PG_SORT_BY_DATAVALUE    3U

typedef struct {
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t attribute_id;
} db_ic_profile_capture_obj;

typedef struct {
    uint8_t datetime[PG_DATETIME_LEN];
    uint8_t values[PG_MAX_CAPTURE][PG_VALUE_SIZE];
    uint8_t value_len[PG_MAX_CAPTURE];
    uint8_t value_count;
} db_ic_profile_entry;

typedef struct {
    db_ic_profile_capture_obj captures[PG_MAX_CAPTURE];
    uint8_t capture_count;
    uint32_t capture_period;
    uint8_t sort_method;
    db_ic_profile_capture_obj sort_object;
    db_ic_profile_entry entries[PG_MAX_ENTRIES];
    uint16_t entry_count;
    uint16_t entries_in_use;
    uint16_t write_index;
} db_ic_profile_data;

static db_ic_profile_data pg_pool[PG_MAX_INSTANCES];
static uint8_t pg_pool_count = 0U;

static db_ic_inst_t pg_inst_tmp;
static db_ic_inst_t pg_inst_pool[PG_MAX_INSTANCES];
static uint8_t pg_inst_count = 0U;

static const db_ic_attr_descr pg_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET,                  2, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_ARRAY },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  5, AXDR_TAG_ENUM },
    { DB_ACCESS_GET | DB_ACCESS_SET,  6, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET,                  7, AXDR_TAG_UNSIGNED32 },
    { DB_ACCESS_GET,                  8, AXDR_TAG_UNSIGNED32 },
};

static const db_ic_method_descr pg_methods[] = {
    { DB_ACCESS_ACTION, 1, AXDR_TAG_NULL },
    { DB_ACCESS_ACTION, 2, AXDR_TAG_NULL },
    { DB_ACCESS_ACTION, 3, AXDR_TAG_NULL },
    { DB_ACCESS_ACTION, 4, AXDR_TAG_NULL },
};

static const db_ic_object_descr pg_descr = {
    .attributes   = pg_attrs,
    .methods      = pg_methods,
    .class_id     = 7,
    .obis         = { 0, 0, 0, 0, 0, 0 },
    .attr_count   = 8,
    .method_count = 4,
    .version      = 1
};

static uint8_t pg_axdr_fixed_size(uint8_t tag)
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
    case PG_TAG_FLOAT32:      return 4U;
    case PG_TAG_FLOAT64:      return 8U;
    default:                  return 0U;
    }
}

static int pg_axdr_is_length_coded(uint8_t tag)
{
    return (tag == AXDR_TAG_OCTETSTRING) ||
           (tag == AXDR_TAG_VISIBLESTRING) ||
           (tag == AXDR_TAG_UTF8_STRING) ||
           (tag == AXDR_TAG_BITSTRING);
}

static int pg_read_axdr_value(csm_array *in, uint8_t *buf, uint8_t *len, uint8_t max_len)
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
    else if (tag == AXDR_TAG_ARRAY || tag == AXDR_TAG_STRUCTURE)
    {
        uint8_t count = 0U;
        if (!csm_array_read_u8(in, &count))
        {
            return FALSE;
        }
        buf[1] = count;
        *len = 2U;
        for (uint8_t i = 0U; i < count; i++)
        {
            uint8_t elem_buf[PG_VALUE_SIZE];
            uint8_t elem_len = 0U;
            if (!pg_read_axdr_value(in, elem_buf, &elem_len, PG_VALUE_SIZE - (*len)))
            {
                return FALSE;
            }
            if ((uint16_t)(*len + elem_len) > max_len)
            {
                return FALSE;
            }
            memcpy(&buf[*len], elem_buf, elem_len);
            *len += elem_len;
        }
        return TRUE;
    }
    else if (pg_axdr_is_length_coded(tag))
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
        uint8_t dsize = pg_axdr_fixed_size(tag);
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

static void pg_init_datetime(uint8_t *dt)
{
    memset(dt, 0xFFU, PG_DATETIME_LEN);
}

static db_ic_inst_t *pg_create(const csm_obis_code *obis)
{
    (void) obis;

    if (pg_pool_count >= PG_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_profile_data *pg = &pg_pool[pg_pool_count];
    memset(pg, 0, sizeof(db_ic_profile_data));
    pg->capture_period = 0U;
    pg->sort_method = PG_SORT_FIFO;
    pg->entry_count = 0U;
    pg->entries_in_use = 0U;
    pg->write_index = 0U;
    pg_pool_count++;

    db_ic_inst_t *inst = &pg_inst_tmp;
    memset(inst, 0, sizeof(db_ic_inst_t));
    inst->descr   = &pg_descr;
    inst->data    = pg;
    inst->version = 1U;
    return inst;
}

static int pg_encode_logical_name(const db_ic_inst_t *inst, csm_array *out)
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

static int pg_encode_capture_obj(csm_array *out,
                                  const db_ic_profile_capture_obj *obj)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
    valid = valid && csm_array_write_u8(out, 3U);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
    valid = valid && csm_array_write_u16(out, obj->class_id);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
    valid = valid && csm_array_write_u8(out, 6U);
    valid = valid && csm_array_write_u8(out, obj->obis.A);
    valid = valid && csm_array_write_u8(out, obj->obis.B);
    valid = valid && csm_array_write_u8(out, obj->obis.C);
    valid = valid && csm_array_write_u8(out, obj->obis.D);
    valid = valid && csm_array_write_u8(out, obj->obis.E);
    valid = valid && csm_array_write_u8(out, obj->obis.F);
    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
    valid = valid && csm_array_write_u8(out, obj->attribute_id);
    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code pg_get_buffer(const db_ic_profile_data *pg, csm_array *out)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
    valid = valid && csm_array_write_u8(out, (uint8_t)pg->entries_in_use);

    for (uint16_t i = 0U; i < pg->entries_in_use && valid; i++)
    {
        uint16_t idx;
        if (pg->entry_count >= PG_MAX_ENTRIES)
        {
            idx = (pg->write_index + i) % PG_MAX_ENTRIES;
        }
        else
        {
            idx = i;
        }

        const db_ic_profile_entry *entry = &pg->entries[idx];
        uint8_t field_count = 1U + entry->value_count;

        valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
        valid = valid && csm_array_write_u8(out, field_count);
        valid = valid && csm_array_write_buff(out, entry->datetime, PG_DATETIME_LEN);

        for (uint8_t j = 0U; j < entry->value_count && valid; j++)
        {
            valid = valid && csm_array_write_buff(out, entry->values[j],
                                                    entry->value_len[j]);
        }
    }

    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code pg_get_capture_objects(const db_ic_profile_data *pg,
                                           csm_array *out)
{
    int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
    valid = valid && csm_array_write_u8(out, pg->capture_count);

    for (uint8_t i = 0U; i < pg->capture_count && valid; i++)
    {
        valid = (pg_encode_capture_obj(out, &pg->captures[i]) == CSM_OK);
    }

    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code pg_set_capture_objects(db_ic_profile_data *pg, csm_array *in)
{
    uint8_t tag = 0xFFU;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    uint8_t count = 0U;
    if (!csm_array_read_u8(in, &count) || count > PG_MAX_CAPTURE)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    pg->capture_count = count;

    for (uint8_t i = 0U; i < count; i++)
    {
        uint8_t stag = 0xFFU;
        uint8_t fields = 0U;
        if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &fields) || fields < 3U)
        {
            return CSM_ERR_BAD_ENCODING;
        }

        /* class_id: Unsigned16 */
        uint8_t ctag = 0xFFU;
        if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u16(in, &pg->captures[i].class_id))
        {
            return CSM_ERR_BAD_ENCODING;
        }

        /* logical_name: Octet-string(6) */
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
            pg->captures[i].obis.A = ob[0];
            pg->captures[i].obis.B = ob[1];
            pg->captures[i].obis.C = ob[2];
            pg->captures[i].obis.D = ob[3];
            pg->captures[i].obis.E = ob[4];
            pg->captures[i].obis.F = ob[5];
        }

        /* attribute_id: Unsigned8 */
        uint8_t atag = 0xFFU;
        if (!csm_array_read_u8(in, &atag) || atag != AXDR_TAG_UNSIGNED8)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &pg->captures[i].attribute_id))
        {
            return CSM_ERR_BAD_ENCODING;
        }
    }

    return CSM_OK;
}

static csm_db_code pg_set_sort_object(db_ic_profile_data *pg, csm_array *in)
{
    uint8_t tag = 0xFFU;
    uint8_t fields = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &fields) || fields < 3U)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    uint8_t ctag = 0xFFU;
    if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u16(in, &pg->sort_object.class_id))
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
        pg->sort_object.obis.A = ob[0];
        pg->sort_object.obis.B = ob[1];
        pg->sort_object.obis.C = ob[2];
        pg->sort_object.obis.D = ob[3];
        pg->sort_object.obis.E = ob[4];
        pg->sort_object.obis.F = ob[5];
    }

    uint8_t atag = 0xFFU;
    if (!csm_array_read_u8(in, &atag) || atag != AXDR_TAG_UNSIGNED8)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &pg->sort_object.attribute_id))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    return CSM_OK;
}

static csm_db_code pg_read_entries_by_range(db_ic_profile_data *pg,
                                              csm_array *in, csm_array *out)
{
    uint8_t tag = 0xFFU;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    uint8_t fields = 0U;
    if (!csm_array_read_u8(in, &fields))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    /* field 0: access_selector (Boolean) */
    uint8_t sel_tag = 0xFFU;
    uint8_t sel_val = 0U;
    if (!csm_array_read_u8(in, &sel_tag) || sel_tag != AXDR_TAG_BOOLEAN)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &sel_val))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    if (!sel_val)
    {
        return pg_get_buffer(pg, out);
    }

    /* field 1: parameter — structure {restricting_object, from_value, to_value} */
    uint8_t ptag = 0xFFU;
    if (!csm_array_read_u8(in, &ptag) || ptag != AXDR_TAG_STRUCTURE)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    uint8_t pfields = 0U;
    if (!csm_array_read_u8(in, &pfields) || pfields < 3U)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    /* restricting_object: {class_id, obis, attribute_id} — skip it */
    {
        uint8_t rtag = 0xFFU;
        if (!csm_array_read_u8(in, &rtag) || rtag != AXDR_TAG_STRUCTURE)
        {
            return CSM_ERR_BAD_ENCODING;
        }
        uint8_t rflds = 0U;
        if (!csm_array_read_u8(in, &rflds))
        {
            return CSM_ERR_BAD_ENCODING;
        }
        for (uint8_t r = 0U; r < rflds; r++)
        {
            uint8_t dummy_tag = 0xFFU;
            if (!csm_array_read_u8(in, &dummy_tag))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (dummy_tag == AXDR_TAG_NULL)
            {
                continue;
            }
            else if (pg_axdr_is_length_coded(dummy_tag))
            {
                uint8_t dlen = 0U;
                if (!csm_array_read_u8(in, &dlen))
                {
                    return CSM_ERR_BAD_ENCODING;
                }
                if (dlen > 0U)
                {
                    if (!csm_array_reader_jump(in, dlen))
                    {
                        return CSM_ERR_BAD_ENCODING;
                    }
                }
            }
            else
            {
                uint8_t dsz = pg_axdr_fixed_size(dummy_tag);
                if (dsz > 0U)
                {
                    if (!csm_array_reader_jump(in, dsz))
                    {
                        return CSM_ERR_BAD_ENCODING;
                    }
                }
            }
        }
    }

    /* from_value: date-time (12 bytes) */
    uint8_t from_dt[PG_DATETIME_LEN];
    {
        uint8_t dt_tag = 0xFFU;
        uint8_t dt_len = 0U;
        if (!csm_array_read_u8(in, &dt_tag))
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (dt_tag == 25U)
        {
            if (!csm_array_read_u8(in, &dt_len) || dt_len != PG_DATETIME_LEN)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_buff(in, from_dt, PG_DATETIME_LEN))
            {
                return CSM_ERR_BAD_ENCODING;
            }
        }
        else
        {
            memset(from_dt, 0xFFU, PG_DATETIME_LEN);
        }
    }

    /* to_value: date-time (12 bytes) */
    uint8_t to_dt[PG_DATETIME_LEN];
    {
        uint8_t dt_tag = 0xFFU;
        uint8_t dt_len = 0U;
        if (!csm_array_read_u8(in, &dt_tag))
        {
            return CSM_ERR_BAD_ENCODING;
        }
        if (dt_tag == 25U)
        {
            if (!csm_array_read_u8(in, &dt_len) || dt_len != PG_DATETIME_LEN)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_buff(in, to_dt, PG_DATETIME_LEN))
            {
                return CSM_ERR_BAD_ENCODING;
            }
        }
        else
        {
            memset(to_dt, 0xFFU, PG_DATETIME_LEN);
        }
    }

    /* Collect matching entries */
    uint16_t match_count = 0U;

    int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
    valid = valid && csm_array_writer_jump(out, 1U);

    for (uint16_t i = 0U; i < pg->entries_in_use && valid; i++)
    {
        uint16_t idx;
        if (pg->entry_count >= PG_MAX_ENTRIES)
        {
            idx = (pg->write_index + i) % PG_MAX_ENTRIES;
        }
        else
        {
            idx = i;
        }

        const db_ic_profile_entry *entry = &pg->entries[idx];

        /* Compare datetime: from_dt <= entry->datetime <= to_dt */
        int ge_from = (memcmp(entry->datetime, from_dt, PG_DATETIME_LEN) >= 0);
        int le_to   = (memcmp(entry->datetime, to_dt, PG_DATETIME_LEN) <= 0);

        if (ge_from && le_to)
        {
            uint8_t field_count = 1U + entry->value_count;
            valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_u8(out, field_count);
            valid = valid && csm_array_write_buff(out, entry->datetime, PG_DATETIME_LEN);
            for (uint8_t j = 0U; j < entry->value_count && valid; j++)
            {
                valid = valid && csm_array_write_buff(out, entry->values[j],
                                                        entry->value_len[j]);
            }
            match_count++;
        }
    }

    /* Patch the array count byte at offset 1 */
    if (valid)
    {
        (void) csm_array_set(out, 1, (uint8_t)match_count);
    }

    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code pg_read_entries_by_index(db_ic_profile_data *pg,
                                              csm_array *in, csm_array *out)
{
    uint8_t tag = 0xFFU;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    uint8_t fields = 0U;
    if (!csm_array_read_u8(in, &fields))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    /* field 0: access_selector (Boolean) */
    uint8_t sel_tag = 0xFFU;
    uint8_t sel_val = 0U;
    if (!csm_array_read_u8(in, &sel_tag) || sel_tag != AXDR_TAG_BOOLEAN)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &sel_val))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    if (!sel_val)
    {
        return pg_get_buffer(pg, out);
    }

    /* field 1: parameter — structure {from_index, to_index} */
    uint8_t ptag = 0xFFU;
    if (!csm_array_read_u8(in, &ptag) || ptag != AXDR_TAG_STRUCTURE)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    uint8_t pfields = 0U;
    if (!csm_array_read_u8(in, &pfields) || pfields < 2U)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    /* from_index: Unsigned16 */
    uint8_t ftag = 0xFFU;
    if (!csm_array_read_u8(in, &ftag) || ftag != AXDR_TAG_UNSIGNED16)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    uint16_t from_idx = 0U;
    if (!csm_array_read_u16(in, &from_idx))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    /* to_index: Unsigned16 */
    uint8_t ttag = 0xFFU;
    if (!csm_array_read_u8(in, &ttag) || ttag != AXDR_TAG_UNSIGNED16)
    {
        return CSM_ERR_BAD_ENCODING;
    }
    uint16_t to_idx = 0U;
    if (!csm_array_read_u16(in, &to_idx))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    if (from_idx > to_idx || to_idx >= pg->entries_in_use)
    {
        return CSM_ERR_BAD_ENCODING;
    }

    uint16_t count = to_idx - from_idx + 1U;
    int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
    valid = valid && csm_array_write_u8(out, (uint8_t)count);

    for (uint16_t i = from_idx; i <= to_idx && valid; i++)
    {
        const db_ic_profile_entry *entry = &pg->entries[i];
        uint8_t field_count = 1U + entry->value_count;

        valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
        valid = valid && csm_array_write_u8(out, field_count);
        valid = valid && csm_array_write_buff(out, entry->datetime, PG_DATETIME_LEN);

        for (uint8_t j = 0U; j < entry->value_count && valid; j++)
        {
            valid = valid && csm_array_write_buff(out, entry->values[j],
                                                    entry->value_len[j]);
        }
    }

    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static void pg_add_entry(db_ic_profile_data *pg, const db_ic_profile_entry *new_entry)
{
    pg->entries[pg->write_index] = *new_entry;
    pg->write_index = (pg->write_index + 1U) % PG_MAX_ENTRIES;
    if (pg->entry_count < PG_MAX_ENTRIES)
    {
        pg->entry_count++;
    }
    pg->entries_in_use = pg->entry_count;
}

static csm_db_code pg_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                 uint8_t attr_id, uint8_t method_id,
                                 csm_array *in, csm_array *out)
{
    (void) method_id;

    if ((inst == NULL) || (inst->data == NULL))
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

    db_ic_profile_data *pg = (db_ic_profile_data *)inst->data;

    if (op == IC_OP_GET)
    {
        switch (attr_id)
        {
        case 1U:
            return pg_encode_logical_name(inst, out);

        case 2U:
            return pg_get_buffer(pg, out);

        case 3U:
            return pg_get_capture_objects(pg, out);

        case 4U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, pg->capture_period);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 5U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
            valid = valid && csm_array_write_u8(out, pg->sort_method);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 6U:
            return pg_encode_capture_obj(out, &pg->sort_object);

        case 7U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, (uint32_t)PG_MAX_ENTRIES);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        case 8U:
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
            valid = valid && csm_array_write_u32(out, (uint32_t)pg->entries_in_use);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }

        default:
            break;
        }
    }
    else if (op == IC_OP_SET)
    {
        switch (attr_id)
        {
        case 3U:
            return pg_set_capture_objects(pg, in);

        case 4U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u32(in, &pg->capture_period))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 5U:
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ENUM)
            {
                return CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u8(in, &pg->sort_method))
            {
                return CSM_ERR_BAD_ENCODING;
            }
            return CSM_OK;
        }

        case 6U:
            return pg_set_sort_object(pg, in);

        default:
            break;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        switch (method_id)
        {
        case 1U:
            /* reset: clear buffer */
            pg->entry_count = 0U;
            pg->entries_in_use = 0U;
            pg->write_index = 0U;
            return CSM_OK;

        case 2U:
        {
            /* capture: store a new entry with null values */
            db_ic_profile_entry entry;
            pg_init_datetime(entry.datetime);
            entry.value_count = pg->capture_count;
            for (uint8_t i = 0U; i < pg->capture_count; i++)
            {
                entry.values[i][0] = AXDR_TAG_NULL;
                entry.value_len[i] = 1U;
            }
            for (uint8_t i = pg->capture_count; i < PG_MAX_CAPTURE; i++)
            {
                entry.values[i][0] = AXDR_TAG_NULL;
                entry.value_len[i] = 1U;
            }
            pg_add_entry(pg, &entry);
            return CSM_OK;
        }

        case 3U:
            return pg_read_entries_by_range(pg, in, out);

        case 4U:
            return pg_read_entries_by_index(pg, in, out);

        default:
            break;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_pg = {
    .class_id  = 7,
    .name      = "Profile Generic",
    .version   = 1,
    .create    = pg_create,
    .dispatch  = pg_dispatch
};

void db_ic_register_profile_generic(void)
{
    db_ic_register(&ic_pg);
}
