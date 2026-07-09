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

#include "csm_channel.h"
#include "csm_config.h"
#include "csm_services.h"
#include "csm_security.h"
#include "csm_axdr_codec.h"

#include <string.h>

/* ── Context-based API (thread-safe) ─────────────────────────────────────── */

void csm_channel_ctx_init(
    csm_channel_ctx *ctx, csm_channel *channels, uint8_t chan_size, csm_asso_state *assos, const csm_asso_config *assos_config, uint8_t asso_size
) {
	if (ctx == NULL)
		return;

	ctx->channels = channels;
	ctx->channel_size = chan_size;
	ctx->asso_states = assos;
	ctx->asso_configs = assos_config;
	ctx->asso_size = asso_size;
	ctx->db_handler = NULL;

	for (uint32_t i = 0U; (assos != NULL) && (i < asso_size); i++) {
		csm_asso_init(&assos[i]);
	}

	for (uint32_t i = 0U; (channels != NULL) && (i < chan_size); i++) {
		channels[i].asso = NULL;
		channels[i].request.channel_id = INVALID_CHANNEL_ID;
	}
}

void csm_channel_ctx_set_db(csm_channel_ctx *ctx, csm_db_access_handler handler) {
	if (ctx != NULL) {
		ctx->db_handler = handler;
	}
}

/* While an HLS association is pending, the only service the client is allowed
 * to invoke is reply_to_HLS_authentication: ACTION on the current Association
 * (class 15) method 1. Peek the request header without consuming it. */
static int csm_channel_is_reply_to_hls(csm_array *packet) {
	uint8_t tag = 0U;
	uint8_t class_hi = 0U;
	uint8_t class_lo = 0U;
	uint8_t method = 0U;

	if (!csm_array_get(packet, 0U, &tag) || (tag != AXDR_ACTION_REQUEST)) {
		return FALSE;
	}

	/* Normal ACTION.request: C3 <type> <invoke> <class:u16> <obis:6> <method> ... */
	if (!csm_array_get(packet, 3U, &class_hi) || !csm_array_get(packet, 4U, &class_lo) || !csm_array_get(packet, 11U, &method)) {
		return FALSE;
	}

	return ((class_hi == 0U) && (class_lo == 15U) && (method == 1U)) ? TRUE : FALSE;
}

/* Handle reply_to_HLS_authentication (HLS mechanism 5, GMAC) entirely within the
 * library: verify the client's f(StoC) via pass 3 and, on success, answer with
 * f(CtoS) via pass 4. The ACTION.request layout is:
 *   C3 <type> <invoke> <class:u16> <obis:6> <method> <have-data> 09 <len> <SC||IC||tag>
 * The octet-string parameter therefore starts at index 13. On success the reply
 * ACTION.response is written into packet and its length returned; 0 on any
 * failure (the caller then refuses the service and the association stays pending). */
static int csm_channel_hls_action_ctx(csm_channel_ctx *ctx, csm_request *request, csm_array *packet) {
	uint8_t invoke_id = 0U;
	uint32_t total = packet->wr_index;

	if (!csm_array_get(packet, 2U, &invoke_id) || (total <= 13U)) {
		return 0;
	}

	/* Copy the reply_to_HLS octet-string into a scratch array with offset
     * headroom for pass 3's AAD scratch, then verify the client GMAC. */
	uint8_t inbuf[256];
	memset(inbuf, 0, sizeof(inbuf));
	csm_array in;
	csm_array_init(&in, inbuf, sizeof(inbuf), 0U, 128U);

	uint32_t param_len = total - 13U;
	if (param_len > 64U) {
		param_len = 64U;
	}
	for (uint32_t k = 0U; k < param_len; k++) {
		uint8_t b = 0U;
		if (!csm_array_get(packet, 13U + k, &b) || !csm_array_write_u8(&in, b)) {
			return 0;
		}
	}

	uint32_t osize = 0U;
	if (!csm_axdr_rd_octetstring(&in, &osize)) {
		return 0;
	}
	if (csm_channel_hls_pass3_ctx(ctx, &in, request) != TRUE) {
		return 0;
	}

	/* Client authenticated (pass 3 granted the association). Build f(CtoS). */
	uint8_t outbuf[256];
	memset(outbuf, 0, sizeof(outbuf));
	csm_array out;
	csm_array_init(&out, outbuf, sizeof(outbuf), 0U, 64U);
	if (csm_channel_hls_pass4_ctx(ctx, &out, request) != TRUE) {
		return 0;
	}

	/* Assemble the ACTION.response-normal carrying the pass-4 octet-string. */
	packet->rd_index = 0U;
	packet->wr_index = 0U;
	int valid = csm_array_write_u8(packet, AXDR_ACTION_RESPONSE);
	valid = valid && csm_array_write_u8(packet, 0x01U); /* Response-Normal */
	valid = valid && csm_array_write_u8(packet, invoke_id);
	valid = valid && csm_array_write_u8(packet, 0x00U); /* action-result: success */
	valid = valid && csm_array_write_u8(packet, 0x01U); /* return-parameters present */
	valid = valid && csm_array_write_u8(packet, 0x00U); /* Get-Data-Result: Data */
	valid = valid && csm_array_write_buff(packet, &outbuf[out.offset], out.wr_index);

	return valid ? (int)packet->wr_index : 0;
}

