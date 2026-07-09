/**
 * DLMS/COSEM Association LN Interface Class handler (class_id = 15)
 *
 * Per Blue Book 4.4.4:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: object_list (array of structures, dynamic)
 * - Attr 3: access_rights (structure, dynamic)
 * - Attr 4: application_context_name (octet-string, static)
 * - Attr 5: security_mechanism_name (octet-string, static)
 * - Attr 6: association_status (enum, dynamic)
 * - Attr 7: xDLMS_context_info (structure, static)
 * - Attr 8: authentication_mechanism_name (octet-string, static)
 * - Attr 9: secret (octet-string, dynamic)
 * - Attr 10: user_list (array of structures, dynamic)
 * - Attr 11: current_user_name (octet-string, dynamic)
 * - Method 1: add_user
 * - Method 3: add_object
 * - Method 4: remove_object
 * - Method 5: remove_user
 * - Method 6: change_password
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

#define ASO_MAX_INSTANCES   4U
#define ASO_MAX_OBJECTS     16U
#define ASO_MAX_USERS       8U
#define ASO_MAX_NAME_SIZE   16U
#define ASO_MAX_SECRET_SIZE 32U
#define ASO_MAX_CTX_INFO    32U
#define ASO_MAX_APP_NAME    32U
#define ASO_MAX_ACCESS_SIZE 64U
#define ASO_TAG_FLOAT32     23U
#define ASO_TAG_FLOAT64     24U

typedef struct {
	uint16_t class_id;
	csm_obis_code obis;
} db_ic_aso_object_t;

typedef struct {
	uint8_t name[ASO_MAX_NAME_SIZE];
	uint8_t name_len;
	uint8_t password[ASO_MAX_NAME_SIZE];
	uint8_t password_len;
} db_ic_aso_user_t;

typedef struct {
	/* object_list */
	db_ic_aso_object_t objects[ASO_MAX_OBJECTS];
	uint8_t object_count;

	/* access_rights: stored as raw AXDR structure */
	uint8_t access_rights_buf[ASO_MAX_ACCESS_SIZE];
	uint8_t access_rights_len;

	/* application_context_name: octet-string */
	uint8_t app_context_buf[ASO_MAX_APP_NAME];
	uint8_t app_context_len;

	/* security_mechanism_name: octet-string */
	uint8_t sec_mechanism_buf[ASO_MAX_APP_NAME];
	uint8_t sec_mechanism_len;

	/* association_status: enum */
	uint8_t association_status;

	/* xDLMS_context_info: stored as raw AXDR structure */
	uint8_t xdlms_ctx_buf[ASO_MAX_CTX_INFO];
	uint8_t xdlms_ctx_len;

	/* authentication_mechanism_name: octet-string */
	uint8_t auth_mechanism_buf[ASO_MAX_APP_NAME];
	uint8_t auth_mechanism_len;

	/* secret: octet-string */
	uint8_t secret_buf[ASO_MAX_SECRET_SIZE];
	uint8_t secret_len;

	/* user_list: array of {name, password} */
	db_ic_aso_user_t users[ASO_MAX_USERS];
	uint8_t user_count;

	/* current_user_name: octet-string */
	uint8_t current_user_buf[ASO_MAX_NAME_SIZE];
	uint8_t current_user_len;
} db_ic_aso_data;

static db_ic_aso_data aso_pool[ASO_MAX_INSTANCES];
static uint8_t aso_pool_count = 0U;

static db_ic_inst_t aso_inst_tmp;
static db_ic_inst_t aso_inst_pool[ASO_MAX_INSTANCES];
static uint8_t aso_inst_count = 0U;

void db_ic_assoc_reset_count(void) {
	aso_pool_count = 0U;
	aso_inst_count = 0U;
}

