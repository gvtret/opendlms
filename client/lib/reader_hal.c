/**
 * reader_hal — HAL template for opendlms_reader (desktop + embeddable).
 */

#include "reader_hal.h"

#include "csm_config.h"
#include "csm_association.h"
#include "csm_definitions.h"
#include "csm_keyring.h"

#include "gcm.h"
#include "md5.h"
#include "sha1.h"
#include "sha256.h"

#include <stdio.h>
#include <string.h>
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#define READER_HAL_MAX_SAP 64U
#define READER_HAL_MAX_CHANNELS 4U

static uint8_t reader_system_title[CSM_DEF_APP_TITLE_SIZE] = {
    0x4DU, 0x4DU, 0x4DU, 0x00U, 0x00U, 0xBCU, 0x61U, 0x4EU
};

static csm_keyring reader_keyring;
static uint8_t     reader_keyring_initialized = 0U;

static char reader_lls_password[READER_HAL_MAX_SAP][9];
static uint8_t reader_lls_valid[READER_HAL_MAX_SAP];

static uint8_t reader_ded_key[READER_HAL_MAX_SAP][16];
static uint8_t reader_ded_valid[READER_HAL_MAX_SAP];

static uint32_t reader_ic_store[16];

static mbedtls_gcm_context reader_gcm_ctx[READER_HAL_MAX_CHANNELS];
static uint8_t reader_gcm_active[READER_HAL_MAX_CHANNELS];

static int reader_hex_value(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        return ch - '0';
    }
    if ((ch >= 'a') && (ch <= 'f'))
    {
        return 10 + (ch - 'a');
    }
    if ((ch >= 'A') && (ch <= 'F'))
    {
        return 10 + (ch - 'A');
    }
    return -1;
}

