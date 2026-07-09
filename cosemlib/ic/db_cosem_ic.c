/**
 * DLMS/COSEM Interface Class (IC) dispatch implementation
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

static const db_ic_class *class_registry[DB_IC_MAX_CLASSES];
static uint8_t class_count = 0U;

static db_ic_inst_t instances[DB_IC_MAX_INSTANCES];
static uint8_t instance_count = 0U;

int db_ic_init(void) {
	class_count = 0U;
	instance_count = 0U;
	memset(instances, 0, sizeof(instances));
	memset((void *)class_registry, 0, sizeof(class_registry));
	db_ic_reset_all_counts();
	return TRUE;
}

int db_ic_register(const db_ic_class *cls) {
	if ((cls == NULL) || (class_count >= DB_IC_MAX_CLASSES)) {
		CSM_ERR("[IC] Cannot register class, NULL or full");
		return FALSE;
	}

	for (uint8_t i = 0U; i < class_count; i++) {
		if (class_registry[i]->class_id == cls->class_id) {
			CSM_ERR("[IC] Class %u already registered", cls->class_id);
			return FALSE;
		}
	}

	class_registry[class_count] = cls;
	class_count++;
	CSM_LOG("[IC] Registered class %u (%s)", cls->class_id, cls->name);
	return TRUE;
}

static const db_ic_class *find_class(uint16_t class_id) {
	for (uint8_t i = 0U; i < class_count; i++) {
		if (class_registry[i]->class_id == class_id) {
			return class_registry[i];
		}
	}
	return NULL;
}

static int obis_equal(const csm_obis_code *a, const csm_obis_code *b) {
	return (a->A == b->A) && (a->B == b->B) && (a->C == b->C) && (a->D == b->D) && (a->E == b->E) && (a->F == b->F);
}

int db_ic_create_inst(uint16_t class_id, const csm_obis_code *obis, const void *init_data, void *user_ctx) {
	if ((obis == NULL) || (instance_count >= DB_IC_MAX_INSTANCES)) {
		CSM_ERR("[IC] Cannot create instance, NULL or full");
		return FALSE;
	}

	const db_ic_class *cls = find_class(class_id);
	if (cls == NULL) {
		CSM_ERR("[IC] Class %u not registered", class_id);
		return FALSE;
	}

	db_ic_inst_t *inst = &instances[instance_count];
	memset(inst, 0, sizeof(db_ic_inst_t));

	if (cls->create != NULL) {
		db_ic_inst_t *created = cls->create(obis);
		if (created != NULL) {
			inst->descr = created->descr;
			inst->data = created->data;
			inst->version = created->version;
		}
	}

	/* Fallback: if create() didn't set descr, use the class descriptor */
	if (inst->descr == NULL) {
		inst->descr = cls->descr;
	}

	inst->user_ctx = user_ctx;

	/* Store OBIS override in the instance (avoids modifying const descriptor) */
	inst->obis = *obis;
	inst->has_obis = 1U;

	(void)init_data;

	instance_count++;
	CSM_LOG("[IC] Created instance class=%u, OBIS=%02X%02X%02X%02X%02X%02X", class_id, obis->A, obis->B, obis->C, obis->D, obis->E, obis->F);
	return TRUE;
}

int db_ic_find(uint16_t class_id, const csm_obis_code *obis, db_ic_inst_t **out) {
	if ((obis == NULL) || (out == NULL)) {
		return FALSE;
	}

	for (uint8_t i = 0U; i < instance_count; i++) {
		if (instances[i].descr == NULL) {
			continue;
		}

		const csm_obis_code *inst_obis = instances[i].has_obis ? &instances[i].obis : &instances[i].descr->obis;

		if ((instances[i].descr->class_id == class_id) && obis_equal(inst_obis, obis)) {
			*out = &instances[i];
			return TRUE;
		}
	}

	*out = NULL;
	return FALSE;
}

int db_ic_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
	if (inst == NULL) {
		return (int)CSM_ERR_OBJECT_NOT_FOUND;
	}

	if (inst->descr == NULL) {
		return (int)CSM_ERR_OBJECT_NOT_FOUND;
	}

	const db_ic_class *cls = find_class(inst->descr->class_id);
	if ((cls == NULL) || (cls->dispatch == NULL)) {
		CSM_ERR("[IC] No handler for class %u", inst->descr->class_id);
		return (int)CSM_ERR_OBJECT_NOT_FOUND;
	}

	/* Verify access permissions */
	if (op == IC_OP_GET) {
		const db_ic_attr_descr *attr = NULL;
		for (uint8_t i = 0U; i < inst->descr->attr_count; i++) {
			if (inst->descr->attributes[i].id == attr_id) {
				attr = &inst->descr->attributes[i];
				break;
			}
		}
		if ((attr == NULL) || ((attr->access & DB_ACCESS_GET) == 0U)) {
			return (int)CSM_ERR_UNAUTHORIZED_ACCESS;
		}
	} else if (op == IC_OP_SET) {
		const db_ic_attr_descr *attr = NULL;
		for (uint8_t i = 0U; i < inst->descr->attr_count; i++) {
			if (inst->descr->attributes[i].id == attr_id) {
				attr = &inst->descr->attributes[i];
				break;
			}
		}
		if ((attr == NULL) || ((attr->access & DB_ACCESS_SET) == 0U)) {
			return (int)CSM_ERR_UNAUTHORIZED_ACCESS;
		}
	} else if (op == IC_OP_ACTION) {
		const db_ic_method_descr *meth = NULL;
		for (uint8_t i = 0U; i < inst->descr->method_count; i++) {
			if (inst->descr->methods[i].id == method_id) {
				meth = &inst->descr->methods[i];
				break;
			}
		}
		if ((meth == NULL) || ((meth->access & DB_ACCESS_ACTION) == 0U)) {
			return (int)CSM_ERR_UNAUTHORIZED_ACCESS;
		}
		if ((meth->type == AXDR_TAG_NULL) && (in != NULL) && (csm_array_unread(in) > 0U)) {
			uint8_t tag = 0xFFU;
			if (!csm_array_read_u8(in, &tag) || (tag != AXDR_TAG_NULL) || (csm_array_unread(in) != 0U)) {
				return (int)CSM_ERR_BAD_ENCODING;
			}
		}
	}

	return (int)cls->dispatch(inst, op, attr_id, method_id, in, out);
}

int db_ic_count(void) {
	return (int)instance_count;
}

void db_ic_reset(void) {
	instance_count = 0U;
	memset(instances, 0, sizeof(instances));
	CSM_LOG("[IC] Reset all instances");
}
