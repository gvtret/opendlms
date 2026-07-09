/**
 * A virtual channel of communication with the logical device
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef CSM_CHANNEL_H
#define CSM_CHANNEL_H

#include "csm_association.h"
#include "csm_services.h"

#define INVALID_CHANNEL_ID 0U

typedef struct csm_channel_s {
	csm_request request;
	csm_asso_state *asso;  //!< Association used for that channel

} csm_channel;

/**
 * \brief Channel context — holds all state previously stored as globals
 *
 *  Eliminates global mutable state for thread safety.
 *  One context per server/client instance.
 */
typedef struct {
	csm_channel *channels;
	uint8_t channel_size;
	csm_asso_state *asso_states;
	const csm_asso_config *asso_configs;
	uint8_t asso_size;
	csm_db_access_handler db_handler;
} csm_channel_ctx;

void csm_channel_ctx_init(
    csm_channel_ctx *ctx, csm_channel *channels, uint8_t chan_size, csm_asso_state *assos, const csm_asso_config *assos_config, uint8_t asso_size
);

void csm_channel_ctx_set_db(csm_channel_ctx *ctx, csm_db_access_handler handler);

void csm_channel_disconnect_ctx(csm_channel_ctx *ctx, uint8_t channel);
int csm_channel_hls_pass3_ctx(csm_channel_ctx *ctx, csm_array *array, csm_request *request);
int csm_channel_hls_pass4_ctx(csm_channel_ctx *ctx, csm_array *array, csm_request *request);

/* Client-side HLS (mechanism 5, GMAC).
 *
 * csm_channel_client_hls_pass3 builds the reply_to_HLS_authentication payload
 * f(StoC) = SC || IC || GMAC(SC || AK || StoC) from the received StoC challenge
 * (asso->handshake.stoc) using the local system title. Writes up to 17 bytes to
 * out and returns the length written, or 0 on error.
 *
 * csm_channel_client_hls_verify_pass4 verifies the server reply f(CtoS) against
 * the challenge the client sent (asso->handshake.ctos) using the peer system
 * title (asso->server_app_title). Returns TRUE when the tag is valid.
 *
 * request supplies the SAP (llc.dsap) and channel_id used for key lookup. */
uint32_t csm_channel_client_hls_pass3(csm_asso_state *asso, csm_request *request, uint8_t *out, uint32_t out_size);
int csm_channel_client_hls_verify_pass4(csm_asso_state *asso, csm_request *request, const uint8_t *reply, uint32_t reply_len);
int csm_channel_execute_ctx(csm_channel_ctx *ctx, csm_db_context_t *db_ctx, uint8_t channel, csm_array *packet);
uint8_t csm_channel_new_ctx(csm_channel_ctx *ctx);

/* Backward-compatible API (uses static default context — single-instance only) */
void csm_channel_init(csm_channel *channels, uint8_t chan_size, csm_asso_state *assos, const csm_asso_config *assos_config, uint8_t asso_size);
void csm_channel_disconnect(uint8_t channel);
int csm_channel_hls_pass3(csm_array *array, csm_request *request);
int csm_channel_hls_pass4(csm_array *array, csm_request *request);
int csm_channel_execute(csm_db_context_t *ctx, uint8_t channel, csm_array *packet);
uint8_t csm_channel_new(void);

#endif  // CSM_CHANNEL_H