static const db_ic_attr_descr aso_attrs[] = {
    {DB_ACCESS_GET,                 1,  AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET | DB_ACCESS_SET, 2,  AXDR_TAG_ARRAY      },
    {DB_ACCESS_GET | DB_ACCESS_SET, 3,  AXDR_TAG_STRUCTURE  },
    {DB_ACCESS_GET,                 4,  AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET,                 5,  AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET,                 6,  AXDR_TAG_ENUM       },
    {DB_ACCESS_GET,                 7,  AXDR_TAG_STRUCTURE  },
    {DB_ACCESS_GET,                 8,  AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET | DB_ACCESS_SET, 9,  AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET,                 10, AXDR_TAG_ARRAY      },
    {DB_ACCESS_GET,                 11, AXDR_TAG_OCTETSTRING},
};

/* Association LN (class 15, version 2) methods, per IEC 62056-6-2 §5.3.7:
 * 1 reply_to_HLS_authentication, 2 change_HLS_secret, 3 add_object,
 * 4 remove_object, 5 add_user, 6 remove_user. */
static const db_ic_method_descr aso_methods[] = {
    {DB_ACCESS_ACTION, 1, AXDR_TAG_STRUCTURE},
    {DB_ACCESS_ACTION, 2, AXDR_TAG_STRUCTURE},
    {DB_ACCESS_ACTION, 3, AXDR_TAG_STRUCTURE},
    {DB_ACCESS_ACTION, 4, AXDR_TAG_STRUCTURE},
    {DB_ACCESS_ACTION, 5, AXDR_TAG_STRUCTURE},
    {DB_ACCESS_ACTION, 6, AXDR_TAG_STRUCTURE},
};

static const db_ic_object_descr aso_descr = {
    .attributes = aso_attrs, .methods = aso_methods, .class_id = 15, .obis = {0, 0, 40, 0, 0, 255},
               .attr_count = 11, .method_count = 6, .version = 2
};

static uint8_t aso_axdr_fixed_size(uint8_t tag) {
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
		case ASO_TAG_FLOAT32:
			return 4U;
		case ASO_TAG_FLOAT64:
			return 8U;
		default:
			return 0U;
	}
}

static int aso_axdr_is_length_coded(uint8_t tag) {
	return (tag == AXDR_TAG_OCTETSTRING) || (tag == AXDR_TAG_VISIBLESTRING) || (tag == AXDR_TAG_UTF8_STRING) || (tag == AXDR_TAG_BITSTRING);
}

