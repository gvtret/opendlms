/**
 * Profile row helpers for Profile Generic
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef CSM_PROFILE_ROW_H
#define CSM_PROFILE_ROW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "csm_array.h"

#define CSM_PROFILE_ROW_MAX_VALUES   16U
#define CSM_PROFILE_ROW_VALUE_MAX    64U

int csm_profile_row_encode(const uint8_t *values, uint8_t count, csm_array *out);
int csm_profile_row_decode(csm_array *in, uint8_t *values, uint8_t *count);

#ifdef __cplusplus
}
#endif

#endif /* CSM_PROFILE_ROW_H */