int csm_channel_execute_ctx(csm_channel_ctx *ctx, csm_db_context_t *db_ctx, uint8_t channel, csm_array *packet) {
	int ret = FALSE;

	if ((ctx == NULL) || (ctx->channels == NULL) || (ctx->asso_states == NULL) || (ctx->asso_configs == NULL) || (packet == NULL) ||
	    (channel >= ctx->channel_size)) {
		CSM_ERR("[CHAN] Stack is not initialized. Call csm_channel_ctx_init() first.");
		return ret;
	}

	uint32_t i = 0U;

	/* Find the association used by this request */
	for (i = 0U; i < ctx->asso_size; i++) {
		if ((ctx->channels[channel].request.llc.ssap == ctx->asso_configs[i].llc.ssap) &&
		    (ctx->channels[channel].request.llc.dsap == ctx->asso_configs[i].llc.dsap)) {
			break;
		}
	}

	if (i < ctx->asso_size) {
		/* Association found, use this one */
		ctx->asso_states[i].config = &ctx->asso_configs[i];
		ctx->channels[channel].asso = &ctx->asso_states[i];

		uint8_t tag;
		if (csm_array_get(packet, 0U, &tag)) {
			switch (tag) {
				case CSM_ASSO_AARE:
				case CSM_ASSO_AARQ:
				case CSM_ASSO_RLRE:
				case CSM_ASSO_RLRQ:
					ret = csm_asso_server_execute(&ctx->asso_states[i], packet);
					break;
				default:
					if (ctx->asso_states[i].state_cf == CF_ASSOCIATED) {
						if (ctx->db_handler != NULL) {
							ret = csm_server_services_execute_handler(ctx->db_handler, db_ctx, &ctx->asso_states[i], &ctx->channels[channel].request, packet);
						} else {
							ret = csm_server_services_execute(db_ctx, &ctx->asso_states[i], &ctx->channels[channel].request, packet);
						}
					} else if (ctx->asso_states[i].state_cf == CF_ASSOCIATION_PENDING) {
						/* HLS handshake: the only service permitted before the
                     * association is granted is reply_to_HLS_authentication
                     * (ACTION on the current Association object, method 1). It is
                     * handled by the library itself (pass 3/4) so no application
                     * db handler is reached while pending. Any other service, or a
                     * reply_to_HLS that fails authentication, is refused. */
						ret = 0;
						if (csm_channel_is_reply_to_hls(packet)) {
							ret = csm_channel_hls_action_ctx(ctx, &ctx->channels[channel].request, packet);
						}

						if (ret <= 0) {
							if (ctx->db_handler != NULL) {
								ret = csm_services_hls_execute_handler(ctx->db_handler, db_ctx, &ctx->asso_states[i], &ctx->channels[channel].request, packet);
							} else {
								ret = csm_services_hls_execute(db_ctx, &ctx->asso_states[i], &ctx->channels[channel].request, packet);
							}
						}
					} else {
						CSM_ERR("[CHAN] Association is not open");
					}
					break;
			}
		}
	}
	return ret;
}

