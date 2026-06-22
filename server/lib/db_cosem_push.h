/**
 * Push transport management for server
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef DB_COSEM_PUSH_H
#define DB_COSEM_PUSH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "db_cosem_ic.h"
#include "csm_definitions.h"

typedef void (*db_cosem_push_transport_fn)(const uint8_t *data, uint32_t len, void *ctx);

void db_cosem_push_set_transport_cb(db_cosem_push_transport_fn transport, void *ctx);
void db_cosem_push_trigger(db_ic_inst_t *inst);
void db_cosem_push_service(uint32_t ms);
void db_cosem_push_bind_trigger(const csm_obis_code *data_obis, const csm_obis_code *push_obis);
void db_cosem_push_on_confirm(db_ic_inst_t *inst, uint32_t long_id);

#ifdef __cplusplus
}
#endif

#endif /* DB_COSEM_PUSH_H */
