/**
 * OpenDLMS meter initialization and main loop
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#include "opendlms_meter.h"
#include "db_cosem_ic.h"
#include "csm_channel.h"
#include "csm_config.h"
#include "csm_services.h"

extern void csm_sys_init(void);

extern csm_db_code csm_db_access_func(csm_db_context_t *ctx, csm_array *in,
                                       csm_array *out, csm_request *request);

static void meter_register_ic_instances(void)
{
    csm_obis_code obis;

    obis.A = 0; obis.B = 0; obis.C = 1; obis.D = 0; obis.E = 0; obis.F = 255;
    db_ic_create_inst(8, &obis, NULL, NULL);

    obis.A = 0; obis.B = 0; obis.C = 40; obis.D = 0; obis.E = 0; obis.F = 255;
    db_ic_create_inst(15, &obis, NULL, NULL);

    obis.A = 0; obis.B = 0; obis.C = 43; obis.D = 0; obis.E = 0; obis.F = 255;
    db_ic_create_inst(64, &obis, NULL, NULL);

    obis.A = 0; obis.B = 0; obis.C = 25; obis.D = 1; obis.E = 0; obis.F = 255;
    db_ic_create_inst(40, &obis, NULL, NULL);

    obis.A = 0; obis.B = 0; obis.C = 96; obis.D = 3; obis.E = 10; obis.F = 255;
    db_ic_create_inst(70, &obis, NULL, NULL);
}

void opendlms_meter_init(void)
{
    csm_sys_init();

    db_ic_init();
    db_ic_register_all_builtins();

    meter_register_ic_instances();

    csm_services_init(csm_db_access_func);

    CSM_LOG("[METER] Initialization complete, %d IC instances", db_ic_count());
}

void opendlms_meter_poll(uint32_t ms)
{
    (void) ms;
}

void opendlms_meter_execute(void)
{
}
