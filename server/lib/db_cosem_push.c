/**
 * Push transport management for server
 *
 * Manages a queue of pending push events, binds data changes to
 * push triggers, and dispatches via a transport callback.
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

#include "db_cosem_push.h"
#include "csm_config.h"
#include <string.h>

#define PUSH_QUEUE_MAX 8U
#define PUSH_BIND_MAX  8U

typedef struct {
	db_ic_inst_t *inst;
	uint32_t long_id;
	uint8_t pending;
} db_cosem_push_queue_entry;

typedef struct {
	csm_obis_code data_obis;
	csm_obis_code push_obis;
	uint8_t active;
} db_cosem_push_bind_entry;

static db_cosem_push_queue_entry push_queue[PUSH_QUEUE_MAX];
static uint8_t push_queue_count = 0U;

static db_cosem_push_bind_entry push_binds[PUSH_BIND_MAX];
static uint8_t push_bind_count = 0U;

static db_cosem_push_transport_fn push_transport = NULL;
static void *push_transport_ctx = NULL;

static uint32_t push_poll_timer = 0U;

void db_cosem_push_set_transport_cb(db_cosem_push_transport_fn transport, void *ctx) {
	push_transport = transport;
	push_transport_ctx = ctx;
	CSM_LOG("[PUSH] Transport callback set");
}

void db_cosem_push_trigger(db_ic_inst_t *inst) {
	if (inst == NULL) {
		return;
	}

	if (push_queue_count < PUSH_QUEUE_MAX) {
		push_queue[push_queue_count].inst = inst;
		push_queue[push_queue_count].long_id = 0U;
		push_queue[push_queue_count].pending = 1U;
		push_queue_count++;
		CSM_LOG("[PUSH] Trigger queued, queue depth=%u", push_queue_count);
	} else {
		CSM_ERR("[PUSH] Queue full, dropping trigger");
	}
}

void db_cosem_push_service(uint32_t ms) {
	push_poll_timer += ms;

	for (uint8_t i = 0U; i < push_queue_count; i++) {
		if (push_queue[i].pending && (push_transport != NULL)) {
			push_transport(NULL, 0U, push_transport_ctx);
			push_queue[i].pending = 0U;
			CSM_LOG("[PUSH] Dispatched entry %u", i);
		}
	}

	push_queue_count = 0U;
	push_poll_timer = 0U;
}

void db_cosem_push_bind_trigger(const csm_obis_code *data_obis, const csm_obis_code *push_obis) {
	if ((data_obis == NULL) || (push_obis == NULL)) {
		return;
	}

	if (push_bind_count >= PUSH_BIND_MAX) {
		CSM_ERR("[PUSH] Bind table full");
		return;
	}

	push_binds[push_bind_count].data_obis = *data_obis;
	push_binds[push_bind_count].push_obis = *push_obis;
	push_binds[push_bind_count].active = 1U;
	push_bind_count++;

	CSM_LOG(
	    "[PUSH] Bound data OBIS %02X%02X%02X%02X%02X%02X to push OBIS %02X%02X%02X%02X%02X%02X", data_obis->A, data_obis->B, data_obis->C, data_obis->D,
	    data_obis->E, data_obis->F, push_obis->A, push_obis->B, push_obis->C, push_obis->D, push_obis->E, push_obis->F
	);
}

void db_cosem_push_on_confirm(db_ic_inst_t *inst, uint32_t long_id) {
	if (inst == NULL) {
		return;
	}

	CSM_LOG("[PUSH] Confirm received for long_id=%u", long_id);

	for (uint8_t i = 0U; i < push_queue_count; i++) {
		if (push_queue[i].inst == inst && push_queue[i].long_id == long_id) {
			push_queue[i].pending = 0U;
			break;
		}
	}
}
