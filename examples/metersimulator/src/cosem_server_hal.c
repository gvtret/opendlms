// Cosem library
#include "csm_config.h"
#include "csm_association.h"
#include "csm_definitions.h"

// OS/System definitions
#include "os_util.h"
#include "server_config.h"

// Ciphering library
#include "gcm.h"

// File system
#include "fs.h"

// Standard libraries
#include <string.h>
#include <stdlib.h>

/*
the  leading  (i.e.  the  leftmost)  64  bits  (8  octets)  shall  hold  the  fixed  field.  It  shall  contain  the
system title, see 4.3.4;
•     the trailing (i.e. the rightmost) 32 bits shall hold the invocation field. The invocation field shall be
an integer counter.
*/

static uint8_t system_title[CSM_DEF_APP_TITLE_SIZE] = { 0x4DU, 0x4DU, 0x4DU, 0x00U, 0x00U, 0xBCU, 0x61U, 0x4EU }; // GreenBook server example

// FIXME:
// 1. Store the key in a configuration file as they can be updated on the field
// 2. Create a key-ring per association (SAP)

// Master key, common for all the associations, not changeable
static uint8_t key_kek[16] = { 0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU };

static uint8_t key_guek[16] = { 0x00U,0x01U,0x02U,0x03U,0x04U,0x05U,0x06U,0x07U,0x08U,0x09U,0x0AU,0x0BU,0x0CU,0x0DU,0x0EU,0x0FU };
static uint8_t key_gak[16] = { 0xD0U,0xD1U,0xD2U,0xD3U,0xD4U,0xD5U,0xD6U,0xD7U,0xD8U,0xD9U,0xDAU,0xDBU,0xDCU,0xDDU,0xDEU,0xDFU };

typedef struct
{
    uint8_t sap; //!< Sap number of the association
    uint8_t guek[16];
    uint8_t gbek[16];
    uint8_t gak[16];
    uint8_t lls_password[CSM_DEF_LLS_MAX_SIZE]; // Password.
    uint8_t mechanism_id;
    uint8_t security_policy;
} cfg_cosem;

static const cfg_cosem cDefaultSap[] = {
    {
        1U,
        { 0x00U,0x01U,0x02U,0x03U,0x04U,0x05U,0x06U,0x07U,0x08U,0x09U,0x0AU,0x0BU,0x0CU,0x0DU,0x0EU,0x0FU },
        { 0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU },
        { 0xD0U,0xD1U,0xD2U,0xD3U,0xD4U,0xD5U,0xD6U,0xD7U,0xD8U,0xD9U,0xDAU,0xDBU,0xDCU,0xDDU,0xDEU,0xDFU },
        { 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x31U },
        CSM_AUTH_LOW_LEVEL,
        0U,
    },
    {
        16U,
        { 0x00U,0x01U,0x02U,0x03U,0x04U,0x05U,0x06U,0x07U,0x08U,0x09U,0x0AU,0x0BU,0x0CU,0x0DU,0x0EU,0x0FU },
        { 0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU },
        { 0xD0U,0xD1U,0xD2U,0xD3U,0xD4U,0xD5U,0xD6U,0xD7U,0xD8U,0xD9U,0xDAU,0xDBU,0xDCU,0xDDU,0xDEU,0xDFU },
        { 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x30U, 0x31U },
        CSM_AUTH_LOW_LEVEL,
        0U,
    }
};

#define CFG_COSEM_NB_ASSOS  (sizeof(cDefaultSap)/sizeof(cDefaultSap[0]))

static const cfg_cosem *find_sap_config(uint8_t sap)
{
    for (uint32_t i = 0U; i < CFG_COSEM_NB_ASSOS; i++)
    {
        if (cDefaultSap[i].sap == sap)
        {
            return &cDefaultSap[i];
        }
    }
    return NULL;
}

// Keep a context by channel to be thread safe
mbedtls_gcm_context chan_ctx[NUMBER_OF_CHANNELS];
static uint8_t chan_ctx_active[NUMBER_OF_CHANNELS];

void csm_sys_set_system_title(const uint8_t *buf)
{
    if (buf != NULL)
    {
        memcpy(system_title, buf, sizeof(system_title));
    }
}


typedef enum
{
    CSM_SEC_IC_CLIENT,
    CSM_SEC_IC_SERVER,
} csm_sec_ic;


// FIXME: create one pair IC per SAP
static uint32_t gIc = 0x01234567U;

uint32_t csm_sys_get_ic(uint8_t sap, csm_sec_ic ic)
{
    (void) sap;
    (void) ic;

    uint32_t value = gIc;
    gIc++;
    return value;
}

