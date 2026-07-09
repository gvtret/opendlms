/**
 * DLMS/COSEM Utility Tables Interface Class handler (class_id = 26)
 *
 * Per Blue Book 4.4.3:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: table_id (Unsigned32, static)
 * - Attr 3: table (octet-string, dynamic)
 * - Attr 4: table_size (Unsigned32, dynamic)
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

#define UTILITY_MAX_INSTANCES 4U
#define UTILITY_TABLE_MAX     256U

typedef struct {
	uint32_t table_id;
	uint8_t table_buf[UTILITY_TABLE_MAX];
	uint16_t table_len;
} db_ic_utility_data;

static db_ic_utility_data utility_pool[UTILITY_MAX_INSTANCES];
static uint8_t utility_pool_count = 0U;

static db_ic_inst_t utility_inst_tmp;

void db_ic_utility_tables_reset_count(void) {
	utility_pool_count = 0U;
}

static const db_ic_attr_descr utility_attrs[] = {
    {DB_ACCESS_GET,                 1, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET | DB_ACCESS_SET, 2, AXDR_TAG_UNSIGNED32 },
    {DB_ACCESS_GET | DB_ACCESS_SET, 3, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET,                 4, AXDR_TAG_UNSIGNED32 },
};

static const db_ic_object_descr utility_descr = {
    .attributes = utility_attrs, .methods = NULL, .class_id = 26, .obis = {0, 0, 10, 0, 0, 255},
               .attr_count = 4, .method_count = 0, .version = 0
};

static db_ic_inst_t *utility_create(const csm_obis_code *obis) {
	(void)obis;
	if (utility_pool_count >= UTILITY_MAX_INSTANCES) {
		return NULL;
	}

	db_ic_utility_data *d = &utility_pool[utility_pool_count];
	memset(d, 0, sizeof(db_ic_utility_data));
	utility_pool_count++;

	memset(&utility_inst_tmp, 0, sizeof(db_ic_inst_t));
	utility_inst_tmp.descr = &utility_descr;
	utility_inst_tmp.data = d;
	utility_inst_tmp.version = 0U;
	return &utility_inst_tmp;
}

static csm_db_code utility_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
	(void)method_id;
	if ((inst == NULL) || (inst->data == NULL)) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}
	db_ic_utility_data *d = (db_ic_utility_data *)inst->data;

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
			int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
			valid = valid && csm_array_write_u32(out, d->table_id);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 3U) {
			return csm_axdr_wr_octetstring(out, d->table_buf, d->table_len) ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 4U) {
			int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED32);
			valid = valid && csm_array_write_u32(out, (uint32_t)d->table_len);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		}
	} else if (op == IC_OP_SET) {
		if (attr_id == 2U) {
			uint8_t tag = 0xFFU;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED32) {
				return CSM_ERR_BAD_ENCODING;
			}
			if (!csm_array_read_u32(in, &d->table_id)) {
				return CSM_ERR_BAD_ENCODING;
			}
			return CSM_OK;
		} else if (attr_id == 3U) {
			uint32_t len = 0U;
			if (!csm_axdr_rd_octetstring(in, &len) || len > UTILITY_TABLE_MAX) {
				return CSM_ERR_BAD_ENCODING;
			}
			d->table_len = (uint16_t)len;
			if (len > 0U) {
				if (!csm_array_read_buff(in, d->table_buf, len)) {
					return CSM_ERR_BAD_ENCODING;
				}
			}
			return CSM_OK;
		}
	}
	return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_utility = {.class_id = 26, .name = "Utility Tables", .version = 0, .create = utility_create, .dispatch = utility_dispatch};

void db_ic_register_utility_tables(void) {
	db_ic_register(&ic_utility);
}
