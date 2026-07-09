/**
 * \file test_hls.cpp
 * \brief HLS / ciphered authentication round-trip coverage.
 *
 * These tests exercise the *success* path of the symmetric auth layer, which
 * previously had only negative coverage. They pin down the exact csm_array
 * layout the security helpers require (17-byte AAD headroom before the SC/IC
 * header) so the HLS pass 3/4 wiring can rely on it.
 */

#include "catch.hpp"

#include "csm_security.h"
#include "csm_definitions.h"
#include "csm_array.h"
#include "csm_association.h"
#include "csm_axdr_codec.h"
extern "C" {
#include "csm_channel.h"
#include "csm_server.h"
#include "csm_transport.h"
}

#include <cstring>
#include <cstdlib>

extern "C" void csm_sys_init();

/* Auth-only GMAC over an 8-octet challenge, mirroring HLS mechanism 5:
 * f(challenge) = SC || IC || GMAC(SC || AK || challenge).
 * Encrypt produces the tag; decrypt over the same inputs must verify it. */
TEST_CASE("Security auth GMAC round-trip verifies", "[crypto][security][hls]") {
	csm_sys_init();

	const uint8_t challenge[8] = {0x50U, 0x36U, 0x77U, 0x52U, 0x4AU, 0x32U, 0x31U, 0x46U};  // "P6wRJ21F"
	const uint8_t chsize = 8U;
	const uint32_t ic = 0x01234567U;

	csm_request req;
	memset(&req, 0, sizeof(req));
	req.channel_id = 0U;
	req.llc.dsap = 1U;

	csm_sec_control_byte sc;
	sc.sh_byte = 0U;
	sc.sh_bit_field.authentication = 1U;

	/* --- Encrypt: layout [17 AAD headroom][challenge][12 tag] ---
     * The helper requires rd_index (the information cursor) >= 17 with 17 bytes
     * of writable AAD headroom before it. csm_array_init's 5th arg is the buffer
     * offset, not rd_index, so position the read cursor explicitly. */
	uint8_t ebuf[17U + 8U + 12U];
	memset(ebuf, 0, sizeof(ebuf));
	memcpy(&ebuf[17], challenge, chsize);

	csm_array earr;
	csm_array_init(&earr, ebuf, sizeof(ebuf), 17U + chsize, 0U);
	earr.rd_index = 17U; /* information cursor -> challenge */

	REQUIRE(csm_sec_auth_encrypt(&earr, &req, csm_sys_get_system_title(), sc, ic) == CSM_SEC_OK);

	uint8_t tag[12];
	memcpy(tag, &ebuf[17U + chsize], 12U); /* tag written right after challenge */

	/* --- Decrypt: layout [12 headroom][SC][IC][challenge][tag], rd=12 --- */
	uint8_t dbuf[12U + 1U + 4U + 8U + 12U];
	memset(dbuf, 0, sizeof(dbuf));
	dbuf[12] = sc.sh_byte;
	dbuf[13] = (uint8_t)((ic >> 24) & 0xFFU);
	dbuf[14] = (uint8_t)((ic >> 16) & 0xFFU);
	dbuf[15] = (uint8_t)((ic >> 8) & 0xFFU);
	dbuf[16] = (uint8_t)(ic & 0xFFU);
	memcpy(&dbuf[17], challenge, chsize);
	memcpy(&dbuf[17U + chsize], tag, 12U);

	csm_array darr;
	csm_array_init(&darr, dbuf, sizeof(dbuf), 17U + chsize + 12U, 0U);
	darr.rd_index = 12U; /* SC/IC cursor; 12 bytes headroom before it */

	REQUIRE(csm_sec_auth_decrypt(&darr, &req, csm_sys_get_system_title()) == CSM_SEC_OK);

	/* A tampered tag must be rejected. */
	dbuf[17U + chsize] ^= 0xFFU;
	csm_array darr2;
	csm_array_init(&darr2, dbuf, sizeof(dbuf), 17U + chsize + 12U, 0U);
	darr2.rd_index = 12U;
	REQUIRE(csm_sec_auth_decrypt(&darr2, &req, csm_sys_get_system_title()) == CSM_SEC_AUTH_FAILURE);
}

/* End-to-end: client builds f(StoC); server pass 3 must verify it (loopback).
 * This is the first positive coverage of the HLS pass 3 success path. */
