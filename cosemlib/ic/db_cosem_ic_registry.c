/**
 * DLMS/COSEM Interface Class registry
 *
 * Registers all IC classes with their class_id, name, version,
 * and handler function pointers.
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
#include <string.h>

/* Stub create/dispatch used by classes without real handlers yet */
static db_ic_inst_t ic_stub_instances[DB_IC_MAX_INSTANCES];
static uint8_t ic_stub_inst_count = 0U;

static db_ic_inst_t *ic_stub_create(const csm_obis_code *obis)
{
    (void) obis;
    if (ic_stub_inst_count >= DB_IC_MAX_INSTANCES)
    {
        return NULL;
    }
    db_ic_inst_t *inst = &ic_stub_instances[ic_stub_inst_count];
    memset(inst, 0, sizeof(db_ic_inst_t));
    ic_stub_inst_count++;
    return inst;
}

static csm_db_code ic_stub_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                     uint8_t attr_id, uint8_t method_id,
                                     csm_array *in, csm_array *out)
{
    (void) inst; (void) op; (void) attr_id; (void) method_id;
    (void) in;   (void) out;
    return CSM_OK;
}

/* Classes with full implementations in their own handler files:
 *   Class 1  Data              — db_cosem_data.c
 *   Class 3  Register          — db_cosem_register.c
 *   Class 4  Extended Register — db_cosem_ext_register.c
 *   Class 5  Demand Register   — db_cosem_demand_register.c
 *   Class 7  Profile Generic   — db_cosem_ic_profile.c
 *   Class 15 Association LN    — db_cosem_ic_association.c
 *   Class 40 Push Setup        — db_cosem_ic_push.c
 *   Class 64 Security Setup    — db_cosem_ic_security_setup.c
 *   Class 71 Limiter           — db_cosem_ic_limiter.c
 *
 * Remaining stub-only classes:
 *   Class 6  Register Activation
 *   Class 8  Clock
 *   Class 18 Image Transfer
 *   Class 70 Disconnect Control
 */

/* ========================= Register Activation (Class ID 6) ========================= */

static const db_ic_attr_descr reg_act_attrs[] = {
    { DB_ACCESS_GET,                  1, 0x09 }, /* logical_name */
    { DB_ACCESS_GET,                  2, 0x01 }, /* register_activation_object_list */
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, 0x01 }, /* register_activation_object_list_index */
};

static const db_ic_object_descr reg_act_descr = {
    .attributes   = reg_act_attrs,
    .methods      = NULL,
    .class_id     = 6,
    .obis         = { 0, 0, 0, 0, 0, 0 },
    .attr_count   = 3,
    .method_count = 0,
    .version      = 0
};

static const db_ic_class ic_reg_act = {
    .class_id  = 6,
    .name      = "Register Activation",
    .version   = 0,
    .create    = ic_stub_create,
    .dispatch  = ic_stub_dispatch
};

void db_ic_register_register_activation(void) { db_ic_register(&ic_reg_act); }

/* Profile Generic (Class ID 7) — implemented in db_cosem_ic_profile.c */

/* ========================= Clock (Class ID 8) ========================= */

static const db_ic_attr_descr clock_attrs[] = {
    { DB_ACCESS_GET,                  1, 0x09 }, /* logical_name */
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, 0x02 }, /* time */
    { DB_ACCESS_GET,                  3, 0x09 }, /* time_zone */
    { DB_ACCESS_GET,                  4, 0x09 }, /* status */
    { DB_ACCESS_GET,                  5, 0x0F }, /* daylight_savings_begin */
    { DB_ACCESS_GET,                  6, 0x0F }, /* daylight_savings_end */
    { DB_ACCESS_GET,                  7, 0x09 }, /* daylight_savings_deviation */
    { DB_ACCESS_GET,                  8, 0x09 }, /* clock_base */
    { DB_ACCESS_GET,                  9, 0x09 }, /* clock_quality */
};

static const db_ic_method_descr clock_methods[] = {
    { DB_ACCESS_ACTION, 1, 0x00 }, /* adjust_to_preset_time */
    { DB_ACCESS_ACTION, 2, 0x00 }, /* adjust_to_quarter */
};

static const db_ic_object_descr clock_descr = {
    .attributes   = clock_attrs,
    .methods      = clock_methods,
    .class_id     = 8,
    .obis         = { 0, 0, 1, 0, 0, 255 },
    .attr_count   = 9,
    .method_count = 2,
    .version      = 0
};

static const db_ic_class ic_clock = {
    .class_id  = 8,
    .name      = "Clock",
    .version   = 0,
    .create    = ic_stub_create,
    .dispatch  = ic_stub_dispatch
};

