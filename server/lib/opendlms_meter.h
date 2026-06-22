/**
 * OpenDLMS meter initialization and main loop
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef OPENDLMS_METER_H
#define OPENDLMS_METER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void opendlms_meter_init(void);
void opendlms_meter_poll(uint32_t ms);
void opendlms_meter_execute(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENDLMS_METER_H */
