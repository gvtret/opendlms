/**
 * \file csm_security_suite.c
 * \brief Security suite implementation
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "csm_security_suite.h"

int csm_sec_suite_get_algorithms(uint8_t suite, csm_cipher_id *cipher, csm_mac_id *mac)
{
    if (cipher == NULL || mac == NULL)
    {
        return -1;
    }

    switch (suite)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        /* AES-GCM suites */
        *cipher = CSM_CIPHER_AES_GCM;
        *mac = CSM_MAC_AES_GMAC;
        return 0;

    case 8:
    case 9:
        /* GOST suites: Kuznyechik-GCM + Streebog-256 CMAC */
        *cipher = CSM_CIPHER_KUZNYECHIK_GCM;
        *mac = CSM_MAC_STREEBOG_256_CMAC;
        return 0;

    default:
        return -1;
    }
}

int csm_sec_suite_is_supported(uint8_t suite)
{
    csm_cipher_id cipher;
    csm_mac_id mac;
    return csm_sec_suite_get_algorithms(suite, &cipher, &mac);
}
