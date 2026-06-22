/**
 * Capture object helpers for Profile Generic
 *
 * Encode/decode a capture object definition as:
 *   SEQUENCE(3) {
 *       class_id     Unsigned16,
 *       logical_name Octet-string(6),
 *       attribute_id Unsigned8
 *   }
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_profile_capture.h"
#include "csm_config.h"
#include "csm_services.h"
#include "csm_axdr_codec.h"
#include <string.h>

int csm_profile_capture_encode(const csm_object_t *obj, csm_array *out)
{
    if (obj == NULL || out == NULL)
    {
        return FALSE;
    }

    int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
    valid = valid && csm_array_write_u8(out, 3U);

    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
    valid = valid && csm_array_write_u16(out, obj->class_id);

    valid = valid && csm_axdr_wr_octetstring(out, (const uint8_t *)&obj->obis, 6U);

    valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
    valid = valid && csm_array_write_u8(out, (uint8_t)(obj->id & 0xFF));

    return valid ? (int)CSM_OK : (int)CSM_ERR_BAD_ENCODING;
}

int csm_profile_capture_decode(csm_array *in, csm_object_t *obj)
{
    if (in == NULL || obj == NULL)
    {
        return FALSE;
    }

    uint8_t stag = 0xFFU;
    uint8_t fields = 0U;
    if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &fields) || fields < 3U)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }

    uint8_t ctag = 0xFFU;
    if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u16(in, &obj->class_id))
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }

    uint8_t otag = 0xFFU;
    uint8_t olen = 0U;
    if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }
    if (!csm_array_read_u8(in, &olen) || olen != 6U)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }
    {
        uint8_t ob[6] = {0U};
        if (!csm_array_read_buff(in, ob, 6U))
        {
            return (int)CSM_ERR_BAD_ENCODING;
        }
        obj->obis.A = ob[0];
        obj->obis.B = ob[1];
        obj->obis.C = ob[2];
        obj->obis.D = ob[3];
        obj->obis.E = ob[4];
        obj->obis.F = ob[5];
    }

    uint8_t atag = 0xFFU;
    if (!csm_array_read_u8(in, &atag) || atag != AXDR_TAG_UNSIGNED8)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }
    uint8_t attr_id = 0U;
    if (!csm_array_read_u8(in, &attr_id))
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }
    obj->id = (int8_t)attr_id;

    return (int)CSM_OK;
}
