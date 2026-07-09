/**
 * Instance table management for COSEM objects
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef CSM_MODEL_INSTANCE_H
#define CSM_MODEL_INSTANCE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "csm_definitions.h"

#define CSM_MODEL_INSTANCE_MAX 64U

int csm_model_instance_add(uint16_t class_id, const csm_obis_code *obis, uint8_t version);
const csm_object_t *csm_model_instance_find(uint16_t class_id, const csm_obis_code *obis);
const csm_object_t *csm_model_instance_get(int index);
int csm_model_instance_count(void);
void csm_model_instance_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CSM_MODEL_INSTANCE_H */