int csm_channel_hls_pass3_ctx(csm_channel_ctx *ctx, csm_array *array, csm_request *request) {
	csm_sec_control_byte sc;
	uint32_t ic;
	int ret = FALSE;
	csm_asso_state *asso = NULL;

	if ((ctx == NULL) || (array == NULL) || (request == NULL) || (ctx->channels == NULL) || (request->channel_id == INVALID_CHANNEL_ID) ||
	    (request->channel_id > ctx->channel_size)) {
		return FALSE;
	}

	asso = ctx->channels[request->channel_id - 1U].asso;
	if (asso == NULL) {
		return FALSE;
	}

	csm_array_dump(array);

	/* Save SC and IC */
	csm_array_read_u8(array, &sc.sh_byte);
	csm_array_read_u32(array, &ic);

	/* Remaining data should be the TAG */
	uint32_t unread = csm_array_unread(array);

	if (unread == 12U) {
		uint32_t offset = array->offset;

		/* csm_sec_auth_decrypt overwrites the 17 bytes preceding the information
         * with the SC || AK AAD and requires its read cursor to land at >= 17
         * after consuming the 5-byte SC/IC header. Reserve 12 bytes of scratch
         * headroom before SC so the read cursor starts at 12. */
		if (offset >= (CSM_DEF_MAX_HLS_SIZE + 12U)) {
			/* Rate limiting: reject after repeated failures. */
			if (asso->hls_failures >= 5U) {
				CSM_ERR("[CHAN] HLS: too many auth failures, aborting");
				return FALSE;
			}

			/* Replay protection: IC must be strictly increasing after first auth. */
			if (asso->ic_valid && ic <= asso->last_client_ic) {
				CSM_ERR("[CHAN] HLS: replayed IC %u <= %u", (unsigned)ic, (unsigned)asso->last_client_ic);
				asso->hls_failures++;
				return FALSE;
			}

			/* Reserve memory & prepare packet. Shifting the working offset by an
             * extra 12 bytes keeps the incoming tag in its original position. */
			array->offset = (offset + array->rd_index) - (CSM_DEF_SEC_HDR_SIZE + asso->handshake.stoc.size + 12U);
			array->rd_index = 0U;
			array->wr_index = 0U;

			/* Build SC || IC || Information || Tag, preceded by AAD scratch */
			csm_array_writer_jump(array, 12U);
			csm_array_write_u8(array, sc.sh_byte);
			csm_array_write_u32(array, ic);
			csm_array_write_buff(array, &asso->handshake.stoc.value[0], asso->handshake.stoc.size);
			csm_array_writer_jump(array, 12U);

			array->rd_index = 12U;

			csm_sec_result res = csm_sec_auth_decrypt(array, request, &asso->client_app_title[0]);

			array->offset = offset;

			if (res == CSM_SEC_OK) {
				CSM_LOG("[CHAN] HLS Pass 3 success!");
				/* Client authenticated: grant the pending HLS association. */
				asso->state_cf = CF_ASSOCIATED;
				asso->last_client_ic = ic;
				asso->ic_valid = 1U;
				asso->hls_failures = 0U;
				ret = TRUE;
			} else {
				CSM_ERR("[CHAN] Bad tag");
				asso->hls_failures++;
			}
		} else {
			CSM_ERR("[CHAN] Array too small for HLS");
		}
	} else {
		CSM_ERR("[CHAN] Bad HLS Pass3 size");
	}

	return ret;
}

