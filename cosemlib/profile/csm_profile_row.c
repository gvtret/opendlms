/**
 * Profile row helpers for Profile Generic
 *
 * Encode/decode a profile buffer row as:
 *   SEQUENCE(n) {
 *       value[0],   // Typically date-time (AXDR tag + 12 bytes)
 *       value[1],   // Capture object value (AXDR encoded)
 *       ...
 *   }
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_profile_row.h"
#include "csm_axdr_codec.h"
#include "csm_config.h"
#include "csm_services.h"
#include <string.h>

int csm_profile_row_encode(const uint8_t *values, uint8_t count, csm_array *out)
{
    if (values == NULL || out == NULL || count == 0U)
    {
        return FALSE;
    }

    int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
    valid = valid && csm_array_write_u8(out, count);

    const uint8_t *p = values;
    for (uint8_t i = 0U; i < count && valid; i++)
    {
        uint8_t tag = *p++;
        valid = valid && csm_array_write_u8(out, tag);

        switch (tag)
        {
        case AXDR_TAG_NULL:
            break;

        case AXDR_TAG_BOOLEAN:
        case AXDR_TAG_INTEGER8:
        case AXDR_TAG_UNSIGNED8:
        case AXDR_TAG_BCD:
        case AXDR_TAG_ENUM:
            valid = valid && csm_array_write_u8(out, *p);
            p++;
            break;

        case AXDR_TAG_INTEGER16:
        case AXDR_TAG_UNSIGNED16:
            valid = valid && csm_array_write_u8(out, *p);
            valid = valid && csm_array_write_u8(out, *(p + 1));
            p += 2;
            break;

        case AXDR_TAG_INTEGER32:
        case AXDR_TAG_UNSIGNED32:
            valid = valid && csm_array_write_u8(out, *p);
            valid = valid && csm_array_write_u8(out, *(p + 1));
            valid = valid && csm_array_write_u8(out, *(p + 2));
            valid = valid && csm_array_write_u8(out, *(p + 3));
            p += 4;
            break;

        case AXDR_TAG_INTEGER64:
        case AXDR_TAG_UNSIGNED64:
            valid = valid && csm_array_write_buff(out, p, 8U);
            p += 8;
            break;

        case AXDR_TAG_OCTETSTRING:
        case AXDR_TAG_VISIBLESTRING:
        case AXDR_TAG_UTF8_STRING:
        {
            uint8_t len = *p++;
            valid = valid && csm_array_write_u8(out, len);
            if (len > 0U)
            {
                valid = valid && csm_array_write_buff(out, p, len);
                p += len;
            }
            break;
        }

        default:
            valid = FALSE;
            break;
        }
    }

    return valid ? (int)CSM_OK : (int)CSM_ERR_BAD_ENCODING;
}

int csm_profile_row_decode(csm_array *in, uint8_t *values, uint8_t *count)
{
    if (in == NULL || values == NULL || count == NULL)
    {
        return FALSE;
    }

    uint8_t stag = 0xFFU;
    if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }

    uint8_t field_count = 0U;
    if (!csm_array_read_u8(in, &field_count) || field_count > CSM_PROFILE_ROW_MAX_VALUES)
    {
        return (int)CSM_ERR_BAD_ENCODING;
    }

    uint8_t *p = values;
    uint16_t remaining = csm_array_unread(in);

    for (uint8_t i = 0U; i < field_count; i++)
    {
        if (remaining < 1U)
        {
            return (int)CSM_ERR_BAD_ENCODING;
        }

        uint8_t tag = 0xFFU;
        if (!csm_array_read_u8(in, &tag))
        {
            return (int)CSM_ERR_BAD_ENCODING;
        }
        remaining--;

        *p++ = tag;

        switch (tag)
        {
        case AXDR_TAG_NULL:
            break;

        case AXDR_TAG_BOOLEAN:
        case AXDR_TAG_INTEGER8:
        case AXDR_TAG_UNSIGNED8:
        case AXDR_TAG_BCD:
        case AXDR_TAG_ENUM:
        {
            uint8_t val = 0U;
            if (!csm_array_read_u8(in, &val) || remaining < 1U)
            {
                return (int)CSM_ERR_BAD_ENCODING;
            }
            *p++ = val;
            remaining--;
            break;
        }

        case AXDR_TAG_INTEGER16:
        case AXDR_TAG_UNSIGNED16:
        {
            if (remaining < 2U)
            {
                return (int)CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u8(in, &p[0]))
            {
                return (int)CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_u8(in, &p[1]))
            {
                return (int)CSM_ERR_BAD_ENCODING;
            }
            p += 2;
            remaining -= 2;
            break;
        }

        case AXDR_TAG_INTEGER32:
        case AXDR_TAG_UNSIGNED32:
        {
            if (remaining < 4U)
            {
                return (int)CSM_ERR_BAD_ENCODING;
            }
            for (uint8_t b = 0U; b < 4U; b++)
            {
                if (!csm_array_read_u8(in, &p[b]))
                {
                    return (int)CSM_ERR_BAD_ENCODING;
                }
            }
            p += 4;
            remaining -= 4;
            break;
        }

        case AXDR_TAG_INTEGER64:
        case AXDR_TAG_UNSIGNED64:
        {
            if (remaining < 8U)
            {
                return (int)CSM_ERR_BAD_ENCODING;
            }
            if (!csm_array_read_buff(in, p, 8U))
            {
                return (int)CSM_ERR_BAD_ENCODING;
            }
            p += 8;
            remaining -= 8;
            break;
        }

        case AXDR_TAG_OCTETSTRING:
        case AXDR_TAG_VISIBLESTRING:
        case AXDR_TAG_UTF8_STRING:
        {
            uint8_t len = 0U;
            if (!csm_array_read_u8(in, &len) || remaining < (uint16_t)(1U + len))
            {
                return (int)CSM_ERR_BAD_ENCODING;
            }
            *p++ = len;
            remaining--;
            if (len > 0U)
            {
                if (!csm_array_read_buff(in, p, len))
                {
                    return (int)CSM_ERR_BAD_ENCODING;
                }
                p += len;
                remaining -= len;
            }
            break;
        }

        default:
            return (int)CSM_ERR_BAD_ENCODING;
        }
    }

    *count = field_count;
    return (int)CSM_OK;
}
