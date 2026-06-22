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

#include "csm_model_instance.h"
#include "csm_config.h"
#include <string.h>

static csm_object_t instance_table[CSM_MODEL_INSTANCE_MAX];
static int instance_count = 0;

static int obis_equal(const csm_obis_code *a, const csm_obis_code *b)
{
    return (a->A == b->A) && (a->B == b->B) && (a->C == b->C) &&
           (a->D == b->D) && (a->E == b->E) && (a->F == b->F);
}

int csm_model_instance_add(uint16_t class_id, const csm_obis_code *obis, uint8_t version)
{
    if (obis == NULL)
    {
        return FALSE;
    }

    if (instance_count >= (int)CSM_MODEL_INSTANCE_MAX)
    {
        CSM_ERR("[INSTANCE] Table full, max %u", CSM_MODEL_INSTANCE_MAX);
        return FALSE;
    }

    for (int i = 0; i < instance_count; i++)
    {
        if (instance_table[i].class_id == class_id &&
            obis_equal(&instance_table[i].obis, obis))
        {
            CSM_ERR("[INSTANCE] Duplicate class=%u OBIS=%02X%02X%02X%02X%02X%02X",
                    class_id, obis->A, obis->B, obis->C, obis->D, obis->E, obis->F);
            return FALSE;
        }
    }

    csm_object_t *obj = &instance_table[instance_count];
    obj->class_id   = class_id;
    obj->obis       = *obis;
    obj->version    = version;
    obj->id         = (int8_t)instance_count;
    obj->data_index = 0;

    instance_count++;
    CSM_LOG("[INSTANCE] Added class=%u OBIS=%02X%02X%02X%02X%02X%02X v%u",
            class_id, obis->A, obis->B, obis->C, obis->D, obis->E, obis->F, version);
    return TRUE;
}

const csm_object_t *csm_model_instance_find(uint16_t class_id, const csm_obis_code *obis)
{
    if (obis == NULL)
    {
        return NULL;
    }

    for (int i = 0; i < instance_count; i++)
    {
        if (instance_table[i].class_id == class_id &&
            obis_equal(&instance_table[i].obis, obis))
        {
            return &instance_table[i];
        }
    }

    return NULL;
}

const csm_object_t *csm_model_instance_get(int index)
{
    if (index < 0 || index >= instance_count)
    {
        return NULL;
    }
    return &instance_table[index];
}

int csm_model_instance_count(void)
{
    return instance_count;
}

void csm_model_instance_reset(void)
{
    instance_count = 0;
    memset(instance_table, 0, sizeof(instance_table));
    CSM_LOG("[INSTANCE] Reset all instances");
}
