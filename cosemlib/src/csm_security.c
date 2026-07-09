/**
 * Cosem security layer functions to (de)cipher and authenticate packets
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 * SECURITY NOTE: Keys are accessed via csm_sys_get_key() which returns a pointer
 * into HAL-provided storage. The library does NOT zeroize key material after use.
 * HAL implementations MUST provide secure key storage and zeroize keys when they
 * are no longer needed. On platforms without secure memory, copy keys to a stack
 * buffer, use them, and zeroize the stack buffer explicitly.
 *
 */

#include "csm_security.h"
#include "os_util.h"
#include <string.h>

static int csm_sec_get_sap_u8(const csm_request *request, uint8_t *sap) {
	if ((request == NULL) || (sap == NULL) || (request->llc.dsap > 0xFFU)) {
		return FALSE;
	}
	*sap = (uint8_t)request->llc.dsap;
	return TRUE;
}

csm_sec_result csm_sec_auth_decrypt(csm_array *array, csm_request *request, const uint8_t *system_title) {
	csm_sec_result retcode = CSM_SEC_OK;
	csm_sec_control_byte sc;
	uint32_t ic;
	uint8_t *tag_read = NULL;
	uint32_t data_size = 0U;
	uint32_t aad_size = 0U;
	uint8_t IV[12];
	uint8_t sap;

	if ((array == NULL) || (request == NULL) || (system_title == NULL)) {
		return CSM_SEC_ERROR;
	}
	if (!csm_array_read_u8(array, &sc.sh_byte) || !csm_array_read_u32(array, &ic) || (array->rd_index < 17U)) {
		return CSM_SEC_ERROR;
	}

	if (!csm_sec_get_sap_u8(request, &sap)) {
		return CSM_SEC_ERROR;
	}

	// Prepare IV
	memcpy(&IV[0], &system_title[0], CSM_DEF_APP_TITLE_SIZE);
	PUT_BE32(&IV[CSM_DEF_APP_TITLE_SIZE], ic);

	uint8_t *data = csm_array_rd_data(array);  // point to the information or tag
	if (data == NULL) {
		return CSM_SEC_ERROR;
	}

	uint32_t unread = csm_array_unread(array);  // size of information + tag

	// We have saved the security header (SC + IC), now  override this header (and beyond) with the AAD
	// The AAD is composed with the SC || AK || information. Information can be null.

	uint8_t *aad = (data - 17U);  // pointer to the begining of AAD
	aad[0] = sc.sh_byte;
	uint8_t *auth_key = csm_sys_get_key(sap, CSM_SEC_GAK);
	if (auth_key == NULL) {
		return CSM_SEC_ERROR;
	}
	memcpy(&aad[1], auth_key, 16U);


	if (sc.sh_bit_field.encryption) {
		CSM_LOG("[SEC] Encryption enabled");

		data_size = unread;
		aad_size = 0U;

		if (sc.sh_bit_field.authentication) {
			CSM_LOG("[SEC] Authentication enabled");
			// E + A: size must be higher than the tag
			if (unread > 12U) {
				data_size -= 12U;
				aad_size += 17U;
				tag_read = data + data_size;
			} else {
				CSM_ERR("[SEC] Bad packet size for deciphering");
				retcode = CSM_SEC_ERROR;
			}
		}

	} else if (sc.sh_bit_field.authentication) {
		CSM_LOG("[SEC] Authentication only");
		if (unread >= 12U) {
			data_size = (unread - 12U);
			aad_size = 17U + data_size;  // SC + AK size + information size
			tag_read = data + data_size;
		} else {
			CSM_ERR("[SEC] Bad packet size for auth");
			retcode = CSM_SEC_ERROR;
		}

		data_size = 0U;  // No data to decipher
	} else {
		// No any encryption/authentication
		data_size = 0U;
		aad_size = 0U;
	}

	if (retcode != CSM_SEC_OK) {
		return retcode;
	}

	if (csm_sys_gcm_init(request->channel_id, sap, CSM_SEC_GUEK, CSM_SEC_DECRYPT, IV, aad, aad_size) == 0) {
		return CSM_SEC_ERROR;
	}

	// Decrypt in place
	if (csm_sys_gcm_update(request->channel_id, data, data_size, data) == 0) {
		return CSM_SEC_ERROR;
	}

	uint8_t tag[16U];
	if (csm_sys_gcm_finish(request->channel_id, tag) == 0) {
		return CSM_SEC_ERROR;
	}

	if ((tag_read != NULL) && (retcode == CSM_SEC_OK)) {
		// Constant-time tag comparison to prevent timing side-channel attacks.
		// memcmp() short-circuits on first mismatch, leaking which byte is correct.
		uint8_t diff = 0U;
		for (uint32_t i = 0U; i < 12U; i++) {
			diff |= tag[i] ^ tag_read[i];
		}
		if (diff != 0U) {
			retcode = CSM_SEC_AUTH_FAILURE;
		}
	}

	return retcode;
}