static int aso_read_axdr_value(csm_array *in, uint8_t *buf, uint8_t *len, uint8_t max_len) {
	uint8_t tag = 0xFFU;
	if (!csm_array_read_u8(in, &tag)) {
		return FALSE;
	}

	buf[0] = tag;

	if (tag == AXDR_TAG_NULL) {
		*len = 1U;
		return TRUE;
	} else if (aso_axdr_is_length_coded(tag)) {
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
		uint8_t dsize = aso_axdr_fixed_size(tag);
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

static int aso_read_octetstring(csm_array *in, uint8_t *buf, uint8_t *len, uint8_t max_len) {
	uint8_t tag = 0xFFU;
	if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) {
		return FALSE;
	}
	uint8_t slen = 0U;
	if (!csm_array_read_u8(in, &slen)) {
		return FALSE;
	}
	if (slen > max_len) {
		return FALSE;
	}
	*len = slen;
	if (slen > 0U) {
		return csm_array_read_buff(in, buf, slen);
	}
	return TRUE;
}

static db_ic_inst_t *aso_create(const csm_obis_code *obis) {
	(void)obis;

	if (aso_pool_count >= ASO_MAX_INSTANCES) {
		return NULL;
	}

	db_ic_aso_data *ad = &aso_pool[aso_pool_count];
	memset(ad, 0, sizeof(db_ic_aso_data));
	ad->object_count = 0U;
	ad->access_rights_buf[0] = AXDR_TAG_NULL;
	ad->access_rights_len = 1U;
	ad->app_context_len = 0U;
	ad->sec_mechanism_len = 0U;
	ad->association_status = 0U;
	ad->xdlms_ctx_buf[0] = AXDR_TAG_NULL;
	ad->xdlms_ctx_len = 1U;
	ad->auth_mechanism_len = 0U;
	ad->secret_len = 0U;
	ad->user_count = 0U;
	ad->current_user_len = 0U;
	aso_pool_count++;

	db_ic_inst_t *inst = &aso_inst_tmp;
	memset(inst, 0, sizeof(db_ic_inst_t));
	inst->descr = &aso_descr;
	inst->data = ad;
	inst->version = 0U;
	return inst;
}

static csm_db_code aso_get_object_list(const db_ic_aso_data *ad, csm_array *out) {
	int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
	valid = valid && csm_array_write_u8(out, ad->object_count);

	for (uint8_t i = 0U; i < ad->object_count && valid; i++) {
		valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
		valid = valid && csm_array_write_u8(out, 2U);
		valid = valid && csm_array_write_u8(out, AXDR_TAG_UNSIGNED16);
		valid = valid && csm_array_write_u16(out, ad->objects[i].class_id);
		valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
		valid = valid && csm_array_write_u8(out, 6U);
		valid = valid && csm_array_write_u8(out, ad->objects[i].obis.A);
		valid = valid && csm_array_write_u8(out, ad->objects[i].obis.B);
		valid = valid && csm_array_write_u8(out, ad->objects[i].obis.C);
		valid = valid && csm_array_write_u8(out, ad->objects[i].obis.D);
		valid = valid && csm_array_write_u8(out, ad->objects[i].obis.E);
		valid = valid && csm_array_write_u8(out, ad->objects[i].obis.F);
	}

	return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static csm_db_code aso_get_user_list(const db_ic_aso_data *ad, csm_array *out) {
	int valid = csm_array_write_u8(out, AXDR_TAG_ARRAY);
	valid = valid && csm_array_write_u8(out, ad->user_count);

	for (uint8_t i = 0U; i < ad->user_count && valid; i++) {
		valid = valid && csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
		valid = valid && csm_array_write_u8(out, 2U);
		valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
		valid = valid && csm_array_write_u8(out, ad->users[i].name_len);
		if (ad->users[i].name_len > 0U) {
			valid = valid && csm_array_write_buff(out, ad->users[i].name, ad->users[i].name_len);
		}
		valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
		valid = valid && csm_array_write_u8(out, ad->users[i].password_len);
		if (ad->users[i].password_len > 0U) {
			valid = valid && csm_array_write_buff(out, ad->users[i].password, ad->users[i].password_len);
		}
	}

	return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
}

static int aso_find_object(const db_ic_aso_data *ad, uint16_t class_id, const csm_obis_code *obis) {
	for (uint8_t i = 0U; i < ad->object_count; i++) {
		if (ad->objects[i].class_id == class_id && ad->objects[i].obis.A == obis->A && ad->objects[i].obis.B == obis->B && ad->objects[i].obis.C == obis->C &&
		    ad->objects[i].obis.D == obis->D && ad->objects[i].obis.E == obis->E && ad->objects[i].obis.F == obis->F) {
			return (int)i;
		}
	}
	return -1;
}

static int aso_find_user(const db_ic_aso_data *ad, const uint8_t *name, uint8_t name_len) {
	for (uint8_t i = 0U; i < ad->user_count; i++) {
		if (ad->users[i].name_len == name_len && memcmp(ad->users[i].name, name, name_len) == 0) {
			return (int)i;
		}
	}
	return -1;
}

static csm_db_code aso_method_add_user(db_ic_aso_data *ad, csm_array *in) {
	uint8_t tag = 0xFFU;
	uint8_t flds = 0U;
	if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) {
		return CSM_ERR_BAD_ENCODING;
	}
	if (!csm_array_read_u8(in, &flds) || flds < 2U) {
		return CSM_ERR_BAD_ENCODING;
	}

	if (ad->user_count >= ASO_MAX_USERS) {
		return CSM_ERR_BAD_ENCODING;
	}

	db_ic_aso_user_t *user = &ad->users[ad->user_count];

	if (!aso_read_octetstring(in, user->name, &user->name_len, ASO_MAX_NAME_SIZE)) {
		return CSM_ERR_BAD_ENCODING;
	}
	if (!aso_read_octetstring(in, user->password, &user->password_len, ASO_MAX_NAME_SIZE)) {
		return CSM_ERR_BAD_ENCODING;
	}

	ad->user_count++;
	return CSM_OK;
}

static csm_db_code aso_method_add_object(db_ic_aso_data *ad, csm_array *in) {
	uint8_t tag = 0xFFU;
	uint8_t flds = 0U;
	if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) {
		return CSM_ERR_BAD_ENCODING;
	}
	if (!csm_array_read_u8(in, &flds) || flds < 2U) {
		return CSM_ERR_BAD_ENCODING;
	}

	if (ad->object_count >= ASO_MAX_OBJECTS) {
		return CSM_ERR_BAD_ENCODING;
	}

	/* class_id: Unsigned16 */
	uint8_t ctag = 0xFFU;
	if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16) {
		return CSM_ERR_BAD_ENCODING;
	}
	uint16_t class_id = 0U;
	if (!csm_array_read_u16(in, &class_id)) {
		return CSM_ERR_BAD_ENCODING;
	}

	/* logical_name: Octet-string(6) */
	uint8_t otag = 0xFFU;
	uint8_t olen = 0U;
	if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING) {
		return CSM_ERR_BAD_ENCODING;
	}
	if (!csm_array_read_u8(in, &olen) || olen != 6U) {
		return CSM_ERR_BAD_ENCODING;
	}
	csm_obis_code obis;
	{
		uint8_t ob[6] = {0U};
		if (!csm_array_read_buff(in, ob, 6U)) {
			return CSM_ERR_BAD_ENCODING;
		}
		obis.A = ob[0];
		obis.B = ob[1];
		obis.C = ob[2];
		obis.D = ob[3];
		obis.E = ob[4];
		obis.F = ob[5];
	}

	/* Check for duplicate */
	if (aso_find_object(ad, class_id, &obis) >= 0) {
		return CSM_OK;
	}

	ad->objects[ad->object_count].class_id = class_id;
	ad->objects[ad->object_count].obis = obis;
	ad->object_count++;

	return CSM_OK;
}

