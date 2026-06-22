/**
 * \file csm_keyring.h
 *
 * \brief Key management for COSEM security layers
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef CSM_KEYRING_H
#define CSM_KEYRING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSM_KEYRING_MAX_KEYS  16

typedef struct
{
    uint8_t  id;
    uint8_t  key[16];
    uint8_t  key_len;
} csm_keyring_entry;

typedef struct
{
    csm_keyring_entry entries[CSM_KEYRING_MAX_KEYS];
    uint8_t count;
} csm_keyring;

void csm_keyring_init(csm_keyring *kr);
int  csm_keyring_add(csm_keyring *kr, uint8_t id, const uint8_t *key, uint8_t len);
const uint8_t *csm_keyring_find(const csm_keyring *kr, uint8_t id);

#ifdef __cplusplus
}
#endif

#endif /* CSM_KEYRING_H */
