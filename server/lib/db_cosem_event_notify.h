/**
 * Event notification transport for server
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef DB_COSEM_EVENT_NOTIFY_H
#define DB_COSEM_EVENT_NOTIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*db_cosem_event_notify_transport_fn)(const uint8_t *data, uint32_t len, void *ctx);

void db_cosem_event_notify_set_transport_cb(db_cosem_event_notify_transport_fn transport, void *ctx);
void db_cosem_event_notify_service(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* DB_COSEM_EVENT_NOTIFY_H */