static csm_db_code aso_method_remove_object(db_ic_aso_data *ad, csm_array *in) {
	uint8_t tag = 0xFFU;
	uint8_t flds = 0U;
	if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) {
		return CSM_ERR_BAD_ENCODING;
	}
	if (!csm_array_read_u8(in, &flds) || flds < 2U) {
		return CSM_ERR_BAD_ENCODING;
	}

	uint8_t ctag = 0xFFU;
	if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16) {
		return CSM_ERR_BAD_ENCODING;
	}
	uint16_t class_id = 0U;
	if (!csm_array_read_u16(in, &class_id)) {
		return CSM_ERR_BAD_ENCODING;
	}

	uint8_t otag = 0xFFU;
	uint8_t olen = 0U;
	if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING) {
		return CSM_ERR_BAD_ENCODING;
	}
	if (!csm_array_read_u8(in, &olen) || olen != 6U) {
		return CSM_ERR_BAD_ENCODING;
	}
	csm_obis_code obis;
	{
		uint8_t ob[6] = {0U};
		if (!csm_array_read_buff(in, ob, 6U)) {
			return CSM_ERR_BAD_ENCODING;
		}
		obis.A = ob[0];
		obis.B = ob[1];
		obis.C = ob[2];
		obis.D = ob[3];
		obis.E = ob[4];
		obis.F = ob[5];
	}

	int idx = aso_find_object(ad, class_id, &obis);
	if (idx < 0) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}

	/* Shift remaining objects */
	uint8_t remove_idx = (uint8_t)idx;
	for (uint8_t i = remove_idx; i < ad->object_count - 1U; i++) {
		ad->objects[i] = ad->objects[i + 1U];
	}
	ad->object_count--;

	return CSM_OK;
}