const uint8_t *csm_sys_get_system_title()
{
    return system_title;
}

uint8_t *csm_sys_get_key(uint8_t sap, csm_sec_key key_id)
{
    const cfg_cosem *cfg = find_sap_config(sap);
    uint8_t *key = NULL;

    switch(key_id)
    {
    case CSM_SEC_KEK:
        key = key_kek;
        break;
    case CSM_SEC_GUEK:
        key = (cfg != NULL) ? (uint8_t *)cfg->guek : key_guek;
        break;
    case CSM_SEC_GBEK:
        key = (cfg != NULL) ? (uint8_t *)cfg->gbek : key_kek;
        break;
    default:
    case CSM_SEC_GAK:
        key = (cfg != NULL) ? (uint8_t *)cfg->gak : key_gak;
        break;
    }

    return key;
}


int csm_sys_gcm_init(uint8_t channel, uint8_t sap, csm_sec_key key_id, csm_sec_mode mode, const uint8_t *iv, const uint8_t *aad, uint32_t aad_len)
{
    uint8_t *key = csm_sys_get_key(sap, key_id);
    if ((channel >= NUMBER_OF_CHANNELS) || (key == NULL) || (iv == NULL))
    {
        return FALSE;
    }

    int mbed_mode = (mode == CSM_SEC_ENCRYPT) ? MBEDTLS_GCM_ENCRYPT : MBEDTLS_GCM_DECRYPT;
    if (chan_ctx_active[channel] != 0U)
    {
        mbedtls_gcm_free(&chan_ctx[channel]);
        chan_ctx_active[channel] = 0U;
    }
    mbedtls_gcm_init(&chan_ctx[channel]);
    if (mbedtls_gcm_setkey(&chan_ctx[channel], MBEDTLS_CIPHER_ID_AES, key, 128) != 0)
    {
        mbedtls_gcm_free(&chan_ctx[channel]);
        return FALSE;
    }
    int res = mbedtls_gcm_starts(&chan_ctx[channel], mbed_mode, iv, 12, aad, aad_len);
    if (res == 0)
    {
        chan_ctx_active[channel] = 1U;
    }
    else
    {
        mbedtls_gcm_free(&chan_ctx[channel]);
    }
    return (res == 0) ? TRUE : FALSE;
}

int csm_sys_gcm_update(uint8_t channel, const uint8_t *plain, uint32_t plain_len, uint8_t *crypt)
{
    if ((channel >= NUMBER_OF_CHANNELS) || (chan_ctx_active[channel] == 0U) ||
        ((plain_len > 0U) && ((plain == NULL) || (crypt == NULL))))
    {
        return FALSE;
    }
    return (mbedtls_gcm_update(&chan_ctx[channel], plain_len, plain, crypt) == 0) ? TRUE : FALSE;
}

// Sizes are total sizes of plain and AAD
int csm_sys_gcm_finish(uint8_t channel, uint8_t *tag)
{
    if ((channel >= NUMBER_OF_CHANNELS) || (chan_ctx_active[channel] == 0U) || (tag == NULL))
    {
        return FALSE;
    }
    int rc = mbedtls_gcm_finish(&chan_ctx[channel], tag, 16);
    mbedtls_gcm_free(&chan_ctx[channel]);
    chan_ctx_active[channel] = 0U;
    return (rc == 0) ? TRUE : FALSE;
}

static const uint8_t default_password[CSM_DEF_LLS_MAX_SIZE] = { 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U };

void csm_hal_get_lls_password(uint8_t sap, uint8_t *array, uint8_t max_size)
{
    const cfg_cosem *cfg = find_sap_config(sap);
    const uint8_t *password = (cfg != NULL) ? cfg->lls_password : default_password;
    uint8_t size = (max_size < CSM_DEF_LLS_MAX_SIZE) ? max_size : CSM_DEF_LLS_MAX_SIZE;

    if ((array != NULL) && (size > 0U))
    {
        memcpy(array, password, size);
    }
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


uint8_t csm_sys_get_mechanism_id(uint8_t sap)
{
    const cfg_cosem *cfg = find_sap_config(sap);

    if (cfg != NULL)
    {
        return cfg->mechanism_id;
    }
    return CSM_AUTH_LOWEST_LEVEL;
}

// TODO: Write a note on the randomize function, it should be NIST compliant (use a target-dependant implementation)
uint8_t csm_hal_get_random_u8(uint8_t min, uint8_t max)
{
    if (max <= min)
    {
        return min;
    }
    return min + rand() % ((max + 1) - min);
}


// ==================================== FS FUNCTIONS ====================================
