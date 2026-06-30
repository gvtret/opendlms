/**
 * Object list encode/decode for Association LN attribute 2
 *
 * Encodes all registered instances as an AXDR array of structures:
 *   ARRAY of SEQUENCE {
 *       class_id         Unsigned16,
 *       logical_name     Octet-string(6),
 *       version          Integer8,
 *       access_rights    SEQUENCE { ... }
 *   }
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_model_object_list.h"
#include "csm_model_instance.h"
#include "csm_config.h"
#include "csm_services.h"
#include "csm_axdr_codec.h"
#include <string.h>

int csm_model_export_object_list(csm_array *out)
{
    if (out == NULL)
    {
        return FALSE;
    }

    int count = csm_model_instance_count();
    int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
    valid = valid && csm_array_write_u8(out, (uint8_t)count);

    for (int i = 0; i < count && valid; i++)
    {
        const csm_object_t *obj = csm_model_instance_get(i);
        if (obj == NULL)
        {
            continue;
        }

        valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
        valid = valid && csm_array_write_u8(out, 4U);

        valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
        valid = valid && csm_array_write_u16(out, obj->class_id);

        valid = valid && csm_axdr_wr_octetstring(out, (const uint8_t *)&obj->obis, 6U);

        valid = valid && csm_array_write_u8(out, AXDR_TAG_INTEGER8);
        valid = valid && csm_array_write_u8(out, (uint8_t)(int8_t)obj->version);

        valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
        valid = valid && csm_array_write_u8(out, 0U);
    }

    return valid ? (int)CSM_OK : (int)CSM_ERR_BAD_ENCODING;
}

int csm_model_import_object_list(csm_array *in)
{
    if (in == NULL)
    {
        return FALSE;
    }

    uint8_t tag = 0xFFU;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }

    uint8_t count = 0U;
    if (!csm_array_read_u8(in, &count))
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }

    csm_model_instance_reset();

    for (uint8_t i = 0U; i < count; i++)
    {
        uint8_t stag = 0xFFU;
        uint8_t fields = 0U;
        if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE)
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &fields) || fields != 4U)
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }

        uint16_t class_id = 0U;
        uint8_t ctag = 0xFFU;
        if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16)
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u16(in, &class_id))
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }

        csm_obis_code obis;
        memset(&obis, 0, sizeof(obis));
        uint8_t otag = 0xFFU;
        uint8_t olen = 0U;
        if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING)
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &olen) || olen != 6U)
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }
        {
            uint8_t ob[6] = {0U};
            if (!csm_array_read_buff(in, ob, 6U))
            {
                csm_model_instance_reset();
                return (int)CSM_ERR_BAD_ENCODING;
            }
            obis.A = ob[0];
            obis.B = ob[1];
            obis.C = ob[2];
            obis.D = ob[3];
            obis.E = ob[4];
            obis.F = ob[5];
        }

        uint8_t version = 0U;
        uint8_t vtag = 0xFFU;
        if (!csm_array_read_u8(in, &vtag) || vtag != AXDR_TAG_INTEGER8)
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &version))
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }

        uint8_t access_tag = 0xFFU;
        uint8_t access_fields = 0U;
        if (!csm_array_read_u8(in, &access_tag) || access_tag != AXDR_TAG_STRUCTURE)
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }
        if (!csm_array_read_u8(in, &access_fields) || access_fields != 0U)
        {
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }

        if (!csm_model_instance_add(class_id, &obis, version))
        {
            CSM_ERR("[OBJLIST] Failed to add instance %u", i);
            csm_model_instance_reset();
            return (int)CSM_ERR_BAD_ENCODING;
        }
    }

    if (csm_array_unread(in) != 0U)
    {
        csm_model_instance_reset();
        return (int)CSM_ERR_BAD_ENCODING;
    }

    return (int)CSM_OK;
}
