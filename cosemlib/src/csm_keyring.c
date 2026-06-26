/**
 * \file csm_keyring.c
 * \brief Small fixed-size keyring implementation.
 */

#include "csm_keyring.h"

#include <string.h>

void csm_keyring_init(csm_keyring *kr)
{
    if (kr)
    {
        memset(kr, 0, sizeof(*kr));
    }
}

int csm_keyring_add(csm_keyring *kr, uint8_t id, const uint8_t *key, uint8_t len)
{
    if (!kr || !key || len == 0U || len > sizeof(kr->entries[0].key))
    {
        return -1;
    }

    for (uint8_t i = 0U; i < kr->count; i++)
    {
        if (kr->entries[i].id == id)
        {
            memcpy(kr->entries[i].key, key, len);
            kr->entries[i].key_len = len;
            return 0;
        }
    }

    if (kr->count >= CSM_KEYRING_MAX_KEYS)
    {
        return -1;
    }

    csm_keyring_entry *entry = &kr->entries[kr->count++];
    entry->id = id;
    memcpy(entry->key, key, len);
    entry->key_len = len;
    return 0;
}

const uint8_t *csm_keyring_find(const csm_keyring *kr, uint8_t id)
{
    if (!kr)
    {
        return NULL;
    }

    for (uint8_t i = 0U; i < kr->count; i++)
    {
        if (kr->entries[i].id == id)
        {
            return kr->entries[i].key;
        }
    }

    return NULL;
}
