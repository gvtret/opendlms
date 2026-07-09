/**
 * \file csm_security_suite.c
 * \brief Security suite implementation
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "csm_security_suite.h"

int csm_sec_suite_get_algorithms(uint8_t suite, csm_cipher_id *cipher, csm_mac_id *mac) {
	if (cipher == NULL || mac == NULL) {
		return -1;
	}

	switch (suite) {
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
			/* GOST suite 8 (R 1323565.1.028): Kuznyechik CTR + CMAC */
			*cipher = CSM_CIPHER_KUZNYECHIK_CTR;
			*mac = CSM_MAC_KUZNYECHIK_CMAC;
			return 0;

		case 9:
			/* Suite 9 additionally requires GOST 34.10-2018 signature,
         * VKO-256 key agreement, and GOST 34.11-2018 (Streebog-256).
         * These are not production-ready — fail closed. */
			return -1;

		default:
			return -1;
	}
}

int csm_sec_suite_is_supported(uint8_t suite) {
	csm_cipher_id cipher;
	csm_mac_id mac;
	return csm_sec_suite_get_algorithms(suite, &cipher, &mac) == 0 ? 1 : 0;
}