csm_sec_result csm_sec_auth_encrypt(csm_array *array, csm_request *request, const uint8_t *system_title, csm_sec_control_byte sc, uint32_t ic) {
	csm_sec_result retcode = CSM_SEC_OK;
	uint8_t *tag_ptr = NULL;
	uint32_t data_size = 0U;
	uint32_t aad_size = 0U;
	uint8_t IV[12];
	uint8_t sap;

	if ((array == NULL) || (request == NULL) || (system_title == NULL) || (array->rd_index < 17U)) {
		return CSM_SEC_ERROR;
	}

	if (!csm_sec_get_sap_u8(request, &sap)) {
		return CSM_SEC_ERROR;
	}

	// Prepare IV
	memcpy(&IV[0], &system_title[0], CSM_DEF_APP_TITLE_SIZE);
	PUT_BE32(&IV[CSM_DEF_APP_TITLE_SIZE], ic);

	uint8_t *data = csm_array_rd_data(array);  // point to the information
	if (data == NULL) {
		return CSM_SEC_ERROR;
	}
	uint32_t unread = csm_array_unread(array);  // size of information

	// We have saved the security header (SC + IC), now  override this header (and beyond) with the AAD
	// The AAD is composed with the SC || AK || information. Information can be null.

	uint8_t *aad = (data - 17U);  // pointer to the begining of AAD
	aad[0] = sc.sh_byte;
	uint8_t *auth_key = csm_sys_get_key(sap, CSM_SEC_GAK);
	if (auth_key == NULL) {
		return CSM_SEC_ERROR;
	}
	memcpy(&aad[1], auth_key, 16U);

	if (sc.sh_bit_field.encryption) {
		CSM_LOG("[SEC] Encryption enabled");

		data_size = unread;
		aad_size = 0U;

		if (sc.sh_bit_field.authentication) {
			CSM_LOG("[SEC] Authentication enabled");
			// E + A: size must be higher than the tag
			if (unread > 12U) {
				data_size -= 12U;
				aad_size += 17U;
				tag_ptr = data + data_size;
			} else {
				CSM_ERR("[SEC] Bad packet size for deciphering");
				retcode = CSM_SEC_ERROR;
			}
		}
	} else if (sc.sh_bit_field.authentication) {
		CSM_LOG("[SEC] Authentication only");
		if (unread > 0U) {
			data_size = unread;
			aad_size = 17U + data_size;  // SC + AK size + information size
			tag_ptr = data + data_size;
		} else {
			CSM_ERR("[SEC] Bad packet size for authentication");
			retcode = CSM_SEC_ERROR;
		}

		data_size = 0U;  // No data to encrypt
	} else {
		// No any encryption/authentication
		data_size = 0U;
		aad_size = 0U;
	}

	if (retcode != CSM_SEC_OK) {
		return retcode;
	}

	if (csm_sys_gcm_init(request->channel_id, sap, CSM_SEC_GUEK, CSM_SEC_ENCRYPT, IV, aad, aad_size) == 0) {
		return CSM_SEC_ERROR;
	}

	// Encrypt in place
	if (csm_sys_gcm_update(request->channel_id, data, data_size, data) == 0) {
		return CSM_SEC_ERROR;
	}

	uint8_t tag[16U];
	if (csm_sys_gcm_finish(request->channel_id, tag) == 0) {
		return CSM_SEC_ERROR;
	}

	csm_array_writer_jump(array, data_size);  // Jump over crypted data

	if ((tag_ptr != NULL) && (retcode == CSM_SEC_OK)) {
		// Insert the tag
		csm_array_write_buff(array, tag, 12U);
	}

	return retcode;
}