static csm_db_code aso_method_remove_user(db_ic_aso_data *ad, csm_array *in) {
	uint8_t tag = 0xFFU;
	uint8_t flds = 0U;
	if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) {
		return CSM_ERR_BAD_ENCODING;
	}
	if (!csm_array_read_u8(in, &flds) || flds < 1U) {
		return CSM_ERR_BAD_ENCODING;
	}

	uint8_t name[ASO_MAX_NAME_SIZE];
	uint8_t name_len = 0U;
	if (!aso_read_octetstring(in, name, &name_len, ASO_MAX_NAME_SIZE)) {
		return CSM_ERR_BAD_ENCODING;
	}

	int idx = aso_find_user(ad, name, name_len);
	if (idx < 0) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}

	uint8_t remove_idx = (uint8_t)idx;
	for (uint8_t i = remove_idx; i < ad->user_count - 1U; i++) {
		ad->users[i] = ad->users[i + 1U];
	}
	ad->user_count--;

	return CSM_OK;
}

static csm_db_code aso_method_change_password(db_ic_aso_data *ad, csm_array *in) {
	uint8_t tag = 0xFFU;
	uint8_t flds = 0U;
	if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_STRUCTURE) {
		return CSM_ERR_BAD_ENCODING;
	}
	if (!csm_array_read_u8(in, &flds) || flds < 2U) {
		return CSM_ERR_BAD_ENCODING;
	}

	/* user_name: Octet-string */
	uint8_t name[ASO_MAX_NAME_SIZE];
	uint8_t name_len = 0U;
	if (!aso_read_octetstring(in, name, &name_len, ASO_MAX_NAME_SIZE)) {
		return CSM_ERR_BAD_ENCODING;
	}

	/* new_password: Octet-string */
	uint8_t new_pw[ASO_MAX_NAME_SIZE];
	uint8_t new_pw_len = 0U;
	if (!aso_read_octetstring(in, new_pw, &new_pw_len, ASO_MAX_NAME_SIZE)) {
		return CSM_ERR_BAD_ENCODING;
	}

	int idx = aso_find_user(ad, name, name_len);
	if (idx < 0) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}

	uint8_t copy_len = new_pw_len;
	if (copy_len > ASO_MAX_NAME_SIZE) {
		copy_len = ASO_MAX_NAME_SIZE;
	}
	memcpy(ad->users[idx].password, new_pw, copy_len);
	ad->users[idx].password_len = copy_len;

	return CSM_OK;
}

