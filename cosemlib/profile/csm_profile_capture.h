/**
 * Capture object helpers for Profile Generic
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef CSM_PROFILE_CAPTURE_H
#define CSM_PROFILE_CAPTURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "csm_array.h"
#include "csm_definitions.h"

int csm_profile_capture_encode(const csm_object_t *obj, csm_array *out);
int csm_profile_capture_decode(csm_array *in, csm_object_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* CSM_PROFILE_CAPTURE_H */
