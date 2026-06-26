/**
 * \file csm_security_suite.h
 *
 * \brief Security suite definitions for COSEM authentication
 *
 *  IEC 62056-5-3 Table 28 — Security Suites:
 *    0: AES-GCM-128 (LSG)
 *    1: AES-GCM-128 (LSG)
 *    2: AES-GCM-128 (LSG)
 *    3: AES-GCM-128 (LSG)
 *    4: AES-GCM-128 (LSG)
 *    5: AES-GCM-128 (LSG)
 *    6-7: reserved
 *    8: Kuznyechik-GCM + Streebog-256 CMAC (GOST)
 *    9: Kuznyechik-GCM + Streebog-256 CMAC (GOST)
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef CSM_SECURITY_SUITE_H
#define CSM_SECURITY_SUITE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    /* AES-GCM suites (IEC 62056-5-3) */
    CSM_SEC_SUITE_ID_0 = 0,
    CSM_SEC_SUITE_ID_1 = 1,
    CSM_SEC_SUITE_ID_2 = 2,
    CSM_SEC_SUITE_ID_3 = 3,
    CSM_SEC_SUITE_ID_4 = 4,
    CSM_SEC_SUITE_ID_5 = 5,

    /* Reserved */
    CSM_SEC_SUITE_ID_6 = 6,
    CSM_SEC_SUITE_ID_7 = 7,

    /* GOST suites (RFC 7838) */
    CSM_SEC_SUITE_ID_8 = 8,  /* Kuznyechik-GCM + Streebog-256 CMAC */
    CSM_SEC_SUITE_ID_9 = 9,  /* Kuznyechik-GCM + Streebog-256 CMAC */
} csm_sec_suite_id;

/**
 * \brief Cipher algorithm identifier
 */
typedef enum
{
    CSM_CIPHER_AES_GCM = 0,
    CSM_CIPHER_KUZNYECHIK_GCM = 8,
} csm_cipher_id;

/**
 * \brief MAC algorithm identifier
 */
typedef enum
{
    CSM_MAC_AES_GMAC = 0,
    CSM_MAC_STREEBOG_256_CMAC = 8,
} csm_mac_id;

/**
 * \brief Get cipher and MAC algorithm for a security suite
 *
 * \param suite     Security suite ID
 * \param cipher    Output: cipher algorithm
 * \param mac       Output: MAC algorithm
 * \return 0 on success, -1 if suite not supported
 */
int csm_sec_suite_get_algorithms(uint8_t suite, csm_cipher_id *cipher, csm_mac_id *mac);

/**
 * \brief Check if a security suite is supported
 */
int csm_sec_suite_is_supported(uint8_t suite);

#ifdef __cplusplus
}
#endif

#endif /* CSM_SECURITY_SUITE_H */
