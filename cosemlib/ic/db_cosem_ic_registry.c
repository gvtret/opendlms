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
#include "db_cosem_clock_util.h"
#include "csm_axdr_codec.h"
#include "csm_config.h"
#include <string.h>

/* Classes with full implementations in their own handler files:
 *   Class 1  Data              — db_cosem_data.c
 *   Class 3  Register          — db_cosem_register.c
 *   Class 4  Extended Register — db_cosem_ext_register.c
 *   Class 5  Demand Register   — db_cosem_demand_register.c
 *   Class 6  Register Activation — db_cosem_ic_register_activation.c
 *   Class 7  Profile Generic   — db_cosem_ic_profile.c
 *   Class 8  Clock             — db_cosem_ic_registry.c
 *   Class 9  Script Table      — db_cosem_ic_script_table.c
 *   Class 10 Schedule          — db_cosem_ic_schedule.c
 *   Class 11 Special Days      — db_cosem_ic_special_days.c
 *   Class 15 Association LN    — db_cosem_ic_association.c
 *   Class 17 SAP Assignment    — db_cosem_ic_sap_assignment.c
 *   Class 18 Image Transfer    — db_cosem_ic_image_transfer.c
 *   Class 19 IEC Local Port    — db_cosem_ic_comms.c
 *   Class 20 Activity Calendar — db_cosem_ic_activity_calendar.c
 *   Class 21 Register Monitor  — db_cosem_ic_register_monitor.c
 *   Class 22 Single Action Sched — db_cosem_ic_single_action_schedule.c
 *   Class 23 IEC HDLC Setup    — db_cosem_ic_comms.c
 *   Class 26 Utility Tables    — db_cosem_ic_utility_tables.c
 *   Class 30 Data Protection   — db_cosem_ic_data_protection.c
 *   Class 31 Profile Filter    — db_cosem_ic_profile_filter.c
 *   Class 40 Push Setup        — db_cosem_ic_push.c
 *   Class 41 TCP-UDP Setup     — db_cosem_ic_comms.c
 *   Class 61 Register Table    — db_cosem_ic_register_table.c
 *   Class 62 Compact Data      — db_cosem_ic_compact_data.c
 *   Class 63 Status Mapping    — db_cosem_ic_status_mapping.c
 *   Class 64 Security Setup    — db_cosem_ic_security_setup.c
 *   Class 65 Parameter Monitor — db_cosem_ic_parameter_monitor.c
 *   Class 67 Sensor Manager    — db_cosem_ic_sensor_manager.c
 *   Class 68 Arbitrator        — db_cosem_ic_arbitrator.c
 *   Class 70 Disconnect Control — db_cosem_ic_disconnect.c
 *   Class 71 Limiter           — db_cosem_ic_limiter.c
 *   Class 8200 Table Manager   — db_cosem_ic_table_manager.c
 */

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

static db_ic_inst_t clock_instances[DB_IC_MAX_INSTANCES];
static db_cosem_clock_data_t clock_data[DB_IC_MAX_INSTANCES];
static uint8_t clock_inst_count = 0U;

static const uint8_t clock_default_dt[DB_CLOCK_DT_LEN] = {
    0x07, 0xD2, 0x0C, 0x04, 0x03, 0x0A, 0x06, 0x0B, 0xFF, 0x00, 0x78, 0x00
};

static uint8_t clock_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U
    };
    if ((month == 0U) || (month > 12U)) { return 31U; }

    uint8_t ret = days[month - 1U];
    if ((month == 2U) &&
        ((year % 4U == 0U) && ((year % 100U != 0U) || (year % 400U == 0U))))
    {
        ret = 29U;
    }
    return ret;
}

static void clock_increment_hour(db_cosem_clock_data_t *clk)
{
    clk->hour++;
    if (clk->hour < 24U) { return; }

    clk->hour = 0U;
    clk->day++;
    if (clk->day <= clock_days_in_month(clk->year, clk->month)) { return; }

    clk->day = 1U;
    clk->month++;
    if (clk->month <= 12U) { return; }

    clk->month = 1U;
    clk->year++;
}

static void clock_adjust_to_quarter(db_ic_inst_t *inst)
{
    db_cosem_clock_data_t *clk = (db_cosem_clock_data_t *)inst->data;
    uint32_t seconds = ((uint32_t)clk->minute * 60U) + (uint32_t)clk->second;
    uint32_t rounded = ((seconds + 450U) / 900U) * 900U;

    if (rounded >= 3600U)
    {
        rounded = 0U;
        clock_increment_hour(clk);
    }

    clk->minute = (uint8_t)(rounded / 60U);
    clk->second = 0U;
    clk->hundredths = 0U;
}

static void db_ic_clock_reset_count(void)
{
    clock_inst_count = 0U;
}

static db_ic_inst_t *clock_create(const csm_obis_code *obis)
{
    (void) obis;
    if (clock_inst_count >= DB_IC_MAX_INSTANCES)
    {
        return NULL;
    }

    db_ic_inst_t *inst = &clock_instances[clock_inst_count];
    db_cosem_clock_data_t *data = &clock_data[clock_inst_count];
    memset(inst, 0, sizeof(*inst));
    memset(data, 0, sizeof(*data));

    inst->descr = &clock_descr;
    inst->data = data;
    inst->version = clock_descr.version;
    db_cosem_clock_set_datetime(clock_default_dt, inst);
    clock_inst_count++;
    return inst;
}

