/**
 * \file native_hal.cpp
 * \brief HAL/system services for native addon linking
 */

#include "csm_definitions.h"
#include "cipher.h"
#include "gcm.h"
#include "md5.h"
#include "sha1.h"
#include "sha256.h"
#include <cstddef>
#include <cstring>
#include <random>

static uint8_t g_system_title[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
static constexpr uint8_t kMaxSap = 16;
static constexpr uint8_t kMaxKeys = 4;
static constexpr uint8_t kMaxGcmChannels = 16;
static constexpr uint8_t kMaxKeyLen = 32;

static uint8_t g_keys[kMaxSap][kMaxKeys][kMaxKeyLen];
static uint8_t g_key_len[kMaxSap][kMaxKeys];
static mbedtls_gcm_context g_gcm[kMaxGcmChannels];
static bool g_gcm_active[kMaxGcmChannels];

static bool valid_key_id(uint8_t key_id)
{
    return key_id < kMaxKeys;
}

static uint8_t configured_key_len(uint8_t sap, uint8_t key_id)
{
    if (sap >= kMaxSap || !valid_key_id(key_id)) return 0;
    return g_key_len[sap][key_id];
}

bool opendlms_native_set_security_key(uint8_t sap, uint8_t key_id,
                                      const uint8_t *key, size_t key_len)
{
    if (sap >= kMaxSap || !valid_key_id(key_id) || !key)
    {
        return false;
    }
    if (key_len != 16U && key_len != 32U)
    {
        return false;
    }

    memcpy(g_keys[sap][key_id], key, key_len);
    g_key_len[sap][key_id] = (uint8_t)key_len;
    return true;
}

void opendlms_native_clear_security_keys(void)
{
    memset(g_keys, 0, sizeof(g_keys));
    memset(g_key_len, 0, sizeof(g_key_len));
}

uint8_t opendlms_native_get_security_key_len(uint8_t sap, uint8_t key_id)
{
    return configured_key_len(sap, key_id);
}

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
    if (max <= min)
    {
        return min;
    }

    static std::random_device rd;
    std::uniform_int_distribution<int> dist(min, max);
    return (uint8_t)dist(rd);
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
    uint8_t id = (uint8_t)key_id;
    if (configured_key_len(sap, id) == 0U)
    {
        return nullptr;
    }
    return g_keys[sap][id];
}

void csm_hal_md5(const uint8_t *input, uint32_t size, uint8_t *output)
{
    if (!output) return;
    mbedtls_md5(input, size, output);
}

void csm_hal_sha1(const uint8_t *input, uint32_t size, uint8_t *output)
{
    if (!output) return;
    mbedtls_sha1(input, size, output);
}

void csm_hal_sha256(const uint8_t *input, uint32_t size, uint8_t *output)
{
    if (!output) return;
    mbedtls_sha256(input, size, output, 0);
}

int csm_sys_gcm_init(uint8_t channel, uint8_t sap, csm_sec_key key_id,
                      csm_sec_mode mode, const uint8_t *iv,
                      const uint8_t *aad, uint32_t aad_len)
{
    uint8_t id = (uint8_t)key_id;
    uint8_t key_len = configured_key_len(sap, id);
    if (channel >= kMaxGcmChannels || key_len == 0U || !iv)
    {
        return 0;
    }

    if (g_gcm_active[channel])
    {
        mbedtls_gcm_free(&g_gcm[channel]);
        g_gcm_active[channel] = false;
    }

    mbedtls_gcm_init(&g_gcm[channel]);
    int mbed_mode = (mode == CSM_SEC_ENCRYPT) ? MBEDTLS_GCM_ENCRYPT : MBEDTLS_GCM_DECRYPT;
    if (mbedtls_gcm_setkey(&g_gcm[channel], MBEDTLS_CIPHER_ID_AES,
                           g_keys[sap][id], (unsigned int)key_len * 8U) != 0)
    {
        mbedtls_gcm_free(&g_gcm[channel]);
        return 0;
    }
    if (mbedtls_gcm_starts(&g_gcm[channel], mbed_mode, iv, 12U, aad, aad_len) != 0)
    {
        mbedtls_gcm_free(&g_gcm[channel]);
        return 0;
    }

    g_gcm_active[channel] = true;
    return 1;
}

int csm_sys_gcm_update(uint8_t channel, const uint8_t *plain,
                        uint32_t plain_len, uint8_t *crypt)
{
    if (channel >= kMaxGcmChannels || !g_gcm_active[channel])
    {
        return 0;
    }
    if (plain_len > 0U && (!plain || !crypt))
    {
        return 0;
    }
    return mbedtls_gcm_update(&g_gcm[channel], plain_len, plain, crypt) == 0 ? 1 : 0;
}

int csm_sys_gcm_finish(uint8_t channel, uint8_t *tag)
{
    if (channel >= kMaxGcmChannels || !g_gcm_active[channel] || !tag)
    {
        return 0;
    }
    int rc = mbedtls_gcm_finish(&g_gcm[channel], tag, 16U);
    mbedtls_gcm_free(&g_gcm[channel]);
    g_gcm_active[channel] = false;
    return rc == 0 ? 1 : 0;
}
