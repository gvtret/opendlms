/**
 * SPODES (STO 006) constants and helpers for Russian metering
 *
 * OBIS codes for mandatory objects and preset configurations
 * per STO 34006.22.026.001 (SPODES Category C).
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef CSM_PROFILE_SPODES_H
#define CSM_PROFILE_SPODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "csm_definitions.h"

#define SPODES_MAX_PRESET_OBJECTS    32U

typedef struct {
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t version;
} csm_spodes_object_def;

typedef struct {
    const csm_spodes_object_def *objects;
    uint8_t count;
} csm_spodes_preset;

/* Class IDs */
#define SPODES_CLASS_DATA                    1U
#define SPODES_CLASS_REGISTER                3U
#define SPODES_CLASS_EXTENDED_REGISTER       4U
#define SPODES_CLASS_DEMAND_REGISTER         5U
#define SPODES_CLASS_REGISTER_ACTIVATION     6U
#define SPODES_CLASS_PROFILE_GENERIC          7U
#define SPODES_CLASS_CLOCK                    8U
#define SPODES_CLASS_SCRIPT_TABLE            12U
#define SPODES_CLASS_SCHEDULE                10U
#define SPODES_CLASS_SPECIAL_DAYS            11U
#define SPODES_CLASS_ACTIVITY_CALENDAR       20U
#define SPODES_CLASS_SINGLE_ACTION_SCHEDULE  22U
#define SPODES_CLASS_DISCONNECT_CONTROL      70U
#define SPODES_CLASS_LIMITER                 71U
#define SPODES_CLASS_ASSOCIATION_LN          15U
#define SPODES_CLASS_IMAGE_TRANSFER          18U
#define SPODES_CLASS_PUSH_SETUP             40U
#define SPODES_CLASS_SECURITY_SETUP          64U
#define SPODES_CLASS_COMPACT_DATA           61U
#define SPODES_CLASS_DATA_PROTECTION        73U
#define SPODES_CLASS_EVENT_LOG              74U

/* OBIS: Device ID objects */
extern const csm_obis_code SPODES_OBIS_DEVICE_NAME;        /* 0.0.42.0.0.255 */
extern const csm_obis_code SPODES_OBIS_SERIAL_NUMBER;      /* 0.0.96.1.0.255 */
extern const csm_obis_code SPODES_OBIS_DEVICE_TIME;        /* 0.0.25.6.0.255 */

/* OBIS: Firmware objects */
extern const csm_obis_code SPODES_OBIS_FW_VERSION_ACTIVE;  /* 0.0.96.2.0.255 */
extern const csm_obis_code SPODES_OBIS_FW_VERSION_PASSIVE; /* 0.0.96.2.1.255 */

/* OBIS: Energy registers (total, tariffs 1-4) */
extern const csm_obis_code SPODES_OBIS_ENERGY_TOTAL;       /* 1.0.1.8.0.255 */
extern const csm_obis_code SPODES_OBIS_ENERGY_T1;          /* 1.0.1.8.1.255 */
extern const csm_obis_code SPODES_OBIS_ENERGY_T2;          /* 1.0.1.8.2.255 */
extern const csm_obis_code SPODES_OBIS_ENERGY_T3;          /* 1.0.1.8.3.255 */
extern const csm_obis_code SPODES_OBIS_ENERGY_T4;          /* 1.0.1.8.4.255 */

extern const csm_obis_code SPODES_OBIS_ENERGY_REACTIVE_TOTAL; /* 1.0.5.8.0.255 */

/* OBIS: Current power */
extern const csm_obis_code SPODES_OBIS_POWER_ACTIVE_TOTAL; /* 1.0.12.7.0.255 */
extern const csm_obis_code SPODES_OBIS_VOLTAGE_L1;         /* 1.0.32.7.0.255 */
extern const csm_obis_code SPODES_OBIS_CURRENT_L1;         /* 1.0.31.7.0.255 */

/* OBIS: Load profiles */
extern const csm_obis_code SPODES_OBIS_LOAD_PROFILE_1;     /* 1.0.99.1.0.255 */

/* OBIS: Event logs */
extern const csm_obis_code SPODES_OBIS_EVENT_LOG;          /* 0.0.99.98.0.255 */

/* OBIS: Association */
extern const csm_obis_code SPODES_OBIS_ASSOC_LOW;          /* 0.0.40.0.0.255 */
extern const csm_obis_code SPODES_OBIS_ASSOC_HIGH;         /* 0.0.40.0.3.255 */

/* Default capture period: 15 minutes (900 seconds) */
#define SPODES_DEFAULT_CAPTURE_PERIOD   900U

/* Category C preset: minimal mandatory object set */
const csm_spodes_preset *csm_spodes_category_c_preset(void);

/* Capture period templates */
uint32_t csm_spodes_capture_period_15min(void);
uint32_t csm_spodes_capture_period_30min(void);
uint32_t csm_spodes_capture_period_60min(void);

/* Helper: populate a standard energy register capture object */
void csm_spodes_make_energy_capture(csm_object_t *obj, uint8_t tariff);

#ifdef __cplusplus
}
#endif

#endif /* CSM_PROFILE_SPODES_H */
