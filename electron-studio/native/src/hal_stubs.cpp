/**
 * \file hal_stubs.cpp
 * \brief HAL/system stubs for native addon linking
 */

#include "csm_definitions.h"
#include <cstring>

static uint8_t g_system_title[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

void csm_sys_set_system_title(const uint8_t *buf)
{
    if (buf) memcpy(g_system_title, buf, 8);
}

const uint8_t *csm_sys_get_system_title(void)
{
    return g_system_title;
}

void csm_hal_get_lls_password(uint8_t sap, uint8_t *array, uint8_t max_size)
{
    (void)sap;
    if (array && max_size > 0) memset(array, 0, max_size);
}

uint8_t csm_hal_get_random_u8(uint8_t min, uint8_t max)
{
    (void)min;
    (void)max;
    return 0;
}

int csm_hal_decode_selective_access(csm_request *request, csm_array *array)
{
    (void)request;
    (void)array;
    return 0;
}

uint8_t csm_sys_get_mechanism_id(uint8_t sap)
{
    (void)sap;
    return 0;
}

uint8_t *csm_sys_get_key(uint8_t sap, csm_sec_key key_id)
{
    (void)sap;
    (void)key_id;
    static uint8_t empty_key[16] = {0};
    return empty_key;
}

void csm_hal_md5(const uint8_t *input, uint32_t size, uint8_t *output)
{
    (void)input;
    (void)size;
    if (output) memset(output, 0, 16);
}

void csm_hal_sha1(const uint8_t *input, uint32_t size, uint8_t *output)
{
    (void)input;
    (void)size;
    if (output) memset(output, 0, 20);
}

void csm_hal_sha256(const uint8_t *input, uint32_t size, uint8_t *output)
{
    (void)input;
    (void)size;
    if (output) memset(output, 0, 32);
}

int csm_sys_gcm_init(uint8_t channel, uint8_t sap, csm_sec_key key_id,
                      csm_sec_mode mode, const uint8_t *iv,
                      const uint8_t *aad, uint32_t aad_len)
{
    (void)channel;
    (void)sap;
    (void)key_id;
    (void)mode;
    (void)iv;
    (void)aad;
    (void)aad_len;
    return 0;
}

int csm_sys_gcm_update(uint8_t channel, const uint8_t *plain,
                        uint32_t plain_len, uint8_t *crypt)
{
    (void)channel;
    if (plain && crypt && plain_len > 0) memcpy(crypt, plain, plain_len);
    return 0;
}

int csm_sys_gcm_finish(uint8_t channel, uint8_t *tag)
{
    (void)channel;
    if (tag) memset(tag, 0, 16);
    return 0;
}
