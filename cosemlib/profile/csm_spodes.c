/**
 * SPODES (STO 006) constants and helpers for Russian metering
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_spodes.h"
#include <string.h>

/* OBIS: Device ID objects */
const csm_obis_code SPODES_OBIS_DEVICE_NAME         = { 0, 0, 42, 0, 0, 255 };
const csm_obis_code SPODES_OBIS_SERIAL_NUMBER       = { 0, 0, 96, 1, 0, 255 };
const csm_obis_code SPODES_OBIS_DEVICE_TIME         = { 0, 0, 25, 6, 0, 255 };

/* OBIS: Firmware objects */
const csm_obis_code SPODES_OBIS_FW_VERSION_ACTIVE   = { 0, 0, 96, 2, 0, 255 };
const csm_obis_code SPODES_OBIS_FW_VERSION_PASSIVE  = { 0, 0, 96, 2, 1, 255 };

/* OBIS: Energy registers */
const csm_obis_code SPODES_OBIS_ENERGY_TOTAL        = { 1, 0, 1, 8, 0, 255 };
const csm_obis_code SPODES_OBIS_ENERGY_T1           = { 1, 0, 1, 8, 1, 255 };
const csm_obis_code SPODES_OBIS_ENERGY_T2           = { 1, 0, 1, 8, 2, 255 };
const csm_obis_code SPODES_OBIS_ENERGY_T3           = { 1, 0, 1, 8, 3, 255 };
const csm_obis_code SPODES_OBIS_ENERGY_T4           = { 1, 0, 1, 8, 4, 255 };
const csm_obis_code SPODES_OBIS_ENERGY_REACTIVE_TOTAL = { 1, 0, 5, 8, 0, 255 };

/* OBIS: Current power */
const csm_obis_code SPODES_OBIS_POWER_ACTIVE_TOTAL  = { 1, 0, 12, 7, 0, 255 };
const csm_obis_code SPODES_OBIS_VOLTAGE_L1          = { 1, 0, 32, 7, 0, 255 };
const csm_obis_code SPODES_OBIS_CURRENT_L1          = { 1, 0, 31, 7, 0, 255 };

/* OBIS: Load profiles */
const csm_obis_code SPODES_OBIS_LOAD_PROFILE_1      = { 1, 0, 99, 1, 0, 255 };

/* OBIS: Event logs */
const csm_obis_code SPODES_OBIS_EVENT_LOG           = { 0, 0, 99, 98, 0, 255 };

/* OBIS: Association */
const csm_obis_code SPODES_OBIS_ASSOC_LOW           = { 0, 0, 40, 0, 0, 255 };
const csm_obis_code SPODES_OBIS_ASSOC_HIGH          = { 0, 0, 40, 0, 3, 255 };

/* Category C preset: minimal mandatory object set per STO 34006 */
static const csm_spodes_object_def category_c_objects[] =
{
    { SPODES_CLASS_DATA,              { 0, 0, 42, 0, 0, 255 }, 0 },
    { SPODES_CLASS_DATA,              { 0, 0, 96, 1, 0, 255 }, 0 },
    { SPODES_CLASS_DATA,              { 0, 0, 96, 2, 0, 255 }, 0 },
    { SPODES_CLASS_REGISTER,          { 1, 0, 1, 8, 0, 255 }, 0 },
    { SPODES_CLASS_REGISTER,          { 1, 0, 1, 8, 1, 255 }, 0 },
    { SPODES_CLASS_REGISTER,          { 1, 0, 1, 8, 2, 255 }, 0 },
    { SPODES_CLASS_REGISTER,          { 1, 0, 1, 8, 3, 255 }, 0 },
    { SPODES_CLASS_REGISTER,          { 1, 0, 1, 8, 4, 255 }, 0 },
    { SPODES_CLASS_REGISTER,          { 1, 0, 2, 8, 0, 255 }, 0 },
    { SPODES_CLASS_CLOCK,             { 0, 0, 25, 6, 0, 255 }, 0 },
    { SPODES_CLASS_PROFILE_GENERIC,   { 1, 0, 99, 1, 0, 255 }, 0 },
    { SPODES_CLASS_EVENT_LOG,         { 0, 0, 99, 98, 0, 255 }, 0 },
    { SPODES_CLASS_ASSOCIATION_LN,    { 0, 0, 40, 0, 0, 255 }, 0 },
    { SPODES_CLASS_ASSOCIATION_LN,    { 0, 0, 40, 0, 3, 255 }, 0 },
    { SPODES_CLASS_SCRIPT_TABLE,      { 0, 0, 10, 0, 255 },    0 },
    { SPODES_CLASS_SCHEDULE,          { 0, 0, 10, 0, 255 },    0 },
    { SPODES_CLASS_SPECIAL_DAYS,      { 0, 0, 11, 0, 255 },    0 },
    { SPODES_CLASS_ACTIVITY_CALENDAR, { 0, 0, 20, 0, 255 },    0 },
};

static const csm_spodes_preset category_c_preset =
{
    .objects = category_c_objects,
    .count  = sizeof(category_c_objects) / sizeof(category_c_objects[0])
};

const csm_spodes_preset *csm_spodes_category_c_preset(void)
{
    return &category_c_preset;
}

uint32_t csm_spodes_capture_period_15min(void)
{
    return 900U;
}

uint32_t csm_spodes_capture_period_30min(void)
{
    return 1800U;
}

uint32_t csm_spodes_capture_period_60min(void)
{
    return 3600U;
}

void csm_spodes_make_energy_capture(csm_object_t *obj, uint8_t tariff)
{
    if (obj == NULL)
    {
        return;
    }

    obj->class_id = SPODES_CLASS_REGISTER;
    obj->version  = 0;
    obj->id       = 0;
    obj->data_index = 0;

    obj->obis.A = 1;
    obj->obis.B = 0;
    obj->obis.C = 1;
    obj->obis.D = 8;
    obj->obis.E = tariff;
    obj->obis.F = 255;
}
