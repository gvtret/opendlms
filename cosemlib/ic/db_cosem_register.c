/**
 * DLMS/COSEM Register Interface Class handler (class_id = 3)
 *
 * Per Blue Book 4.3.2:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: value (CHOICE, dynamic)
 * - Attr 3: scaler_unit (structure, static)
 * - Method 1: reset (data)
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

#define REG_VALUE_MAX       64U
#define REG_MAX_INSTANCES   8U
#define REG_SCALER_UNIT_LEN 6U
#define REG_TAG_FLOAT32     23U
#define REG_TAG_FLOAT64     24U

typedef struct {
	uint8_t value_buf[REG_VALUE_MAX];
	uint8_t value_len;
	uint8_t scaler_unit[REG_SCALER_UNIT_LEN];
} db_ic_reg_val_t;

static db_ic_reg_val_t reg_val_pool[REG_MAX_INSTANCES];
static uint8_t reg_val_count = 0U;

static db_ic_inst_t reg_inst_tmp;
static db_ic_inst_t reg_inst_pool[REG_MAX_INSTANCES];
static uint8_t reg_inst_count = 0U;

void db_ic_register_reset_count(void) {
	reg_val_count = 0U;
	reg_inst_count = 0U;
}

static const db_ic_attr_descr register_attrs[] = {
    {DB_ACCESS_GET, 1, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET, 2, AXDR_TAG_NULL       },
    {DB_ACCESS_GET, 3, AXDR_TAG_STRUCTURE  },
};

static const db_ic_method_descr register_methods[] = {
    {DB_ACCESS_ACTION, 1, AXDR_TAG_NULL},
};

static const db_ic_object_descr register_descr = {
    .attributes = register_attrs, .methods = register_methods, .class_id = 3, .obis = {0, 0, 0, 0, 0, 0},
               .attr_count = 3, .method_count = 1, .version = 0
};

static uint8_t reg_axdr_fixed_size(uint8_t tag) {
	switch (tag) {
		case AXDR_TAG_NULL:
			return 0U;
		case AXDR_TAG_BOOLEAN:
			return 1U;
		case AXDR_TAG_INTEGER32:
			return 4U;
		case AXDR_TAG_UNSIGNED32:
			return 4U;
		case AXDR_TAG_INTEGER8:
			return 1U;
		case AXDR_TAG_INTEGER16:
			return 2U;
		case AXDR_TAG_UNSIGNED8:
			return 1U;
		case AXDR_TAG_UNSIGNED16:
			return 2U;
		case AXDR_TAG_INTEGER64:
			return 8U;
		case AXDR_TAG_UNSIGNED64:
			return 8U;
		case AXDR_TAG_ENUM:
			return 1U;
		case REG_TAG_FLOAT32:
			return 4U;
		case REG_TAG_FLOAT64:
			return 8U;
		default:
			return 0U;
	}
}

static int reg_axdr_is_length_coded(uint8_t tag) {
	return (tag == AXDR_TAG_OCTETSTRING) || (tag == AXDR_TAG_VISIBLESTRING) || (tag == AXDR_TAG_UTF8_STRING) || (tag == AXDR_TAG_BITSTRING);
}

static int reg_write_axdr_value(csm_array *out, const uint8_t *buf, uint8_t len) {
	return csm_array_write_buff(out, buf, len);
}

static int reg_read_axdr_value(csm_array *in, uint8_t *buf, uint8_t *len, uint8_t max_len) {
	uint8_t tag = 0xFFU;
	if (!csm_array_read_u8(in, &tag)) {
		return FALSE;
	}

	buf[0] = tag;

	if (tag == AXDR_TAG_NULL) {
		*len = 1U;
		return TRUE;
	} else if (reg_axdr_is_length_coded(tag)) {
		uint8_t lbyte = 0U;
		if (!csm_array_read_u8(in, &lbyte)) {
			return FALSE;
		}
		if ((uint16_t)(2U + lbyte) > max_len) {
			return FALSE;
		}
		buf[1] = lbyte;
		if (lbyte > 0U) {
			if (!csm_array_read_buff(in, &buf[2], lbyte)) {
				return FALSE;
			}
		}
		*len = 2U + lbyte;
		return TRUE;
	} else {
		uint8_t dsize = reg_axdr_fixed_size(tag);
		if (dsize == 0U) {
			return FALSE;
		}
		if ((uint16_t)(1U + dsize) > max_len) {
			return FALSE;
		}
		if (!csm_array_read_buff(in, &buf[1], dsize)) {
			return FALSE;
		}
		*len = 1U + dsize;
		return TRUE;
	}
}

static void reg_init_scaler_unit(uint8_t *su, int8_t scaler, uint8_t unit) {
	su[0] = AXDR_TAG_STRUCTURE;
	su[1] = 4U;
	su[2] = AXDR_TAG_INTEGER8;
	su[3] = (uint8_t)scaler;
	su[4] = AXDR_TAG_ENUM;
	su[5] = unit;
}

static db_ic_inst_t *reg_create(const csm_obis_code *obis) {
	(void)obis;

	if (reg_val_count >= REG_MAX_INSTANCES) {
		return NULL;
	}

	db_ic_reg_val_t *val = &reg_val_pool[reg_val_count];
	val->value_buf[0] = AXDR_TAG_NULL;
	val->value_len = 1U;
	reg_init_scaler_unit(val->scaler_unit, 0, 0);
	reg_val_count++;

	db_ic_inst_t *inst = &reg_inst_tmp;
	memset(inst, 0, sizeof(db_ic_inst_t));
	inst->descr = &register_descr;
	inst->data = val;
	inst->version = 0U;
	return inst;
}

static csm_db_code reg_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
	(void)method_id;

	if ((inst == NULL) || (inst->data == NULL)) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}

	if (op == IC_OP_GET) {
		if (attr_id == 1U) {
			const csm_obis_code *obis = inst->has_obis ? &inst->obis : &inst->descr->obis;
			int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
			valid = valid && csm_array_write_u8(out, 6U);
			valid = valid && csm_array_write_u8(out, obis->A);
			valid = valid && csm_array_write_u8(out, obis->B);
			valid = valid && csm_array_write_u8(out, obis->C);
			valid = valid && csm_array_write_u8(out, obis->D);
			valid = valid && csm_array_write_u8(out, obis->E);
			valid = valid && csm_array_write_u8(out, obis->F);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 2U) {
			db_ic_reg_val_t *val = (db_ic_reg_val_t *)inst->data;
			int valid = reg_write_axdr_value(out, val->value_buf, val->value_len);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 3U) {
			db_ic_reg_val_t *val = (db_ic_reg_val_t *)inst->data;
			int valid = csm_array_write_buff(out, val->scaler_unit, REG_SCALER_UNIT_LEN);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		}
	} else if (op == IC_OP_ACTION) {
		if (method_id == 1U) {
			db_ic_reg_val_t *val = (db_ic_reg_val_t *)inst->data;
			memset(val->value_buf, 0, REG_VALUE_MAX);
			val->value_buf[0] = AXDR_TAG_NULL;
			val->value_len = 1U;
			return CSM_OK;
		}
	}

	return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_register_class = {.class_id = 3, .name = "Register", .version = 0, .create = reg_create, .dispatch = reg_dispatch};

void db_ic_register_register(void) {
	db_ic_register(&ic_register_class);
}
