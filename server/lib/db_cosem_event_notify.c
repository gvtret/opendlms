/**
 * Event notification transport for server
 *
 * Manages a queue of pending unsolicited event notifications and
 * dispatches them via a transport callback.
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#include "db_cosem_event_notify.h"
#include "csm_config.h"
#include <string.h>

#define EVENT_NOTIFY_QUEUE_MAX 8U
#define EVENT_NOTIFY_DATA_MAX  128U

typedef struct {
	uint8_t data[EVENT_NOTIFY_DATA_MAX];
	uint32_t len;
	uint8_t pending;
} db_cosem_event_notify_entry;

static db_cosem_event_notify_entry event_queue[EVENT_NOTIFY_QUEUE_MAX];
static uint8_t event_queue_count = 0U;

static db_cosem_event_notify_transport_fn event_transport = NULL;
static void *event_transport_ctx = NULL;

void db_cosem_event_notify_set_transport_cb(db_cosem_event_notify_transport_fn transport, void *ctx) {
	event_transport = transport;
	event_transport_ctx = ctx;
	CSM_LOG("[EVENT] Transport callback set");
}

void db_cosem_event_notify_service(uint32_t ms) {
	(void)ms;

	for (uint8_t i = 0U; i < event_queue_count; i++) {
		if (event_queue[i].pending && (event_transport != NULL)) {
			event_transport(event_queue[i].data, event_queue[i].len, event_transport_ctx);
			event_queue[i].pending = 0U;
			CSM_LOG("[EVENT] Dispatched entry %u, len=%u", i, event_queue[i].len);
		}
	}

	event_queue_count = 0U;
}