static csm_db_code clock_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                  uint8_t attr_id, uint8_t method_id,
                                  csm_array *in, csm_array *out)
{
    if ((inst == NULL) || (inst->data == NULL))
    {
        return CSM_ERR_OBJECT_NOT_FOUND;
    }

    if (op == IC_OP_GET)
    {
        if (attr_id == 1U)
        {
            const csm_obis_code *obis = inst->has_obis ? &inst->obis : &inst->descr->obis;
            return csm_axdr_wr_octetstring(out, (const uint8_t *)&obis->A, 6U)
                ? CSM_OK
                : CSM_ERR_OBJECT_ERROR;
        }

        if (attr_id == 2U)
        {
            uint8_t dt[DB_CLOCK_DT_LEN];
            db_cosem_clock_get_datetime(dt, inst);
            return csm_axdr_wr_octetstring(out, dt, DB_CLOCK_DT_LEN)
                ? CSM_OK
                : CSM_ERR_OBJECT_ERROR;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t tag = 0U;
            uint8_t len = 0U;
            uint8_t dt[DB_CLOCK_DT_LEN];

            if (!csm_array_read_u8(in, &tag) ||
                !csm_array_read_u8(in, &len) ||
                (tag != AXDR_TAG_OCTETSTRING) ||
                (len != DB_CLOCK_DT_LEN) ||
                !csm_array_read_buff(in, dt, DB_CLOCK_DT_LEN))
            {
                return CSM_ERR_BAD_ENCODING;
            }

            db_cosem_clock_set_datetime(dt, inst);
            return CSM_OK;
        }
    }
    else if (op == IC_OP_ACTION)
    {
        if (method_id == 1U)
        {
            return CSM_ERR_DATA_CONTENT_NOT_OK;
        }
        if (method_id == 2U)
        {
            if ((in != NULL) && (csm_array_unread(in) != 0U)) { return CSM_ERR_BAD_ENCODING; }
            clock_adjust_to_quarter(inst);
            return CSM_OK;
        }
    }

    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_clock = {
    .class_id  = 8,
    .name      = "Clock",
    .version   = 0,
    .descr     = &clock_descr,
    .create    = clock_create,
    .dispatch  = clock_dispatch
};

void db_ic_register_clock(void) { db_ic_register(&ic_clock); }

/* Push Setup (Class ID 40) — implemented in db_cosem_ic_push.c */

/* Security Setup (Class ID 64) — implemented in db_cosem_ic_security_setup.c */

/* Limiter (Class ID 71) — implemented in db_cosem_ic_limiter.c */

/* Extern declarations for per-IC reset functions defined in other handler files */
extern void db_ic_data_reset_count(void);
extern void db_ic_register_reset_count(void);
extern void db_ic_ext_register_reset_count(void);
extern void db_ic_demand_register_reset_count(void);
extern void db_ic_assoc_reset_count(void);
extern void db_ic_push_reset_count(void);
extern void db_ic_security_reset_count(void);
extern void db_ic_limiter_reset_count(void);
extern void db_ic_profile_reset_count(void);
extern void db_ic_activity_cal_reset_count(void);
extern void db_ic_reg_monitor_reset_count(void);
extern void db_ic_schedule_reset_count(void);
extern void db_ic_script_table_reset_count(void);
extern void db_ic_single_action_reset_count(void);
extern void db_ic_special_days_reset_count(void);

/* ========================= Reset all counts ========================= */

void db_ic_reset_all_counts(void)
{
    db_ic_clock_reset_count();
    db_ic_data_reset_count();
    db_ic_register_reset_count();
    db_ic_ext_register_reset_count();
    db_ic_demand_register_reset_count();
    db_ic_assoc_reset_count();
    db_ic_push_reset_count();
    db_ic_security_reset_count();
    db_ic_limiter_reset_count();
    db_ic_profile_reset_count();
    db_ic_activity_cal_reset_count();
    db_ic_reg_monitor_reset_count();
    db_ic_schedule_reset_count();
    db_ic_script_table_reset_count();
    db_ic_single_action_reset_count();
    db_ic_special_days_reset_count();
}

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
    db_ic_register_script_table();
    db_ic_register_schedule();
    db_ic_register_special_days();
    db_ic_register_association_ln();
    db_ic_register_sap_assignment();
    db_ic_register_image_transfer();
    db_ic_register_comms();
    db_ic_register_activity_calendar();
    db_ic_register_register_monitor();
    db_ic_register_single_action_schedule();
    db_ic_register_utility_tables();
    db_ic_register_data_protection();
    db_ic_register_profile_filter();
    db_ic_register_push_setup();
    db_ic_register_compact_data();
    db_ic_register_register_table();
    db_ic_register_status_mapping();
    db_ic_register_security_setup();
    db_ic_register_parameter_monitor();
    db_ic_register_sensor_manager();
    db_ic_register_arbitrator();
    db_ic_register_disconnect_control();
    db_ic_register_limiter();
    db_ic_register_table_manager();
}
