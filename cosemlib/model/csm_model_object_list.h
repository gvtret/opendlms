/**
 * Object list encode/decode for Association LN attribute 2
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef CSM_MODEL_OBJECT_LIST_H
#define CSM_MODEL_OBJECT_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "csm_array.h"

int csm_model_export_object_list(csm_array *out);
int csm_model_import_object_list(csm_array *in);

#ifdef __cplusplus
}
#endif

#endif /* CSM_MODEL_OBJECT_LIST_H */
