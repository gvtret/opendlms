/**
 * Legacy catalog bridge for backward compatibility
 *
 * Bridges old db_element API to new IC layer, allowing legacy code
 * to access IC instances through the original database handler pattern.
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#include "db_cosem_ic.h"
#include "csm_config.h"
#include "csm_services.h"
#include "csm_axdr_codec.h"
#include <string.h>

csm_db_code db_cosem_catalog_legacy_handler(csm_db_context_t *ctx, csm_array *in, csm_array *out, csm_request *request) {
	(void)ctx;

	if ((in == NULL) || (out == NULL) || (request == NULL)) {
		return CSM_ERR_OBJECT_ERROR;
	}

	const csm_object_t *ln = &request->db_request.logical_name;

	db_ic_inst_t *inst = NULL;
	if (!db_ic_find(ln->class_id, &ln->obis, &inst) || (inst == NULL)) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}

	uint8_t attr_id = (uint8_t)ln->id;
	db_ic_op_t op;
	uint8_t method_id = 0U;

	switch (request->db_request.service) {
		case SVC_GET:
			op = IC_OP_GET;
			break;
		case SVC_SET:
			op = IC_OP_SET;
			break;
		case SVC_ACTION:
			op = IC_OP_ACTION;
			method_id = (uint8_t)ln->id;
			break;
		default:
			return CSM_ERR_OBJECT_ERROR;
	}

	return (csm_db_code)db_ic_dispatch(inst, op, attr_id, method_id, in, out);
}