int csm_channel_hls_pass4_ctx(csm_channel_ctx *ctx, csm_array *array, csm_request *request) {
	int ret = FALSE;
	csm_asso_state *asso = NULL;

	if ((ctx == NULL) || (array == NULL) || (request == NULL) || (ctx->channels == NULL) || (request->channel_id == INVALID_CHANNEL_ID) ||
	    (request->channel_id > ctx->channel_size)) {
		return FALSE;
	}

	csm_sec_control_byte sc;
	sc.sh_byte = 0U;
	sc.sh_bit_field.authentication = 1U;

	uint32_t offset = array->offset;

	asso = ctx->channels[request->channel_id - 1U].asso;
	if (asso == NULL) {
		return FALSE;
	}

	(void)offset;
	uint8_t ctos_size = asso->handshake.ctos.size;

	/* Need room after the buffer offset for [17 AAD scratch][CtoS][12 tag]. */
	if ((ctos_size > 0U) && (ctos_size <= CSM_DEF_CHALLENGE_SIZE) && ((array->size - array->offset) >= (17U + (uint32_t)ctos_size + 12U))) {
		/* Use per-association invocation counter */
		uint32_t ic = asso->invocation_counter;
		asso->invocation_counter++;

		/* GMAC over CtoS: layout [17 AAD scratch][CtoS][12 tag], info cursor 17. */
		array->rd_index = 0U;
		array->wr_index = 0U;
		int valid = csm_array_writer_jump(array, 17U);
		valid = valid && csm_array_write_buff(array, &asso->handshake.ctos.value[0], ctos_size);
		array->rd_index = 17U;

		csm_sec_result res = CSM_SEC_ERROR;
		if (valid) {
			res = csm_sec_auth_encrypt(array, request, csm_sys_get_system_title(), sc, ic);
		}

		uint8_t tag[12];
		for (uint32_t i = 0U; valid && (i < 12U); i++) {
			valid = csm_array_get(array, 17U + (uint32_t)ctos_size + i, &tag[i]);
		}

		/* Emit the reply_to_HLS response octet-string SC || IC || tag. */
		array->rd_index = 0U;
		array->wr_index = 0U;
		valid = valid && csm_array_write_u8(array, AXDR_TAG_OCTETSTRING);
		valid = valid && csm_ber_write_len(array, 17U);
		valid = valid && csm_array_write_u8(array, sc.sh_byte);
		valid = valid && csm_array_write_u32(array, ic);
		valid = valid && csm_array_write_buff(array, tag, 12U);

		if ((res == CSM_SEC_OK) && valid) {
			CSM_LOG("[CHAN] HLS Pass 4 success!");
			ret = TRUE;
		} else {
			CSM_ERR("[CHAN] HLS Pass 4 failure");
		}
	} else {
		CSM_ERR("[CHAN] Array too small for HLS pass 4");
	}

	return ret;
}

uint32_t csm_channel_client_hls_pass3(csm_asso_state *asso, csm_request *request, uint8_t *out, uint32_t out_size) {
	if ((asso == NULL) || (request == NULL) || (out == NULL) || (out_size < 17U)) {
		return 0U;
	}

	uint8_t chsize = asso->handshake.stoc.size;
	if ((chsize == 0U) || (chsize > CSM_DEF_CHALLENGE_SIZE)) {
		return 0U;
	}

	csm_sec_control_byte sc;
	sc.sh_byte = 0U;
	sc.sh_bit_field.authentication = 1U;

	uint32_t ic = asso->invocation_counter;
	asso->invocation_counter++;

	/* Layout [17 AAD scratch][StoC][12 tag] with the info cursor at 17. */
	uint8_t buf[17U + CSM_DEF_CHALLENGE_SIZE + 12U];
	memset(buf, 0, sizeof(buf));
	memcpy(&buf[17], &asso->handshake.stoc.value[0], chsize);

	csm_array a;
	csm_array_init(&a, buf, sizeof(buf), 17U + (uint32_t)chsize, 0U);
	a.rd_index = 17U;

	if (csm_sec_auth_encrypt(&a, request, csm_sys_get_system_title(), sc, ic) != CSM_SEC_OK) {
		return 0U;
	}

	out[0] = sc.sh_byte;
	out[1] = (uint8_t)((ic >> 24) & 0xFFU);
	out[2] = (uint8_t)((ic >> 16) & 0xFFU);
	out[3] = (uint8_t)((ic >> 8) & 0xFFU);
	out[4] = (uint8_t)(ic & 0xFFU);
	memcpy(&out[5], &buf[17U + (uint32_t)chsize], 12U);
	return 17U;
}

