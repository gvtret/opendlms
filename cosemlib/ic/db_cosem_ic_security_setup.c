/**
 * DLMS/COSEM Security Setup Interface Class handler (class_id = 64)
 *
 * Per Blue Book 4.4.7:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: security_policy (enum, static)
 * - Attr 3: security_suite (enum, static)
 * - Attr 4: key_transfer (octet-string, dynamic)
 * - Method 1: activate (no-response)
 * - Method 2: key_transfer (octet-string)
 * - Methods 3-8: additional key management
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

#define SEC_MAX_INSTANCES 4U
#define SEC_KEY_MAX_SIZE  256U
#define SEC_TAG_FLOAT32   23U
#define SEC_TAG_FLOAT64   24U

typedef enum {
	SEC_POLICY_NONE = 0U,
	SEC_POLICY_NO_ENCRYPTION = 1U,
	SEC_POLICY_NO_AUTH = 2U,
	SEC_POLICY_ENCRYPT_AND_AUTH = 3U
} db_ic_sec_policy_t;

typedef enum {
	SEC_SUITE_0 = 0U,
	SEC_SUITE_1 = 1U,
	SEC_SUITE_2 = 2U,
	SEC_SUITE_3 = 3U
} db_ic_sec_suite_t;

typedef struct {
	uint8_t policy;
	uint8_t suite;
	uint8_t key_transfer_buf[SEC_KEY_MAX_SIZE];
	uint8_t key_transfer_len;
	uint8_t activated;
} db_ic_sec_data;

static db_ic_sec_data sec_pool[SEC_MAX_INSTANCES];
static uint8_t sec_pool_count = 0U;

static db_ic_inst_t sec_inst_tmp;
static db_ic_inst_t sec_inst_pool[SEC_MAX_INSTANCES];
static uint8_t sec_inst_count = 0U;

void db_ic_security_reset_count(void) {
	sec_pool_count = 0U;
	sec_inst_count = 0U;
}

static const db_ic_attr_descr sec_attrs[] = {
    {DB_ACCESS_GET,                 1, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET | DB_ACCESS_SET, 2, AXDR_TAG_ENUM       },
    {DB_ACCESS_GET | DB_ACCESS_SET, 3, AXDR_TAG_ENUM       },
    {DB_ACCESS_GET | DB_ACCESS_SET, 4, AXDR_TAG_OCTETSTRING},
};

static const db_ic_method_descr sec_methods[] = {
    {DB_ACCESS_ACTION, 1, AXDR_TAG_NULL       },
    {DB_ACCESS_ACTION, 2, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_ACTION, 3, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_ACTION, 4, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_ACTION, 5, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_ACTION, 6, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_ACTION, 7, AXDR_TAG_NULL       },
    {DB_ACCESS_ACTION, 8, AXDR_TAG_NULL       },
};

static const db_ic_object_descr sec_descr = {
    .attributes = sec_attrs, .methods = sec_methods, .class_id = 64, .obis = {0, 0, 43, 0, 0, 255},
               .attr_count = 4, .method_count = 8, .version = 1
};

static db_ic_inst_t *sec_create(const csm_obis_code *obis) {
	(void)obis;

	if (sec_pool_count >= SEC_MAX_INSTANCES) {
		return NULL;
	}

	db_ic_sec_data *sd = &sec_pool[sec_pool_count];
	memset(sd, 0, sizeof(db_ic_sec_data));
	sd->policy = SEC_POLICY_NONE;
	sd->suite = SEC_SUITE_0;
	sd->key_transfer_len = 0U;
	sd->activated = 0U;
	sec_pool_count++;

	db_ic_inst_t *inst = &sec_inst_tmp;
	memset(inst, 0, sizeof(db_ic_inst_t));
	inst->descr = &sec_descr;
	inst->data = sd;
	inst->version = 1U;
	return inst;
}

static csm_db_code sec_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
	(void)method_id;

	if ((inst == NULL) || (inst->data == NULL)) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}

	db_ic_sec_data *sd = (db_ic_sec_data *)inst->data;

	if (op == IC_OP_GET) {
		switch (attr_id) {
			case 1U: {
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
			}

			case 2U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
				valid = valid && csm_array_write_u8(out, sd->policy);
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			case 3U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
				valid = valid && csm_array_write_u8(out, sd->suite);
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			case 4U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
				valid = valid && csm_array_write_u8(out, sd->key_transfer_len);
				if (sd->key_transfer_len > 0U) {
					valid = valid && csm_array_write_buff(out, sd->key_transfer_buf, sd->key_transfer_len);
				}
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			default:
				break;
		}
	} else if (op == IC_OP_SET) {
		switch (attr_id) {
			case 2U: {
				uint8_t tag = 0xFFU;
				if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ENUM) {
					return CSM_ERR_BAD_ENCODING;
				}
				if (!csm_array_read_u8(in, &sd->policy)) {
					return CSM_ERR_BAD_ENCODING;
				}
				return CSM_OK;
			}

			case 3U: {
				uint8_t tag = 0xFFU;
				if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ENUM) {
					return CSM_ERR_BAD_ENCODING;
				}
				if (!csm_array_read_u8(in, &sd->suite)) {
					return CSM_ERR_BAD_ENCODING;
				}
				return CSM_OK;
			}

			case 4U: {
				uint8_t tag = 0xFFU;
				if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) {
					return CSM_ERR_BAD_ENCODING;
				}
				uint8_t len = 0U;
				if (!csm_array_read_u8(in, &len)) {
					return CSM_ERR_BAD_ENCODING;
				}
				if (len > SEC_KEY_MAX_SIZE) {
					return CSM_ERR_BAD_ENCODING;
				}
				sd->key_transfer_len = len;
				if (len > 0U) {
					if (!csm_array_read_buff(in, sd->key_transfer_buf, len)) {
						return CSM_ERR_BAD_ENCODING;
					}
				}
				return CSM_OK;
			}

			default:
				break;
		}
	} else if (op == IC_OP_ACTION) {
		switch (method_id) {
			case 1U:
				/* activate: activate the security suite */
				sd->activated = 1U;
				return CSM_OK;

			case 2U:
			case 3U:
			case 4U:
			case 5U:
			case 6U: {
				/* key_transfer: receive wrapped key material */
				uint8_t tag = 0xFFU;
				if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) {
					return CSM_ERR_BAD_ENCODING;
				}
				uint8_t len = 0U;
				if (!csm_array_read_u8(in, &len)) {
					return CSM_ERR_BAD_ENCODING;
				}
				if (len > SEC_KEY_MAX_SIZE) {
					return CSM_ERR_BAD_ENCODING;
				}
				sd->key_transfer_len = len;
				if (len > 0U) {
					if (!csm_array_read_buff(in, sd->key_transfer_buf, len)) {
						return CSM_ERR_BAD_ENCODING;
					}
				}
				return CSM_OK;
			}

			case 7U:
				return CSM_ERR_OBJECT_NOT_FOUND;

			case 8U:
				return CSM_ERR_OBJECT_NOT_FOUND;

			default:
				break;
		}
	}

	return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_sec = {.class_id = 64, .name = "Security Setup", .version = 1, .create = sec_create, .dispatch = sec_dispatch};

void db_ic_register_security_setup(void) {
	db_ic_register(&ic_sec);
}
