/**
 * SPODUS (STO 013) constants for data concentrators
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_spodus.h"
#include <string.h>

/* Table Manager OBIS codes */
const csm_obis_code SPODUS_OBIS_TABLE_MGR_1 = { 0, 0, 96, 9, 0, 255 };
const csm_obis_code SPODUS_OBIS_TABLE_MGR_2 = { 0, 0, 96, 9, 1, 255 };
const csm_obis_code SPODUS_OBIS_TABLE_MGR_3 = { 0, 0, 96, 9, 2, 255 };

/* IVKE profile OBIS codes */
const csm_obis_code SPODUS_OBIS_IVKE_PROFILE = { 0, 0, 94, 31, 12, 255 };

/* Aggregated event log OBIS codes */
const csm_obis_code SPODUS_OBIS_AGG_EVENT_LOG = { 0, 0, 99, 98, 1, 255 };
const csm_obis_code SPODUS_OBIS_AGG_EVENT_CFG = { 0, 0, 99, 98, 2, 255 };

/* IVKE profile template: 15-minute aggregation */
static const csm_spodus_ivke_template ivke_15min_template =
{
    .captures = {
        { 3U, { 1, 0, 1, 8, 0, 255 }, 2U },
        { 3U, { 1, 0, 1, 8, 1, 255 }, 2U },
        { 3U, { 1, 0, 1, 8, 2, 255 }, 2U },
        { 3U, { 1, 0, 1, 8, 3, 255 }, 2U },
        { 3U, { 1, 0, 1, 8, 4, 255 }, 2U },
        { 3U, { 1, 0, 2, 8, 0, 255 }, 2U },
        { 4U, { 1, 0, 12, 7, 0, 255 }, 2U },
    },
    .count = 7U,
    .capture_period = 900U
};

/* IVKE profile template: 60-minute aggregation */
static const csm_spodus_ivke_template ivke_60min_template =
{
    .captures = {
        { 3U, { 1, 0, 1, 8, 0, 255 }, 2U },
        { 3U, { 1, 0, 1, 8, 1, 255 }, 2U },
        { 3U, { 1, 0, 1, 8, 2, 255 }, 2U },
        { 3U, { 1, 0, 1, 8, 3, 255 }, 2U },
        { 3U, { 1, 0, 1, 8, 4, 255 }, 2U },
        { 3U, { 1, 0, 2, 8, 0, 255 }, 2U },
    },
    .count = 6U,
    .capture_period = 3600U
};

const csm_spodus_ivke_template *csm_spodus_ivke_template_15min(void)
{
    return &ivke_15min_template;
}

const csm_spodus_ivke_template *csm_spodus_ivke_template_60min(void)
{
    return &ivke_60min_template;
}

void csm_spodus_default_agg_event_config(csm_spodus_agg_event_config *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(csm_spodus_agg_event_config));

    cfg->entries[0].class_id  = 7U;
    cfg->entries[0].obis      = (csm_obis_code){ 1, 0, 99, 1, 0, 255 };
    cfg->entries[0].threshold = 10U;

    cfg->entries[1].class_id  = 7U;
    cfg->entries[1].obis      = (csm_obis_code){ 1, 0, 99, 1, 0, 255 };
    cfg->entries[1].threshold = 50U;

    cfg->count = 2U;
}
