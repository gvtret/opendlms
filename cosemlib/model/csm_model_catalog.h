/**
 * YAML catalog parser for COSEM object definitions
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef CSM_MODEL_CATALOG_H
#define CSM_MODEL_CATALOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "csm_definitions.h"

#define CSM_MODEL_CATALOG_MAX_ENTRIES   64U

int csm_model_catalog_load_yaml(const char *filename);
int csm_model_catalog_parse_buffer(const char *yaml, size_t len);
int csm_model_catalog_count(void);
const csm_object_t *csm_model_catalog_get(int index);
void csm_model_catalog_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CSM_MODEL_CATALOG_H */