static int reader_parse_hex16(const char *hex, uint8_t *out)
{
    if ((hex == NULL) || (out == NULL))
    {
        return -1;
    }

    if (strlen(hex) != 32U)
    {
        return -1;
    }

    for (uint32_t i = 0U; i < 16U; i++)
    {
        int hi = reader_hex_value(hex[i * 2U]);
        int lo = reader_hex_value(hex[(i * 2U) + 1U]);
        if ((hi < 0) || (lo < 0))
        {
            return -1;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return 0;
}

static int reader_hal_random_byte(uint8_t *out)
{
    if (out == NULL)
    {
        return 0;
    }

#if defined(_WIN32) || defined(WIN32)
    HCRYPTPROV provider = 0;
    if (!CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        return 0;
    }
    BOOL ok = CryptGenRandom(provider, 1U, out);
    CryptReleaseContext(provider, 0U);
    return ok ? 1 : 0;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        return 0;
    }
    ssize_t got = read(fd, out, 1U);
    close(fd);
    return (got == 1) ? 1 : 0;
#endif
}

void reader_hal_init(void)
{
    if (!reader_keyring_initialized)
    {
        csm_keyring_init(&reader_keyring);
        reader_keyring_initialized = 1U;
    }
}

void reader_hal_set_lls_password(uint8_t sap, const char *password)
{
    uint32_t idx = (uint32_t)sap;

    if (idx >= READER_HAL_MAX_SAP)
    {
        return;
    }

    if ((password == NULL) || (password[0] == '\0'))
    {
        reader_lls_valid[idx] = 0U;
        reader_lls_password[idx][0] = '\0';
        return;
    }

    (void)snprintf(reader_lls_password[idx], sizeof(reader_lls_password[idx]), "%s", password);
    reader_lls_valid[idx] = 1U;
}

int reader_hal_keyring_set_hex(uint8_t sap,
                               const char *guek_hex,
                               const char *gak_hex,
                               const char *kek_hex)
{
    uint8_t key[16];

    reader_hal_init();

    if (guek_hex != NULL)
    {
        if (reader_parse_hex16(guek_hex, key) != 0)
        {
            return -1;
        }
        if (csm_keyring_add(&reader_keyring, (uint8_t)CSM_SEC_GUEK, key, 16U) != 0)
        {
            return -1;
        }
    }

    if (gak_hex != NULL)
    {
        if (reader_parse_hex16(gak_hex, key) != 0)
        {
            return -1;
        }
        if (csm_keyring_add(&reader_keyring, (uint8_t)CSM_SEC_GAK, key, 16U) != 0)
        {
            return -1;
        }
    }

    if (kek_hex != NULL)
    {
        if (reader_parse_hex16(kek_hex, key) != 0)
        {
            return -1;
        }
        if (csm_keyring_add(&reader_keyring, (uint8_t)CSM_SEC_KEK, key, 16U) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int reader_hal_set_dedicated_key_hex(uint8_t sap, const char *ded_hex)
{
    uint32_t idx = (uint32_t)sap;

    if ((idx >= READER_HAL_MAX_SAP) || (ded_hex == NULL))
    {
        return -1;
    }

    if (reader_parse_hex16(ded_hex, reader_ded_key[idx]) != 0)
    {
        reader_ded_valid[idx] = 0U;
        return -1;
    }

    reader_ded_valid[idx] = 1U;
    return 0;
}

uint8_t csm_hal_get_dedicated_key(uint8_t sap, uint8_t *buf, uint8_t buf_cap)
{
    uint32_t idx = (uint32_t)sap;

    if ((buf == NULL) || (buf_cap < 16U) || (idx >= READER_HAL_MAX_SAP) ||
        (reader_ded_valid[idx] == 0U))
    {
        return 0U;
    }

    memcpy(buf, reader_ded_key[idx], 16U);
    return 16U;
}

void csm_sys_init(void) {}

void reader_hal_sys_init(void)
{
    csm_sys_init();
}

const uint8_t *csm_sys_get_system_title(void)
{
    return reader_system_title;
}

uint8_t csm_sys_get_key_len(uint8_t sap, csm_sec_key key_id)
{
    uint8_t len = 16U;

    if (!reader_keyring_initialized)
    {
        return len;
    }
    (void)sap;
    if (csm_keyring_find(&reader_keyring, (uint8_t)key_id) == NULL)
    {
        return 16U;
    }
    return (len != 0U) ? len : 16U;
}

void csm_sys_apply_security_suite(uint8_t sap, uint8_t sym_key_len, uint8_t kek_len)
{
    (void)sap;
    (void)sym_key_len;
    (void)kek_len;
}

uint8_t *csm_sys_get_key(uint8_t sap, csm_sec_key key_id)
{
    reader_hal_init();
    (void)sap;
    return (uint8_t *)csm_keyring_find(&reader_keyring, (uint8_t)key_id);
}

uint8_t csm_sys_get_mechanism_id(uint8_t sap)
{
    (void)sap;
    return CSM_AUTH_LOWEST_LEVEL;
}

void csm_hal_md5(const uint8_t *input, uint32_t size, uint8_t *output)
{
    mbedtls_md5(input, size, output);
}

void csm_hal_sha1(const uint8_t *input, uint32_t size, uint8_t *output)
{
    mbedtls_sha1(input, size, output);
}

void csm_hal_sha256(const uint8_t *input, uint32_t size, uint8_t *output)
{
    mbedtls_sha256(input, size, output, 0);
}

int csm_sys_gcm_init(uint8_t channel, uint8_t sap, csm_sec_key key_id, csm_sec_mode mode,
                     const uint8_t *iv, const uint8_t *aad, uint32_t aad_len)
{
    uint8_t *key = csm_sys_get_key(sap, key_id);
    if ((channel >= READER_HAL_MAX_CHANNELS) || (key == NULL) || (iv == NULL))
    {
        return 0;
    }

    int mbed_mode = (mode == CSM_SEC_ENCRYPT) ? MBEDTLS_GCM_ENCRYPT : MBEDTLS_GCM_DECRYPT;

    if (reader_gcm_active[channel] != 0U)
    {
        mbedtls_gcm_free(&reader_gcm_ctx[channel]);
        reader_gcm_active[channel] = 0U;
    }

    mbedtls_gcm_init(&reader_gcm_ctx[channel]);
    {
        unsigned int key_bits = (csm_sys_get_key_len(sap, key_id) == 32U) ? 256U : 128U;
        if (mbedtls_gcm_setkey(&reader_gcm_ctx[channel], MBEDTLS_CIPHER_ID_AES,
                               key, key_bits) != 0)
        {
            mbedtls_gcm_free(&reader_gcm_ctx[channel]);
            return 0;
        }
    }
    if (mbedtls_gcm_starts(&reader_gcm_ctx[channel], mbed_mode, iv, 12, aad, aad_len) != 0)
    {
        mbedtls_gcm_free(&reader_gcm_ctx[channel]);
        return 0;
    }

    reader_gcm_active[channel] = 1U;
    return 1;
}

int csm_sys_gcm_update(uint8_t channel, const uint8_t *plain, uint32_t plain_len, uint8_t *crypt)
{
    if ((channel >= READER_HAL_MAX_CHANNELS) || (reader_gcm_active[channel] == 0U) ||
        ((plain_len > 0U) && ((plain == NULL) || (crypt == NULL))))
    {
        return 0;
    }
    return (mbedtls_gcm_update(&reader_gcm_ctx[channel], plain_len, plain, crypt) == 0) ? 1 : 0;
}

int csm_sys_gcm_finish(uint8_t channel, uint8_t *tag)
{
    if ((channel >= READER_HAL_MAX_CHANNELS) || (reader_gcm_active[channel] == 0U) || (tag == NULL))
    {
        return 0;
    }
    int rc = mbedtls_gcm_finish(&reader_gcm_ctx[channel], tag, 16);
    mbedtls_gcm_free(&reader_gcm_ctx[channel]);
    reader_gcm_active[channel] = 0U;
    return (rc == 0) ? 1 : 0;
}

uint8_t csm_hal_get_random_u8(uint8_t min, uint8_t max)
{
    uint8_t value = 0U;
    uint16_t span;
    uint16_t limit;

    if (max <= min)
    {
        return min;
    }

    span = (uint16_t)max - (uint16_t)min + 1U;
    limit = (uint16_t)(256U - (256U % span));
    for (uint8_t attempt = 0U; attempt < 32U; attempt++)
    {
        if (!reader_hal_random_byte(&value))
        {
            break;
        }
        if ((uint16_t)value < limit)
        {
            return (uint8_t)(min + (value % span));
        }
    }

    return min;
}

void csm_hal_get_lls_password(uint8_t sap, uint8_t *array, uint8_t max_size)
{
    uint32_t idx = (uint32_t)sap;
    uint8_t  len;
    uint8_t  i;

    if ((array == NULL) || (max_size == 0U))
    {
        return;
    }

    if ((idx < READER_HAL_MAX_SAP) && (reader_lls_valid[idx] != 0U))
    {
        len = (uint8_t)strlen(reader_lls_password[idx]);
        if (len > max_size)
        {
            len = max_size;
        }
        memcpy(array, reader_lls_password[idx], len);
        for (i = len; i < max_size; i++)
        {
            array[i] = 0x30U;
        }
        return;
    }

    {
        uint8_t dflt[] = { 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x30U };
        len            = (max_size < 8U) ? max_size : 8U;
        memcpy(array, dflt, len);
    }
}

void reader_hal_set_invocation_counter(uint8_t sap, uint32_t ic)
{
    uint32_t idx = (uint32_t)sap & 0x0FU;

    reader_ic_store[idx] = ic;
}

void reader_hal_set_system_title(const uint8_t title[8])
{
    if (title != NULL)
    {
        memcpy(reader_system_title, title, CSM_DEF_APP_TITLE_SIZE);
    }
}

uint32_t csm_hal_load_ic(uint8_t sap)
{
    uint32_t idx = (uint32_t)sap & 0x0FU;

    return reader_ic_store[idx];
}

void csm_hal_save_ic(uint8_t sap, uint32_t ic)
{
    uint32_t idx = (uint32_t)sap & 0x0FU;

    reader_ic_store[idx] = ic;
}

int csm_hal_decode_selective_access(csm_request *request, csm_array *array)
{
    if ((request == NULL) || (array == NULL))
    {
        return FALSE;
    }
    request->db_request.sel_access.data = *array;
    return TRUE;
}
