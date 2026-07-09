/**

 * reader_hal — desktop / embedded HAL template for opendlms_reader.

 *

 * Implement csm_hal_* / csm_sys_* here or copy into your firmware.

 *

 * Copyright (c) 2026, OpenDLMS — MIT License

 */



#ifndef READER_HAL_H

#define READER_HAL_H



#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



void reader_hal_init(void);



/** Calls csm_sys_init() — invoke once before connect. */

void reader_hal_sys_init(void);



/** Set 8-octet LLS password returned by csm_hal_get_lls_password for @p sap. */

void reader_hal_set_lls_password(uint8_t sap, const char *password);



/**

 * Load GUEK/GAK (and optional KEK) into keyring for @p sap.

 * Hex strings: 32 digits = 16 bytes; NULL skips that key.

 * @return 0 on success, -1 on parse error.

 */

int reader_hal_keyring_set_hex(uint8_t sap, const char *guek_hex, const char *gak_hex, const char *kek_hex);



/** Dedicated key for ciphered AARQ InitiateRequest (16-byte hex). */

int reader_hal_set_dedicated_key_hex(uint8_t sap, const char *ded_hex);



/** Seed csm_hal_load_ic() for @p sap (default 0 until set or synced from meter). */

void reader_hal_set_invocation_counter(uint8_t sap, uint32_t ic);



/** Set 8-octet client system title (AARQ calling AP-title, HLS5 GMAC IV). */

void reader_hal_set_system_title(const uint8_t title[8]);



#ifdef __cplusplus

}

#endif



#endif /* READER_HAL_H */