void db_ic_register_clock(void) { db_ic_register(&ic_clock); }

/* Association LN (Class ID 15) — implemented in db_cosem_ic_association.c */

/* ========================= Image Transfer (Class ID 18) ========================= */

static const db_ic_attr_descr img_xfer_attrs[] = {
    { DB_ACCESS_GET,                  1, 0x09 }, /* logical_name */
    { DB_ACCESS_GET,                  2, 0x09 }, /* image_transfer_status */
    { DB_ACCESS_GET,                  3, 0x09 }, /* image_blocks_transferred */
    { DB_ACCESS_GET,                  4, 0x09 }, /* image_block_size */
    { DB_ACCESS_GET,                  5, 0x09 }, /* image_transferred_blocks_status */
    { DB_ACCESS_GET,                  6, 0x09 }, /* image_first_not_transferred_block_number */
    { DB_ACCESS_GET,                  7, 0x09 }, /* image_transfer_enabled */
};

static const db_ic_method_descr img_xfer_methods[] = {
    { DB_ACCESS_ACTION, 1, 0x00 }, /* image_block_transfer */
    { DB_ACCESS_ACTION, 2, 0x00 }, /* image_transfer_init */
    { DB_ACCESS_ACTION, 3, 0x00 }, /* image_transfer_start */
    { DB_ACCESS_ACTION, 4, 0x00 }, /* image_transfer_stop */
    { DB_ACCESS_ACTION, 5, 0x00 }, /* image_verify */
    { DB_ACCESS_ACTION, 6, 0x00 }, /* image_activate */
};

static const db_ic_object_descr img_xfer_descr = {
    .attributes   = img_xfer_attrs,
    .methods      = img_xfer_methods,
    .class_id     = 18,
    .obis         = { 0, 0, 44, 0, 0, 255 },
    .attr_count   = 7,
    .method_count = 6,
    .version      = 0
};

static const db_ic_class ic_img_xfer = {
    .class_id  = 18,
    .name      = "Image Transfer",
    .version   = 0,
    .create    = ic_stub_create,
    .dispatch  = ic_stub_dispatch
};

void db_ic_register_image_transfer(void) { db_ic_register(&ic_img_xfer); }

/* Push Setup (Class ID 40) — implemented in db_cosem_ic_push.c */

/* Security Setup (Class ID 64) — implemented in db_cosem_ic_security_setup.c */

/* ========================= Disconnect Control (Class ID 70) ========================= */

static const db_ic_attr_descr disc_attrs[] = {
    { DB_ACCESS_GET,                  1, 0x09 }, /* logical_name */
    { DB_ACCESS_GET,                  2, 0x03 }, /* output_state */
    { DB_ACCESS_GET,                  3, 0x03 }, /* control_mode */
    { DB_ACCESS_GET,                  4, 0x09 }, /* control_configuration */
    { DB_ACCESS_GET,                  5, 0x09 }, /* control_event */
};

static const db_ic_method_descr disc_methods[] = {
    { DB_ACCESS_ACTION, 1, 0x00 }, /* disconnect */
    { DB_ACCESS_ACTION, 2, 0x00 }, /* reconnect */
    { DB_ACCESS_ACTION, 3, 0x00 }, /* output_pulse_on */
    { DB_ACCESS_ACTION, 4, 0x00 }, /* output_pulse_off */
};

static const db_ic_object_descr disc_descr = {
    .attributes   = disc_attrs,
    .methods      = disc_methods,
    .class_id     = 70,
    .obis         = { 0, 0, 96, 3, 10, 255 },
    .attr_count   = 5,
    .method_count = 4,
    .version      = 0
};

static const db_ic_class ic_disc = {
    .class_id  = 70,
    .name      = "Disconnect Control",
    .version   = 0,
    .create    = ic_stub_create,
    .dispatch  = ic_stub_dispatch
};

void db_ic_register_disconnect_control(void) { db_ic_register(&ic_disc); }

/* Limiter (Class ID 71) — implemented in db_cosem_ic_limiter.c */

/* ========================= Registration helper ========================= */

void db_ic_register_all_builtins(void)
{
    db_ic_register_data();
    db_ic_register_register();
    db_ic_register_extended_register();
    db_ic_register_demand_register();
    db_ic_register_register_activation();
    db_ic_register_profile_generic();
    db_ic_register_clock();
    db_ic_register_association_ln();
    db_ic_register_image_transfer();
    db_ic_register_push_setup();
    db_ic_register_security_setup();
    db_ic_register_disconnect_control();
    db_ic_register_limiter();
}
