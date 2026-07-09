/**
 * SPODUS (STO 013) constants for data concentrators
 *
 * Table Manager (class 8200) OBIS codes, IVKE profile templates,
 * and aggregated event log configuration per STO 34006.22.013.
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef CSM_PROFILE_SPODUS_H
#define CSM_PROFILE_SPODUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "csm_definitions.h"

/* Class IDs for SPODUS extensions */
#define SPODUS_CLASS_TABLE_MANAGER    8200U
#define SPODUS_CLASS_COMPACT_PROFILE  8201U
#define SPODUS_CLASS_AGGREGATED_EVENT 8202U

/* Table Manager OBIS codes */
extern const csm_obis_code SPODUS_OBIS_TABLE_MGR_1; /* 0.0.96.9.0.255 */
extern const csm_obis_code SPODUS_OBIS_TABLE_MGR_2; /* 0.0.96.9.1.255 */
extern const csm_obis_code SPODUS_OBIS_TABLE_MGR_3; /* 0.0.96.9.2.255 */

/* IVKE profile OBIS codes */
extern const csm_obis_code SPODUS_OBIS_IVKE_PROFILE; /* 0.0.94.31.12.255 */

/* Aggregated event log OBIS codes */
extern const csm_obis_code SPODUS_OBIS_AGG_EVENT_LOG; /* 0.0.99.98.1.255 */
extern const csm_obis_code SPODUS_OBIS_AGG_EVENT_CFG; /* 0.0.99.98.2.255 */

/* IVKE profile capture objects */
typedef struct {
	uint16_t class_id;
	csm_obis_code obis;
	uint8_t attribute_id;
} csm_spodus_capture_obj;

#define SPODUS_MAX_IVKE_CAPTURE 8U

typedef struct {
	csm_spodus_capture_obj captures[SPODUS_MAX_IVKE_CAPTURE];
	uint8_t count;
	uint32_t capture_period;
} csm_spodus_ivke_template;

/* Default IVKE templates */
const csm_spodus_ivke_template *csm_spodus_ivke_template_15min(void);
const csm_spodus_ivke_template *csm_spodus_ivke_template_60min(void);

/* Aggregated event log configuration */
#define SPODUS_MAX_AGG_EVENTS 16U

typedef struct {
	uint16_t class_id;
	csm_obis_code obis;
	uint8_t threshold;
} csm_spodus_agg_event_entry;

typedef struct {
	csm_spodus_agg_event_entry entries[SPODUS_MAX_AGG_EVENTS];
	uint8_t count;
} csm_spodus_agg_event_config;

void csm_spodus_default_agg_event_config(csm_spodus_agg_event_config *cfg);

#ifdef __cplusplus
}
#endif

#endif /* CSM_PROFILE_SPODUS_H */
