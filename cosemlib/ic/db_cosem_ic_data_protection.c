/**
 * DLMS/COSEM Data Protection Interface Class handler (class_id = 30)
 *
 * Per Blue Book 4.4.6:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: key (octet-string, static)
 * - Attr 3: password (octet-string, static)
 * - Attr 4: protection_status (enum, static)
 * - Attr 5: error_code (enum, static)
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

#define DATA_PROT_MAX_INSTANCES 4U
#define DATA_PROT_KEY_MAX       16U
#define DATA_PROT_PWD_MAX       16U

/* Protection status enum */
#define DATA_PROT_STATUS_UNLOCKED 0U
#define DATA_PROT_STATUS_LOCKED   1U

/* Error codes */
#define DATA_PROT_ERROR_NONE           0U
#define DATA_PROT_ERROR_UNKNOWN_KEY    1U
#define DATA_PROT_ERROR_WRONG_PASSWORD 2U

typedef struct {
	uint8_t key[DATA_PROT_KEY_MAX];
	uint8_t key_len;
	uint8_t password[DATA_PROT_PWD_MAX];
	uint8_t password_len;
	uint8_t protection_status;
	uint8_t error_code;
} db_ic_data_prot_data;

static db_ic_data_prot_data data_prot_pool[DATA_PROT_MAX_INSTANCES];
static uint8_t data_prot_pool_count = 0U;

static db_ic_inst_t data_prot_inst_tmp;

void db_ic_data_protection_reset_count(void) {
	data_prot_pool_count = 0U;
}

static const db_ic_attr_descr data_prot_attrs[] = {
    {DB_ACCESS_GET,                 1, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET | DB_ACCESS_SET, 2, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET | DB_ACCESS_SET, 3, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET,                 4, AXDR_TAG_ENUM       },
    {DB_ACCESS_GET,                 5, AXDR_TAG_ENUM       },
};

static const db_ic_object_descr data_prot_descr = {
    .attributes = data_prot_attrs, .methods = NULL, .class_id = 30, .obis = {0, 0, 43, 0, 0, 255},
               .attr_count = 5, .method_count = 0, .version = 0
};

static db_ic_inst_t *data_prot_create(const csm_obis_code *obis) {
	(void)obis;
	if (data_prot_pool_count >= DATA_PROT_MAX_INSTANCES) {
		return NULL;
	}

	db_ic_data_prot_data *d = &data_prot_pool[data_prot_pool_count];
	memset(d, 0, sizeof(db_ic_data_prot_data));
	d->protection_status = DATA_PROT_STATUS_UNLOCKED;
	data_prot_pool_count++;

	memset(&data_prot_inst_tmp, 0, sizeof(db_ic_inst_t));
	data_prot_inst_tmp.descr = &data_prot_descr;
	data_prot_inst_tmp.data = d;
	data_prot_inst_tmp.version = 0U;
	return &data_prot_inst_tmp;
}

static csm_db_code data_prot_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
	(void)method_id;
	if ((inst == NULL) || (inst->data == NULL)) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}
	db_ic_data_prot_data *d = (db_ic_data_prot_data *)inst->data;

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
			int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
			valid = valid && csm_array_write_u8(out, d->key_len);
			if (d->key_len > 0U) {
				valid = valid && csm_array_write_buff(out, d->key, d->key_len);
			}
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 3U) {
			int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
			valid = valid && csm_array_write_u8(out, d->password_len);
			if (d->password_len > 0U) {
				valid = valid && csm_array_write_buff(out, d->password, d->password_len);
			}
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 4U) {
			int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
			valid = valid && csm_array_write_u8(out, d->protection_status);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 5U) {
			int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
			valid = valid && csm_array_write_u8(out, d->error_code);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		}
	} else if (op == IC_OP_SET) {
		if (attr_id == 2U) {
			uint8_t tag = 0xFFU;
			uint8_t len = 0U;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) {
				return CSM_ERR_BAD_ENCODING;
			}
			if (!csm_array_read_u8(in, &len) || len > DATA_PROT_KEY_MAX) {
				return CSM_ERR_BAD_ENCODING;
			}
			d->key_len = len;
			if (len > 0U) {
				if (!csm_array_read_buff(in, d->key, len)) {
					return CSM_ERR_BAD_ENCODING;
				}
			}
			return CSM_OK;
		} else if (attr_id == 3U) {
			uint8_t tag = 0xFFU;
			uint8_t len = 0U;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) {
				return CSM_ERR_BAD_ENCODING;
			}
			if (!csm_array_read_u8(in, &len) || len > DATA_PROT_PWD_MAX) {
				return CSM_ERR_BAD_ENCODING;
			}
			d->password_len = len;
			if (len > 0U) {
				if (!csm_array_read_buff(in, d->password, len)) {
					return CSM_ERR_BAD_ENCODING;
				}
			}
			return CSM_OK;
		}
	}
	return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_data_prot = {.class_id = 30, .name = "Data Protection", .version = 0, .create = data_prot_create, .dispatch = data_prot_dispatch};

void db_ic_register_data_protection(void) {
	db_ic_register(&ic_data_prot);
}
