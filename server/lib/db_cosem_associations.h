/**
 * Association management DB handler for server
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef DB_COSEM_ASSOCIATIONS_H
#define DB_COSEM_ASSOCIATIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "csm_services.h"

int db_cosem_associations_init(void);

csm_db_code db_cosem_associations_func(csm_db_context_t *ctx, csm_array *in,
                                        csm_array *out, csm_request *request);

#ifdef __cplusplus
}
#endif

#endif /* DB_COSEM_ASSOCIATIONS_H */
