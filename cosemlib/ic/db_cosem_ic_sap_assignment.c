/**
 * DLMS/COSEM SAP Assignment Interface Class handler (class_id = 17)
 *
 * Per Blue Book 4.4.1:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: sap_assignment_list (array of structures, static)
 * - Method 1: connect_logical_device (Unsigned16)
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

#define SAP_MAX_INSTANCES 4U
#define SAP_MAX_ENTRIES   8U

typedef struct {
	uint16_t sap_id;
	uint8_t name[16];
	uint8_t name_len;
} db_ic_sap_entry;

typedef struct {
	db_ic_sap_entry entries[SAP_MAX_ENTRIES];
	uint8_t entry_count;
} db_ic_sap_data;

static db_ic_sap_data sap_pool[SAP_MAX_INSTANCES];
static uint8_t sap_pool_count = 0U;

static db_ic_inst_t sap_inst_tmp;

void db_ic_sap_assignment_reset_count(void) {
	sap_pool_count = 0U;
}

static const db_ic_attr_descr sap_attrs[] = {
    {DB_ACCESS_GET,                 1, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET | DB_ACCESS_SET, 2, AXDR_TAG_ARRAY      },
};

static const db_ic_method_descr sap_methods[] = {
    {DB_ACCESS_ACTION, 1, AXDR_TAG_UNSIGNED16},
};

static const db_ic_object_descr sap_descr = {
    .attributes = sap_attrs, .methods = sap_methods, .class_id = 17, .obis = {0, 0, 41, 0, 0, 255},
               .attr_count = 2, .method_count = 1, .version = 0
};

static db_ic_inst_t *sap_create(const csm_obis_code *obis) {
	(void)obis;
	if (sap_pool_count >= SAP_MAX_INSTANCES) {
		return NULL;
	}

	db_ic_sap_data *d = &sap_pool[sap_pool_count];
	memset(d, 0, sizeof(db_ic_sap_data));
	sap_pool_count++;

	memset(&sap_inst_tmp, 0, sizeof(db_ic_inst_t));
	sap_inst_tmp.descr = &sap_descr;
	sap_inst_tmp.data = d;
	sap_inst_tmp.version = 0U;
	return &sap_inst_tmp;
}

static csm_db_code sap_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
	if ((inst == NULL) || (inst->data == NULL)) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}
	db_ic_sap_data *d = (db_ic_sap_data *)inst->data;

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
			int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
			valid = valid && csm_array_write_u8(out, d->entry_count);
			for (uint8_t i = 0U; i < d->entry_count && valid; i++) {
				valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
				valid = valid && csm_array_write_u8(out, 2U);
				valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
				valid = valid && csm_array_write_u16(out, d->entries[i].sap_id);
				valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
				valid = valid && csm_array_write_u8(out, d->entries[i].name_len);
				if (d->entries[i].name_len > 0U) {
					valid = valid && csm_array_write_buff(out, d->entries[i].name, d->entries[i].name_len);
				}
			}
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		}
	} else if (op == IC_OP_SET) {
		if (attr_id == 2U) {
			uint8_t tag = 0xFFU;
			uint8_t count = 0U;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY) {
				return CSM_ERR_BAD_ENCODING;
			}
			if (!csm_array_read_u8(in, &count) || count > SAP_MAX_ENTRIES) {
				return CSM_ERR_BAD_ENCODING;
			}
			d->entry_count = count;
			for (uint8_t i = 0U; i < count; i++) {
				uint8_t stag = 0xFFU;
				uint8_t flds = 0U;
				if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE) {
					return CSM_ERR_BAD_ENCODING;
				}
				if (!csm_array_read_u8(in, &flds) || flds < 2U) {
					return CSM_ERR_BAD_ENCODING;
				}

				uint8_t utag = 0xFFU;
				if (!csm_array_read_u8(in, &utag) || utag != AXDR_TAG_UNSIGNED16) {
					return CSM_ERR_BAD_ENCODING;
				}
				if (!csm_array_read_u16(in, &d->entries[i].sap_id)) {
					return CSM_ERR_BAD_ENCODING;
				}

				uint8_t otag = 0xFFU;
				uint8_t olen = 0U;
				if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING) {
					return CSM_ERR_BAD_ENCODING;
				}
				if (!csm_array_read_u8(in, &olen) || olen > 16U) {
					return CSM_ERR_BAD_ENCODING;
				}
				d->entries[i].name_len = olen;
				if (olen > 0U) {
					if (!csm_array_read_buff(in, d->entries[i].name, olen)) {
						return CSM_ERR_BAD_ENCODING;
					}
				}
			}
			return CSM_OK;
		}
	} else if (op == IC_OP_ACTION) {
		if (method_id == 1U) {
			uint8_t tag = 0xFFU;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED16) {
				return CSM_ERR_BAD_ENCODING;
			}
			uint16_t sap_id = 0U;
			if (!csm_array_read_u16(in, &sap_id) || csm_array_unread(in) != 0U) {
				return CSM_ERR_BAD_ENCODING;
			}

			for (uint8_t i = 0U; i < d->entry_count; i++) {
				if (d->entries[i].sap_id == sap_id) {
					return CSM_OK;
				}
			}

			return CSM_ERR_OBJECT_NOT_FOUND;
		}
	}
	return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_sap = {.class_id = 17, .name = "SAP Assignment", .version = 0, .create = sap_create, .dispatch = sap_dispatch};

void db_ic_register_sap_assignment(void) {
	db_ic_register(&ic_sap);
}