int csm_channel_client_hls_verify_pass4(csm_asso_state *asso, csm_request *request, const uint8_t *reply, uint32_t reply_len) {
	if ((asso == NULL) || (request == NULL) || (reply == NULL) || (reply_len < 17U)) {
		return FALSE;
	}

	uint8_t chsize = asso->handshake.ctos.size;
	if ((chsize == 0U) || (chsize > CSM_DEF_CHALLENGE_SIZE)) {
		return FALSE;
	}

	/* Reconstruct [12 scratch][SC][IC][CtoS][tag] with the header cursor at 12. */
	uint8_t buf[12U + 1U + 4U + CSM_DEF_CHALLENGE_SIZE + 12U];
	memset(buf, 0, sizeof(buf));
	buf[12] = reply[0];              /* SC */
	memcpy(&buf[13], &reply[1], 4U); /* IC */
	memcpy(&buf[17], &asso->handshake.ctos.value[0], chsize);
	memcpy(&buf[17U + (uint32_t)chsize], &reply[5], 12U); /* tag */

	csm_array d;
	csm_array_init(&d, buf, sizeof(buf), 17U + (uint32_t)chsize + 12U, 0U);
	d.rd_index = 12U;

	return (csm_sec_auth_decrypt(&d, request, &asso->server_app_title[0]) == CSM_SEC_OK) ? TRUE : FALSE;
}

void csm_channel_disconnect_ctx(csm_channel_ctx *ctx, uint8_t channel) {
	if (ctx == NULL)
		return;

	uint8_t index = channel;
	if ((channel > 0U) && (channel <= ctx->channel_size) && (ctx->channels[channel - 1U].request.channel_id == channel)) {
		index = (uint8_t)(channel - 1U);
	}

	if (index < ctx->channel_size) {
		ctx->channels[index].request.channel_id = INVALID_CHANNEL_ID;
		if (ctx->channels[index].asso != NULL) {
			ctx->channels[index].asso->state_cf = CF_IDLE;
			ctx->channels[index].asso->last_client_ic = 0U;
			ctx->channels[index].asso->ic_valid = 0U;
			ctx->channels[index].asso->hls_failures = 0U;
		}
	}
}

uint8_t csm_channel_new_ctx(csm_channel_ctx *ctx) {
	uint8_t chan_id = INVALID_CHANNEL_ID;

	if (ctx != NULL) {
		for (uint32_t i = 0U; i < ctx->channel_size; i++) {
			if (ctx->channels[i].request.channel_id == INVALID_CHANNEL_ID) {
				chan_id = i + 1U;
				ctx->channels[i].request.channel_id = chan_id;
				CSM_LOG("[CHAN] Grant connection to channel %d", chan_id);
				break;
			}
		}
	}

	return chan_id;
}

/* ── Legacy API (backward compatibility, single-instance only) ────────────── */

static csm_channel_ctx *g_default_ctx = NULL;

void csm_channel_init(csm_channel *channels, uint8_t chan_size, csm_asso_state *assos, const csm_asso_config *assos_config, uint8_t asso_size) {
	/* Allocate context statically for legacy API */
	static csm_channel_ctx legacy_ctx;
	g_default_ctx = &legacy_ctx;
	csm_channel_ctx_init(g_default_ctx, channels, chan_size, assos, assos_config, asso_size);
}

void csm_channel_disconnect(uint8_t channel) {
	if (g_default_ctx != NULL) {
		csm_channel_disconnect_ctx(g_default_ctx, channel);
	}
}

int csm_channel_hls_pass3(csm_array *array, csm_request *request) {
	if (g_default_ctx != NULL) {
		return csm_channel_hls_pass3_ctx(g_default_ctx, array, request);
	}
	return FALSE;
}

int csm_channel_hls_pass4(csm_array *array, csm_request *request) {
	if (g_default_ctx != NULL) {
		return csm_channel_hls_pass4_ctx(g_default_ctx, array, request);
	}
	return FALSE;
}

int csm_channel_execute(csm_db_context_t *ctx, uint8_t channel, csm_array *packet) {
	if (g_default_ctx != NULL) {
		return csm_channel_execute_ctx(g_default_ctx, ctx, channel, packet);
	}
	return FALSE;
}

uint8_t csm_channel_new(void) {
	if (g_default_ctx != NULL) {
		return csm_channel_new_ctx(g_default_ctx);
	}
	return INVALID_CHANNEL_ID;
}
