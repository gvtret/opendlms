/**
 * Association management DB handler for server
 *
 * Bridges Association LN (class 15) IC instances to the DB access handler
 * pattern. Handles object_list, access_rights, association_status.
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#include "db_cosem_associations.h"
#include "db_cosem_ic.h"
#include "csm_config.h"
#include "csm_axdr_codec.h"
#include <string.h>

int db_cosem_associations_init(void)
{
    CSM_LOG("[ASSO] Association handler initialized");
    return TRUE;
}

csm_db_code db_cosem_associations_func(csm_db_context_t *ctx, csm_array *in,
                                        csm_array *out, csm_request *request)
{
    (void) ctx;

    if ((in == NULL) || (out == NULL) || (request == NULL))
    {
        return CSM_ERR_OBJECT_ERROR;
    }

    const csm_object_t *ln = &request->db_request.logical_name;

    db_ic_inst_t *inst = NULL;
    if (!db_ic_find(15, &ln->obis, &inst) || (inst == NULL))
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

    uint8_t attr_id = (uint8_t) ln->id;
    db_ic_op_t op;

    switch (request->db_request.service)
    {
    case SVC_GET:
        op = IC_OP_GET;
        break;
    case SVC_SET:
        op = IC_OP_SET;
        break;
    case SVC_ACTION:
        op = IC_OP_ACTION;
        break;
    default:
        return CSM_ERR_OBJECT_ERROR;
    }

    uint8_t method_id = (op == IC_OP_ACTION) ? (uint8_t) request->db_request.logical_name.id : 0U;

    return (csm_db_code) db_ic_dispatch(inst, op, attr_id, method_id, in, out);
}
