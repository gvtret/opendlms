/**
 * \file csm_security_suite.h
 *
 * \brief Security suite definitions for COSEM authentication
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
    CSM_SEC_SUITE_ID_0 = 0,
    CSM_SEC_SUITE_ID_1 = 1,
    CSM_SEC_SUITE_ID_2 = 2,
    CSM_SEC_SUITE_ID_3 = 3,
    CSM_SEC_SUITE_ID_4 = 4,
    CSM_SEC_SUITE_ID_5 = 5,
} csm_sec_suite_id;

#ifdef __cplusplus
}
#endif

#endif /* CSM_SECURITY_SUITE_H */
