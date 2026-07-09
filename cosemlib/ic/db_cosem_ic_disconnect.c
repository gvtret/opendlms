/**
 * DLMS/COSEM Disconnect Control Interface Class handler (class_id = 70)
 *
 * Per Blue Book 4.5.8:
 * - Attr 1: logical_name (octet-string, static)
 * - Attr 2: output_state (boolean, dynamic)
 * - Attr 3: control_mode (enum, static)
 * - Attr 4: control_configuration (octet-string, static)
 * - Attr 5: control_event (structure, dynamic)
 * - Method 1: remote_disconnect (null)
 * - Method 2: remote_reconnect (null)
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

#define DISC_MAX_INSTANCES 4U
#define DISC_CONFIG_MAX    16U
#define DISC_DATETIME_LEN  12U

/* Control mode enum */
#define DISC_MODE_MANUAL         0U
#define DISC_MODE_AUTO_RECONNECT 1U
#define DISC_MODE_REMOTE_ONLY    2U

typedef struct {
	uint8_t output_state;
	uint8_t control_mode;
	uint8_t control_config[DISC_CONFIG_MAX];
	uint8_t control_config_len;
	uint8_t control_event[DISC_DATETIME_LEN];
	uint8_t control_event_valid;
} db_ic_disc_data;

static db_ic_disc_data disc_pool[DISC_MAX_INSTANCES];
static uint8_t disc_pool_count = 0U;

static db_ic_inst_t disc_inst_tmp;

void db_ic_disconnect_reset_count(void) {
	disc_pool_count = 0U;
}

static const db_ic_attr_descr disc_attrs[] = {
    {DB_ACCESS_GET,                 1, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET,                 2, AXDR_TAG_BOOLEAN    },
    {DB_ACCESS_GET | DB_ACCESS_SET, 3, AXDR_TAG_ENUM       },
    {DB_ACCESS_GET | DB_ACCESS_SET, 4, AXDR_TAG_OCTETSTRING},
    {DB_ACCESS_GET,                 5, AXDR_TAG_STRUCTURE  },
};

static const db_ic_method_descr disc_methods[] = {
    {DB_ACCESS_ACTION, 1, AXDR_TAG_NULL},
    {DB_ACCESS_ACTION, 2, AXDR_TAG_NULL},
};

static const db_ic_object_descr disc_descr = {
    .attributes = disc_attrs, .methods = disc_methods, .class_id = 70, .obis = {0, 0, 96, 3, 10, 255},
               .attr_count = 5, .method_count = 2, .version = 0
};

static db_ic_inst_t *disc_create(const csm_obis_code *obis) {
	(void)obis;
	if (disc_pool_count >= DISC_MAX_INSTANCES) {
		return NULL;
	}

	db_ic_disc_data *d = &disc_pool[disc_pool_count];
	memset(d, 0, sizeof(db_ic_disc_data));
	d->output_state = 1U;
	d->control_mode = DISC_MODE_MANUAL;
	disc_pool_count++;

	memset(&disc_inst_tmp, 0, sizeof(db_ic_inst_t));
	disc_inst_tmp.descr = &disc_descr;
	disc_inst_tmp.data = d;
	disc_inst_tmp.version = 0U;
	return &disc_inst_tmp;
}

static csm_db_code disc_dispatch(db_ic_inst_t *inst, db_ic_op_t op, uint8_t attr_id, uint8_t method_id, csm_array *in, csm_array *out) {
	if ((inst == NULL) || (inst->data == NULL)) {
		return CSM_ERR_OBJECT_NOT_FOUND;
	}
	db_ic_disc_data *d = (db_ic_disc_data *)inst->data;

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
			int valid = csm_array_write_u8(out, AXDR_TAG_BOOLEAN);
			valid = valid && csm_array_write_u8(out, d->output_state);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 3U) {
			int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
			valid = valid && csm_array_write_u8(out, d->control_mode);
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 4U) {
			int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
			valid = valid && csm_array_write_u8(out, d->control_config_len);
			if (d->control_config_len > 0U) {
				valid = valid && csm_array_write_buff(out, d->control_config, d->control_config_len);
			}
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		} else if (attr_id == 5U) {
			int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
			valid = valid && csm_array_write_u8(out, 2U);
			valid = valid && csm_array_write_u8(out, AXDR_TAG_BOOLEAN);
			valid = valid && csm_array_write_u8(out, d->output_state);
			valid = valid && csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
			valid = valid && csm_array_write_u8(out, DISC_DATETIME_LEN);
			if (d->control_event_valid) {
				valid = valid && csm_array_write_buff(out, d->control_event, DISC_DATETIME_LEN);
			} else {
				for (uint8_t i = 0U; i < DISC_DATETIME_LEN; i++) {
					valid = valid && csm_array_write_u8(out, 0xFFU);
				}
			}
			return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
		}
	} else if (op == IC_OP_SET) {
		if (attr_id == 3U) {
			uint8_t tag = 0xFFU;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ENUM) {
				return CSM_ERR_BAD_ENCODING;
			}
			if (!csm_array_read_u8(in, &d->control_mode)) {
				return CSM_ERR_BAD_ENCODING;
			}
			return CSM_OK;
		} else if (attr_id == 4U) {
			uint8_t tag = 0xFFU;
			uint8_t len = 0U;
			if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) {
				return CSM_ERR_BAD_ENCODING;
			}
			if (!csm_array_read_u8(in, &len) || len > DISC_CONFIG_MAX) {
				return CSM_ERR_BAD_ENCODING;
			}
			d->control_config_len = len;
			if (len > 0U) {
				if (!csm_array_read_buff(in, d->control_config, len)) {
					return CSM_ERR_BAD_ENCODING;
				}
			}
			return CSM_OK;
		}
	} else if (op == IC_OP_ACTION) {
		if (method_id == 1U) {
			/* remote_disconnect */
			d->output_state = 0U;
			d->control_event_valid = 1U;
			memset(d->control_event, 0xFFU, DISC_DATETIME_LEN);
			return CSM_OK;
		} else if (method_id == 2U) {
			/* remote_reconnect */
			d->output_state = 1U;
			d->control_event_valid = 1U;
			memset(d->control_event, 0xFFU, DISC_DATETIME_LEN);
			return CSM_OK;
		}
	}
	return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_disc = {.class_id = 70, .name = "Disconnect Control", .version = 0, .create = disc_create, .dispatch = disc_dispatch};

void db_ic_register_disconnect_control(void) {
	db_ic_register(&ic_disc);
}
