/**
 * DLMS/COSEM Demand Register Interface Class handler (class_id = 5)
 *
 * Per Blue Book 4.3.4:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: current_average_value (CHOICE, dynamic)
 * - Attr 3: scaler_unit (structure, static)
 * - Attr 4: status (CHOICE, dynamic)
 * - Attr 5: period (long-unsigned, static)
 * - Attr 6: number_of_periods (long-unsigned, static)
 * - Attr 7: last_average_value (CHOICE, dynamic)
 * - Method 1: reset (data)
 * - Method 2: next_period (data)
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

#define DEMAND_VALUE_MAX       64U
#define DEMAND_MAX_INSTANCES   8U
#define DEMAND_SCALER_UNIT_LEN 6U
#define DEMAND_TAG_FLOAT32     23U
#define DEMAND_TAG_FLOAT64     24U

typedef struct {
	uint8_t current_avg_buf[DEMAND_VALUE_MAX];
	uint8_t current_avg_len;
	uint8_t scaler_unit[DEMAND_SCALER_UNIT_LEN];
	uint8_t status_buf[DEMAND_VALUE_MAX];
	uint8_t status_len;
	uint16_t period;
	uint16_t num_periods;
	uint8_t last_avg_buf[DEMAND_VALUE_MAX];
	uint8_t last_avg_len;
} db_ic_demand_val_t;

static db_ic_demand_val_t demand_val_pool[DEMAND_MAX_INSTANCES];
static uint8_t demand_val_count = 0U;

static db_ic_inst_t demand_inst_tmp;
static db_ic_inst_t demand_inst_pool[DEMAND_MAX_INSTANCES];
static uint8_t demand_inst_count = 0U;

void db_ic_demand_register_reset_count(void) {
	demand_val_count = 0U;
	demand_inst_count = 0U;
}

static const db_ic_attr_descr demand_attrs[] = {
    {DB_ACCESS_GET, 1, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET, 2, AXDR_TAG_NULL       },
    {DB_ACCESS_GET, 3, AXDR_TAG_STRUCTURE  },
    {DB_ACCESS_GET, 4, AXDR_TAG_NULL       },
    {DB_ACCESS_GET, 5, AXDR_TAG_UNSIGNED16 },
    {DB_ACCESS_GET, 6, AXDR_TAG_UNSIGNED16 },
    {DB_ACCESS_GET, 7, AXDR_TAG_NULL       },
};

static const db_ic_method_descr demand_methods[] = {
    {DB_ACCESS_ACTION, 1, AXDR_TAG_NULL},
    {DB_ACCESS_ACTION, 2, AXDR_TAG_NULL},
};

static const db_ic_object_descr demand_descr = {
    .attributes = demand_attrs, .methods = demand_methods, .class_id = 5, .obis = {0, 0, 0, 0, 0, 0},
               .attr_count = 7, .method_count = 2, .version = 0
};

static uint8_t demand_axdr_fixed_size(uint8_t tag) {
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
		case DEMAND_TAG_FLOAT32:
			return 4U;
		case DEMAND_TAG_FLOAT64:
			return 8U;
		default:
			return 0U;
	}
}

static int demand_axdr_is_length_coded(uint8_t tag) {
	return (tag == AXDR_TAG_OCTETSTRING) || (tag == AXDR_TAG_VISIBLESTRING) || (tag == AXDR_TAG_UTF8_STRING) || (tag == AXDR_TAG_BITSTRING);
}

static int demand_read_axdr_value(csm_array *in, uint8_t *buf, uint8_t *len, uint8_t max_len) {
	uint8_t tag = 0xFFU;
	if (!csm_array_read_u8(in, &tag)) {
		return FALSE;
	}

	buf[0] = tag;

	if (tag == AXDR_TAG_NULL) {
		*len = 1U;
		return TRUE;
	} else if (demand_axdr_is_length_coded(tag)) {
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
		uint8_t dsize = demand_axdr_fixed_size(tag);
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

static void demand_init_scaler_unit(uint8_t *su, int8_t scaler, uint8_t unit) {
	su[0] = AXDR_TAG_STRUCTURE;
	su[1] = 4U;
	su[2] = AXDR_TAG_INTEGER8;
	su[3] = (uint8_t)scaler;
	su[4] = AXDR_TAG_ENUM;
	su[5] = unit;
}

static db_ic_inst_t *demand_create(const csm_obis_code *obis) {
	(void)obis;

	if (demand_val_count >= DEMAND_MAX_INSTANCES) {
		return NULL;
	}

	db_ic_demand_val_t *val = &demand_val_pool[demand_val_count];
	val->current_avg_buf[0] = AXDR_TAG_NULL;
	val->current_avg_len = 1U;
	demand_init_scaler_unit(val->scaler_unit, 0, 0);
	val->status_buf[0] = AXDR_TAG_NULL;
	val->status_len = 1U;
	val->period = 0U;
	val->num_periods = 0U;
	val->last_avg_buf[0] = AXDR_TAG_NULL;
	val->last_avg_len = 1U;
	demand_val_count++;

	db_ic_inst_t *inst = &demand_inst_tmp;
	memset(inst, 0, sizeof(db_ic_inst_t));
	inst->descr = &demand_descr;
	inst->data = val;
	inst->version = 0U;
	return inst;
}

static csm_db_code demand_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
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
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			int valid = csm_array_write_buff(out, val->current_avg_buf, val->current_avg_len);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 3U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			int valid = csm_array_write_buff(out, val->scaler_unit, DEMAND_SCALER_UNIT_LEN);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 4U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			int valid = csm_array_write_buff(out, val->status_buf, val->status_len);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 5U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
			valid = valid && csm_array_write_u16(out, val->period);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 6U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
			valid = valid && csm_array_write_u16(out, val->num_periods);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 7U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			int valid = csm_array_write_buff(out, val->last_avg_buf, val->last_avg_len);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		}
	} else if (op == IC_OP_SET) {
		if (attr_id == 2U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			if (!demand_read_axdr_value(in, val->current_avg_buf, &val->current_avg_len, DEMAND_VALUE_MAX)) {
				return CSM_ERR_BAD_ENCODING;
			}
			return CSM_OK;
		} else if (attr_id == 4U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			if (!demand_read_axdr_value(in, val->status_buf, &val->status_len, DEMAND_VALUE_MAX)) {
				return CSM_ERR_BAD_ENCODING;
			}
			return CSM_OK;
		} else if (attr_id == 5U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			uint8_t tag = 0xFFU;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) {
				return CSM_ERR_BAD_ENCODING;
			}
			uint16_t v = 0U;
			if (!csm_array_read_u16(in, &v)) {
				return CSM_ERR_BAD_ENCODING;
			}
			val->period = v;
			return CSM_OK;
		} else if (attr_id == 6U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			uint8_t tag = 0xFFU;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) {
				return CSM_ERR_BAD_ENCODING;
			}
			uint16_t v = 0U;
			if (!csm_array_read_u16(in, &v)) {
				return CSM_ERR_BAD_ENCODING;
			}
			val->num_periods = v;
			return CSM_OK;
		}
	} else if (op == IC_OP_ACTION) {
		if (method_id == 1U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			val->current_avg_buf[0] = AXDR_TAG_NULL;
			val->current_avg_len = 1U;
			val->status_buf[0] = AXDR_TAG_NULL;
			val->status_len = 1U;
			val->last_avg_buf[0] = AXDR_TAG_NULL;
			val->last_avg_len = 1U;
			return CSM_OK;
		} else if (method_id == 2U) {
			db_ic_demand_val_t *val = (db_ic_demand_val_t *)inst->data;
			memcpy(val->last_avg_buf, val->current_avg_buf, val->current_avg_len);
			val->last_avg_len = val->current_avg_len;
			val->current_avg_buf[0] = AXDR_TAG_NULL;
			val->current_avg_len = 1U;
			return CSM_OK;
		}
	}

	return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_demand_class = {.class_id = 5, .name = "Demand Register", .version = 0, .create = demand_create, .dispatch = demand_dispatch};

void db_ic_register_demand_register(void) {
	db_ic_register(&ic_demand_class);
}