TEST_CASE("HLS pass 3 verifies a client GMAC reply", "[hls][channel]") {
	csm_sys_init();

	csm_channel channels[1];
	csm_asso_state assos[1];
	csm_asso_config configs[1];
	memset(configs, 0, sizeof(configs));
	configs[0].llc.ssap = 0U;
	configs[0].llc.dsap = 1U;

	csm_channel_ctx ctx;
	csm_channel_ctx_init(&ctx, channels, 1U, assos, configs, 1U);

	csm_asso_state *asso = &assos[0];
	channels[0].asso = asso;

	const uint8_t stoc[8] = {0x50U, 0x36U, 0x77U, 0x52U, 0x4AU, 0x32U, 0x31U, 0x46U};
	asso->handshake.stoc.size = 8U;
	memcpy(asso->handshake.stoc.value, stoc, 8U);
	/* Client and server share the test HAL system title. */
	memcpy(asso->client_app_title, csm_sys_get_system_title(), CSM_DEF_APP_TITLE_SIZE);

	csm_request req;
	memset(&req, 0, sizeof(req));
	req.channel_id = 1U;
	req.llc.dsap = 1U;

	/* Client builds the reply_to_HLS payload via the library function. */
	uint8_t f_stoc[17];
	REQUIRE(csm_channel_client_hls_pass3(asso, &req, f_stoc, sizeof(f_stoc)) == 17U);

	/* reply_to_HLS payload SC || IC || tag in a buffer with offset headroom. */
	uint8_t big[256];
	memset(big, 0, sizeof(big));
	csm_array pkt;
	csm_array_init(&pkt, big, sizeof(big), 0U, 128U);
	REQUIRE(csm_array_write_buff(&pkt, f_stoc, 17U) == TRUE);

	REQUIRE(csm_channel_hls_pass3_ctx(&ctx, &pkt, &req) == TRUE);
}

/* Server pass 4 generates f(CtoS); the client must verify it. First positive
 * coverage of the HLS pass 4 success path. */
TEST_CASE("HLS pass 4 reply verifies on the client", "[hls][channel]") {
	csm_sys_init();

	csm_channel channels[1];
	csm_asso_state assos[1];
	csm_asso_config configs[1];
	memset(configs, 0, sizeof(configs));
	configs[0].llc.dsap = 1U;

	csm_channel_ctx ctx;
	csm_channel_ctx_init(&ctx, channels, 1U, assos, configs, 1U);
	csm_asso_state *asso = &assos[0];
	channels[0].asso = asso;

	const uint8_t ctos[8] = {0x41U, 0x42U, 0x43U, 0x44U, 0x45U, 0x46U, 0x47U, 0x48U};
	asso->handshake.ctos.size = 8U;
	memcpy(asso->handshake.ctos.value, ctos, 8U);
	asso->invocation_counter = 5U;
	/* Client verifies with the peer (server) system title. */
	memcpy(asso->server_app_title, csm_sys_get_system_title(), CSM_DEF_APP_TITLE_SIZE);

	csm_request req;
	memset(&req, 0, sizeof(req));
	req.channel_id = 1U;
	req.llc.dsap = 1U;

	uint8_t out[256];
	memset(out, 0, sizeof(out));
	csm_array pkt;
	csm_array_init(&pkt, out, sizeof(out), 0U, 64U);

	REQUIRE(csm_channel_hls_pass4_ctx(&ctx, &pkt, &req) == TRUE);

	/* Output octet-string: 0x09 0x11 SC IC(4) tag(12) at buffer offset 64. */
	REQUIRE(out[64] == 0x09U);
	REQUIRE(out[65] == 0x11U);

	uint8_t f_ctos[17];
	memcpy(f_ctos, &out[66], 17U);
	REQUIRE(csm_channel_client_hls_verify_pass4(asso, &req, f_ctos, 17U) == TRUE);
}

/* ---- Full client HLS handshake over an in-process loopback transport ----
 * The loopback runs the real server: csm_asso_server_execute for AARQ/AARE and
 * csm_channel_hls_pass3/4 for the reply_to_HLS ACTION. This proves the client
 * csm_client_connect HLS wiring end to end against real server crypto. */

struct hls_loopback {
	csm_channel channels[1];
	csm_asso_state assos[1];
	csm_asso_config configs[1];
	csm_channel_ctx ctx;
	uint8_t resp[256];
	uint32_t resp_len;
};

static int lb_open(void *ctx, uint8_t ch) {
	(void)ctx;
	(void)ch;
	return CSM_TRANSPORT_OK;
}

static int lb_is_connected(void *ctx, uint8_t ch) {
	(void)ctx;
	(void)ch;
	return 1;
}

static void lb_close(void *ctx, uint8_t ch) {
	(void)ctx;
	(void)ch;
}

