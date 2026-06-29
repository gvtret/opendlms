/**
 * OpenDLMS reader — association auth profiles (Public / Reader / Configurator).
 *
 * Maps SPODUS three AA roles to cosemlib ACSE parameters. Keys and LLS password
 * are loaded via reader_hal.c before connect.
 *
 * Copyright (c) 2026, OpenDLMS — MIT License
 */

#ifndef OPENDLMS_READER_AUTH_H
#define OPENDLMS_READER_AUTH_H

#include "csm_association.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t auth_level;           /**< CSM_AUTH_* (csm_association.h) */
    uint8_t application_context;  /**< LN_REF (1) or LN_REF_WITH_CYPHERING (3) */
    uint8_t ciphering;            /**< Non-zero when glo-ciphering negotiated */
    uint8_t use_invocation_counter;
    csm_obis_code ic_obis;
    uint16_t ic_class_id;
    uint8_t ic_attribute_id;
} opendlms_reader_auth_t;

/** SAP 16 — no authentication, context 1. */
void opendlms_reader_auth_public(opendlms_reader_auth_t *auth);

/** SAP 32 — LLS password via csm_hal_get_lls_password (reader_hal_set_lls_password). */
void opendlms_reader_auth_reader(opendlms_reader_auth_t *auth);

/**
 * SAP 48 — HLS5 GMAC + glo-ciphering AARQ (context 3).
 * Service traffic remains fail-closed until HLS pass 3/4 support is complete.
 */
void opendlms_reader_auth_configurator(opendlms_reader_auth_t *auth);

/**
 * SAP 48 — HLS5 GMAC AARQ without glo-ciphering (context 1).
 * Service traffic remains fail-closed until HLS pass 3/4 support is complete.
 */
void opendlms_reader_auth_configurator_plain(opendlms_reader_auth_t *auth);

#ifdef __cplusplus
}
#endif

#endif /* OPENDLMS_READER_AUTH_H */
