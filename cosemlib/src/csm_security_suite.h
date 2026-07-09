/**
 * \file csm_security_suite.h
 *
 * \brief Security suite definitions for COSEM authentication
 *
 *  IEC 62056-5-3 Table 28 — Security Suites:
 *    0-5: AES-GCM-128 (LSG)
 *    6-7: reserved
 *
 *  GOST suites per R 1323565.1.028 §4 (see also RFC 9189):
 *    8: KUZN-CTR-CMAC — Kuznyechik (GOST 34.12-2018) CTR + CMAC.
 *    9: adds GOST 34.10-2018 signature, VKO-256 key agreement, and
 *       GOST 34.11-2018 (Streebog-256) hash.
 *
 *  Suite 9 is reported unsupported here: its signature / key-agreement /
 *  Streebog-256 primitives are not production-ready in this build.
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

typedef enum {
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

	/* GOST suites (R 1323565.1.028) */
	CSM_SEC_SUITE_ID_8 = 8, /* KUZN-CTR-CMAC */
	CSM_SEC_SUITE_ID_9 = 9, /* + GOST 34.10 signature / VKO / Streebog-256 */
} csm_sec_suite_id;

/**
 * \brief Cipher algorithm identifier
 */
typedef enum {
	CSM_CIPHER_AES_GCM = 0,
	CSM_CIPHER_KUZNYECHIK_CTR = 8,
} csm_cipher_id;

/**
 * \brief MAC algorithm identifier
 */
typedef enum {
	CSM_MAC_AES_GMAC = 0,
	CSM_MAC_KUZNYECHIK_CMAC = 8,
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
