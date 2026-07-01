/**
 * AXDR utility function to serialize data
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_axdr_codec.h"
#include "csm_ber.h"

// -------------------------------   DECODERS   ------------------------------------------

int csm_axdr_rd_null(csm_array *array)
{
    int ret = FALSE;
    uint8_t byte = 0xFFU;
    uint32_t saved_rd_index = 0U;
    if (array == NULL)
    {
        return FALSE;
    }

    saved_rd_index = array->rd_index;
    if (csm_array_read_u8(array, &byte))
    {
        if (byte == AXDR_TAG_NULL)
        {
            ret = TRUE;
        }
    }
    if (ret == FALSE)
    {
        array->rd_index = saved_rd_index;
    }
    return ret;
}

int csm_axdr_size(csm_array *array, uint32_t *size)
{
    int ret = FALSE;

    if ((array == NULL) || (size == NULL))
    {
        return FALSE;
    }

    ber_length len;
    if (!csm_ber_read_len(array, &len))
    {
        return FALSE;
    }

    // Check if size is somewhat possible
    if (len.length <= csm_array_unread(array))
    {
        *size = len.length;
        ret = TRUE;
    }
    return ret;
}

int csm_axdr_rd_octetstring(csm_array *array, uint32_t *size)
{
    int ret = FALSE;
    uint8_t byte = 0xFFU;
    uint32_t saved_rd_index = 0U;
    if ((array == NULL) || (size == NULL))
    {
        return FALSE;
    }

    saved_rd_index = array->rd_index;
    if (csm_array_read_u8(array, &byte))
    {
        if (byte == AXDR_TAG_OCTETSTRING)
        {
            ret = csm_axdr_size(array, size);
        }
    }
    if (ret == FALSE)
    {
        array->rd_index = saved_rd_index;
    }
    return ret;
}

typedef enum
{
    AXDR_SIZE_NONE = 0,
    AXDR_SIZE_1 = 1,
    AXDR_SIZE_2 = 2,
    AXDR_SIZE_4 = 4,
    AXDR_SIZE_5 = 5,
    AXDR_SIZE_8 = 8,
    AXDR_SIZE_12 = 12,
    AXDR_SIZE_CODED,
} axdr_size_t;

typedef struct
{
    uint8_t tag;
    uint8_t is_struct;
    axdr_size_t size;
} tag_t;

static const tag_t tags[] = {
        { AXDR_TAG_NULL,            0, AXDR_SIZE_NONE},
        { AXDR_TAG_ARRAY,           1, AXDR_SIZE_CODED},
        { AXDR_TAG_STRUCTURE,       1, AXDR_SIZE_CODED},
        { AXDR_TAG_BOOLEAN,         0, AXDR_SIZE_1},
        { AXDR_TAG_BITSTRING,       0, AXDR_SIZE_CODED},
        { AXDR_TAG_INTEGER32,       0, AXDR_SIZE_4},
        { AXDR_TAG_UNSIGNED32,      0, AXDR_SIZE_4},
        { AXDR_TAG_OCTETSTRING,     0, AXDR_SIZE_CODED},
        { AXDR_TAG_VISIBLESTRING,   0, AXDR_SIZE_CODED},
        { AXDR_TAG_UTF8_STRING,     0, AXDR_SIZE_CODED},
        { AXDR_TAG_BCD,             0, AXDR_SIZE_1},
        { AXDR_TAG_INTEGER8,        0, AXDR_SIZE_1},
        { AXDR_TAG_INTEGER16,       0, AXDR_SIZE_2},
        { AXDR_TAG_UNSIGNED8,       0, AXDR_SIZE_1} ,
        { AXDR_TAG_UNSIGNED16,      0, AXDR_SIZE_2},
        { AXDR_TAG_INTEGER64,       0, AXDR_SIZE_8},
        { AXDR_TAG_UNSIGNED64,      0, AXDR_SIZE_8},
        { AXDR_TAG_ENUM,            0, AXDR_SIZE_1},
        { AXDR_TAG_FLOAT32,         0, AXDR_SIZE_4},
        { AXDR_TAG_FLOAT64,         0, AXDR_SIZE_8},
        { AXDR_TAG_DATE_TIME,       0, AXDR_SIZE_12},
        { AXDR_TAG_DATE,            0, AXDR_SIZE_5},
        { AXDR_TAG_TIME,            0, AXDR_SIZE_4},
        { AXDR_TAG_DONT_CARE,       0, AXDR_SIZE_NONE}
};

static const uint32_t tags_size = sizeof(tags) / sizeof(tags[0]);
#define CSM_AXDR_MAX_NESTING 32U

static const tag_t *csm_axdr_find_tag(uint8_t tag)
{
    for (uint32_t i = 0U; i < tags_size; i++)
    {
        if (tags[i].tag == tag)
        {
            return &tags[i];
        }
    }
    return NULL;
}

static int csm_axdr_decode_one(csm_array *array, axdr_data_cb callback, uint32_t depth)
{
    uint8_t tag = 0xFFU;
    if (depth > CSM_AXDR_MAX_NESTING)
    {
        return FALSE;
    }

    if (!csm_array_read_u8(array, &tag))
    {
        return FALSE;
    }

    const tag_t *descriptor = csm_axdr_find_tag(tag);
    if (descriptor == NULL)
    {
        return FALSE;
    }

    uint32_t size = descriptor->size;
    if (descriptor->size == AXDR_SIZE_CODED)
    {
        if (!csm_axdr_size(array, &size))
        {
            return FALSE;
        }
    }

    if (descriptor->is_struct)
    {
        if (callback != NULL)
        {
            callback(tag, size, csm_array_rd_data(array));
        }

        for (uint32_t i = 0U; i < size; i++)
        {
            if (!csm_axdr_decode_one(array, callback, depth + 1U))
            {
                return FALSE;
            }
        }
        return TRUE;
    }

    if (size == 0U)
    {
        if (callback != NULL)
        {
            callback(tag, size, csm_array_rd_data(array));
        }
        return TRUE;
    }

    uint32_t payload_size = size;
    if (tag == AXDR_TAG_BITSTRING)
    {
        payload_size = BITFIELD_BYTES(size);
    }
    if ((csm_array_unread(array) < payload_size) ||
        ((payload_size > 0U) && (csm_array_rd_data(array) == NULL)))
    {
        return FALSE;
    }
    if (callback != NULL)
    {
        callback(tag, size, csm_array_rd_data(array));
    }
    return csm_array_reader_jump(array, payload_size);
}

int csm_axdr_decode_tags(csm_array *array, axdr_data_cb callback)
{
    if (array == NULL)
    {
        return FALSE;
    }

    while (csm_array_unread(array) > 0U)
    {
        if (!csm_axdr_decode_one(array, callback, 0U))
        {
            return FALSE;
        }
    }

    return TRUE;
}

int csm_axdr_decode_block(csm_array *array, uint32_t *size)
{
    int ret = FALSE;
    uint8_t byte = 0xFFU;
    uint32_t saved_rd_index = 0U;
    if ((array == NULL) || (size == NULL))
    {
        return FALSE;
    }

    saved_rd_index = array->rd_index;
    if (csm_array_read_u8(array, &byte))
    {
        if (byte == 0x00U)
        {
            // begin of the block
            ber_length len;
            if (!csm_ber_read_len(array, &len))
            {
                return FALSE;
            }

            // Check if size is somewhat possible
            if (len.length <= csm_array_unread(array))
            {
                *size = len.length;
                ret = TRUE;
            }
        }
    }
    if (ret == FALSE)
    {
        array->rd_index = saved_rd_index;
    }
    return ret;
}


// -------------------------------   ENCODERS ------------------------------------------
int csm_axdr_wr_octetstring(csm_array *array, const uint8_t *buffer, uint32_t size)
{
    if ((array == NULL) || ((buffer == NULL) && (size > 0U)))
    {
        return FALSE;
    }

    int valid = csm_array_write_u8(array, AXDR_TAG_OCTETSTRING);
    valid = valid && csm_ber_write_len(array, size);
    valid = valid && csm_array_write_buff(array, buffer, size);
    return valid;
}

int csm_axdr_wr_i8(csm_array *array, int8_t value)
{
    int valid = csm_array_write_u8(array, AXDR_TAG_INTEGER8);
    valid = valid && csm_array_write_u8(array, value);
    return valid;
}

int csm_axdr_wr_u16(csm_array *array, uint16_t value)
{
    int valid = csm_array_write_u8(array, AXDR_TAG_UNSIGNED16);
    valid = valid && csm_array_write_u16(array, value);
    return valid;
}

int csm_axdr_wr_boolean(csm_array *array, uint8_t value)
{
    int valid = csm_array_write_u8(array, AXDR_TAG_BOOLEAN);
    valid = valid && csm_array_write_u8(array, value);
    return valid;
}

int csm_axdr_wr_capture_object(csm_array *array, csm_object_t *data)
{
    if ((array == NULL) || (data == NULL))
    {
        return FALSE;
    }

    int valid = csm_array_write_u8(array, AXDR_TAG_STRUCTURE);
    valid = valid && csm_ber_write_len(array, 4U);

    // 1.
    valid = valid && csm_axdr_wr_u16(array, data->class_id);
    // 2.
    valid = valid && csm_axdr_wr_octetstring(array, (const uint8_t *)&data->obis.A, 6U);
    // 3.
    valid = valid && csm_axdr_wr_i8(array, data->id);
    // 4.
    valid = valid && csm_axdr_wr_u16(array, data->data_index);
    return valid;
}