static int lb_send(void *vctx, uint8_t ch, const uint8_t *data, uint32_t len) {
	(void)ch;
	hls_loopback *lb = static_cast<hls_loopback *>(vctx);
	lb->resp_len = 0U;

	if ((len == 0U) || (data == nullptr)) {
		return -1;
	}

	if (data[0] == CSM_ASSO_AARQ) {
		uint8_t buf[512];
		memset(buf, 0, sizeof(buf));
		memcpy(buf, data, len);
		csm_array pkt;
		csm_array_init(&pkt, buf, sizeof(buf), len, 0U);
		int n = csm_asso_server_execute(&lb->assos[0], &pkt);
		if (n <= 0) {
			return -1;
		}
		memcpy(lb->resp, buf, (uint32_t)n);
		lb->resp_len = (uint32_t)n;
	} else if (data[0] == AXDR_ACTION_REQUEST) {
		csm_request req;
		memset(&req, 0, sizeof(req));
		req.channel_id = 1U;
		req.llc.dsap = 1U;

		/* ACTION request: C3 01 iid class(2) obis(6) method present 09 11 <17>. */
		uint8_t sbuf[256];
		memset(sbuf, 0, sizeof(sbuf));
		csm_array in;
		csm_array_init(&in, sbuf, sizeof(sbuf), 0U, 128U);
		if (csm_array_write_buff(&in, &data[13], 19U) != TRUE) {
			return -1;
		}
		uint32_t osize = 0U;
		if (csm_axdr_rd_octetstring(&in, &osize) != TRUE) {
			return -1;
		}
		if (csm_channel_hls_pass3_ctx(&lb->ctx, &in, &req) != TRUE) {
			return -1;
		}

		uint8_t obuf[256];
		memset(obuf, 0, sizeof(obuf));
		csm_array out;
		csm_array_init(&out, obuf, sizeof(obuf), 0U, 64U);
		if (csm_channel_hls_pass4_ctx(&lb->ctx, &out, &req) != TRUE) {
			return -1;
		}

		/* Envelope: C7 01 iid 00 01 00 <octet-string 09 11 SC IC tag>. */
		lb->resp[0] = (uint8_t)AXDR_ACTION_RESPONSE;
		lb->resp[1] = 0x01U;
		lb->resp[2] = data[2];
		lb->resp[3] = 0x00U;
		lb->resp[4] = 0x01U;
		lb->resp[5] = 0x00U;
		memcpy(&lb->resp[6], &obuf[64], 19U);
		lb->resp_len = 25U;
	} else {
		return -1;
	}
	return (int)len;
}

static int lb_recv(void *vctx, uint8_t ch, uint8_t *buf, uint32_t sz, uint32_t ms) {
	(void)ch;
	(void)ms;
	hls_loopback *lb = static_cast<hls_loopback *>(vctx);
	if ((lb->resp_len == 0U) || (lb->resp_len > sz)) {
		return -1;
	}
	memcpy(buf, lb->resp, lb->resp_len);
	return (int)lb->resp_len;
}

static const csm_transport_ops lb_ops = {lb_open, lb_send, lb_recv, lb_close, lb_is_connected, nullptr};

TEST_CASE("Client HLS GMAC handshake completes over loopback", "[hls][client]") {
	csm_sys_init();

	hls_loopback lb;
	memset(&lb, 0, sizeof(lb));
	lb.configs[0].llc.ssap = 0x01;
	lb.configs[0].llc.dsap = 0x00;
	lb.configs[0].conformance = 0xFFFFFFFFU;
	csm_channel_ctx_init(&lb.ctx, lb.channels, 1U, lb.assos, lb.configs, 1U);
	lb.channels[0].asso = &lb.assos[0];
	/* csm_channel_execute normally links the config; the loopback drives the
     * server association directly, so wire it up here. */
	lb.assos[0].config = &lb.configs[0];

	csm_transport transport;
	transport.ops = &lb_ops;
	transport.ctx = &lb;

	csm_client *client = csm_client_create(&transport, 0U, CSM_FRAMING_NONE);
	REQUIRE(client != nullptr);

	csm_asso_config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.authentication = (uint8_t)CSM_AUTH_HIGH_LEVEL_GMAC;
	cfg.application_context = (uint8_t)LN_REF;
	cfg.llc.ssap = 0x00;
	cfg.llc.dsap = 0x01;
	REQUIRE(csm_client_set_association(client, &cfg) == 0);

	/* connect returns OK only if AARQ/AARE and HLS pass 3/4 all succeeded. */
	REQUIRE(csm_client_connect(client, 1000U) == CSM_TRANSPORT_OK);

	csm_client_destroy(client);
	free(client);
}
