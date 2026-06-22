/**
 * DLMS/COSEM IC shared class defaults
 *
 * Default GET/SET/ACTION handlers for common IC patterns:
 *   - Default GET for logical_name (attr 1): return OBIS code
 *   - Default GET for value (attr 2): return stored value
 *   - Default SET for value: update stored value
 *   - Default ACTION: return success
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

csm_db_code db_ic_default_get_logical_name(const db_ic_inst_t *inst, csm_array *out)
{
    if ((inst == NULL) || (inst->descr == NULL))
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

    const csm_obis_code *obis = &inst->descr->obis;
    int valid = csm_array_write_u8(out, 0x09U); /* octet-string tag */
    valid = valid && csm_array_write_u8(out, 6U); /* length */
    valid = valid && csm_array_write_u8(out, obis->A);
    valid = valid && csm_array_write_u8(out, obis->B);
    valid = valid && csm_array_write_u8(out, obis->C);
    valid = valid && csm_array_write_u8(out, obis->D);
    valid = valid && csm_array_write_u8(out, obis->E);
    valid = valid && csm_array_write_u8(out, obis->F);

    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

csm_db_code db_ic_default_get_attr(const db_ic_inst_t *inst, uint8_t attr_id,
                                   uint8_t *buf, uint8_t buf_size,
                                   csm_array *out)
{
    (void) inst;
    (void) attr_id;

    if (buf == NULL)
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

    int valid = csm_array_write_buff(out, buf, buf_size);
    return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

csm_db_code db_ic_default_set_attr(db_ic_inst_t *inst, uint8_t attr_id,
                                   const uint8_t *buf, uint8_t buf_size,
                                   csm_array *in)
{
    (void) inst;
    (void) attr_id;
    (void) buf;
    (void) buf_size;
    (void) in;

    return CSM_OK;
}

csm_db_code db_ic_default_action(db_ic_inst_t *inst, uint8_t method_id,
                                 csm_array *in, csm_array *out)
{
    (void) inst;
    (void) method_id;
    (void) in;
    (void) out;

    return CSM_OK;
}