static csm_db_code aso_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
	(void)method_id;

	if ((inst == NULL) || (inst->data == NULL)) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}

	db_ic_aso_data *ad = (db_ic_aso_data *)inst->data;

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

			case 2U:
				return aso_get_object_list(ad, out);

			case 3U:
				return csm_array_write_buff(out, ad->access_rights_buf, ad->access_rights_len) ? CSM_OK : CSM_ERR_BAD_ENCODING;

			case 4U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
				valid = valid && csm_array_write_u8(out, ad->app_context_len);
				if (ad->app_context_len > 0U) {
					valid = valid && csm_array_write_buff(out, ad->app_context_buf, ad->app_context_len);
				}
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			case 5U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
				valid = valid && csm_array_write_u8(out, ad->sec_mechanism_len);
				if (ad->sec_mechanism_len > 0U) {
					valid = valid && csm_array_write_buff(out, ad->sec_mechanism_buf, ad->sec_mechanism_len);
				}
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			case 6U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
				valid = valid && csm_array_write_u8(out, ad->association_status);
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			case 7U:
				return csm_array_write_buff(out, ad->xdlms_ctx_buf, ad->xdlms_ctx_len) ? CSM_OK : CSM_ERR_BAD_ENCODING;

			case 8U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
				valid = valid && csm_array_write_u8(out, ad->auth_mechanism_len);
				if (ad->auth_mechanism_len > 0U) {
					valid = valid && csm_array_write_buff(out, ad->auth_mechanism_buf, ad->auth_mechanism_len);
				}
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			case 9U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
				valid = valid && csm_array_write_u8(out, ad->secret_len);
				if (ad->secret_len > 0U) {
					valid = valid && csm_array_write_buff(out, ad->secret_buf, ad->secret_len);
				}
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			case 10U:
				return aso_get_user_list(ad, out);

			case 11U: {
				int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
				valid = valid && csm_array_write_u8(out, ad->current_user_len);
				if (ad->current_user_len > 0U) {
					valid = valid && csm_array_write_buff(out, ad->current_user_buf, ad->current_user_len);
				}
				return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
			}

			default:
				break;
		}
	} else if (op == IC_OP_SET) {
		switch (attr_id) {
			case 2U: {
				/* object_list: read array of structures */
				uint8_t tag = 0xFFU;
				if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ARRAY) {
					return CSM_ERR_BAD_ENCODING;
				}
				uint8_t count = 0U;
				if (!csm_array_read_u8(in, &count) || count > ASO_MAX_OBJECTS) {
					return CSM_ERR_BAD_ENCODING;
				}
				ad->object_count = count;
				for (uint8_t i = 0U; i < count; i++) {
					uint8_t stag = 0xFFU;
					uint8_t sflds = 0U;
					if (!csm_array_read_u8(in, &stag) || stag != AXDR_TAG_STRUCTURE) {
						return CSM_ERR_BAD_ENCODING;
					}
					if (!csm_array_read_u8(in, &sflds) || sflds < 2U) {
						return CSM_ERR_BAD_ENCODING;
					}

					uint8_t ctag = 0xFFU;
					if (!csm_array_read_u8(in, &ctag) || ctag != AXDR_TAG_UNSIGNED16) {
						return CSM_ERR_BAD_ENCODING;
					}
					if (!csm_array_read_u16(in, &ad->objects[i].class_id)) {
						return CSM_ERR_BAD_ENCODING;
					}

					uint8_t otag = 0xFFU;
					uint8_t olen = 0U;
					if (!csm_array_read_u8(in, &otag) || otag != AXDR_TAG_OCTETSTRING) {
						return CSM_ERR_BAD_ENCODING;
					}
					if (!csm_array_read_u8(in, &olen) || olen != 6U) {
						return CSM_ERR_BAD_ENCODING;
					}
					{
						uint8_t ob[6] = {0U};
						if (!csm_array_read_buff(in, ob, 6U)) {
							return CSM_ERR_BAD_ENCODING;
						}
						ad->objects[i].obis.A = ob[0];
						ad->objects[i].obis.B = ob[1];
						ad->objects[i].obis.C = ob[2];
						ad->objects[i].obis.D = ob[3];
						ad->objects[i].obis.E = ob[4];
						ad->objects[i].obis.F = ob[5];
					}
				}
				return CSM_OK;
			}

			case 3U:
				return aso_read_axdr_value(in, ad->access_rights_buf, &ad->access_rights_len, ASO_MAX_ACCESS_SIZE) ? CSM_OK : CSM_ERR_BAD_ENCODING;

			case 9U:
				return aso_read_axdr_value(in, ad->secret_buf, &ad->secret_len, ASO_MAX_SECRET_SIZE) ? CSM_OK : CSM_ERR_BAD_ENCODING;

			default:
				break;
		}
	} else if (op == IC_OP_ACTION) {
		switch (method_id) {
			case 1U:
				/* reply_to_HLS_authentication is performed by the channel (HLS
             * pass 3/4) before the object handler is reached; not serviced here. */
				return CSM_ERR_OBJECT_NOT_FOUND;

			case 2U:
				return aso_method_change_password(ad, in); /* change_HLS_secret */

			case 3U:
				return aso_method_add_object(ad, in);

			case 4U:
				return aso_method_remove_object(ad, in);

			case 5U:
				return aso_method_add_user(ad, in);

			case 6U:
				return aso_method_remove_user(ad, in);

			default:
				break;
		}
	}

	return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_asso_ln = {.class_id = 15, .name = "Association LN", .version = 2, .create = aso_create, .dispatch = aso_dispatch};

void db_ic_register_association_ln(void) {
	db_ic_register(&ic_asso_ln);
}
