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
extern "C" {
#include "csm_channel.h"
}

#include <cstring>

extern "C" void csm_sys_init();

/* Build the HLS reply_to_HLS payload f(challenge) = SC || IC || GMAC into out[17].
 * This is exactly what the client side must send in pass 3. */
static void build_f_challenge(const uint8_t *challenge, uint8_t chsize,
                              uint32_t ic, uint8_t sap, uint8_t channel,
                              uint8_t out[17])
{
    csm_sec_control_byte sc;
    sc.sh_byte = 0U;
    sc.sh_bit_field.authentication = 1U;

    uint8_t buf[17U + CSM_DEF_CHALLENGE_SIZE + 12U];
    memset(buf, 0, sizeof(buf));
    memcpy(&buf[17], challenge, chsize);

    csm_array a;
    csm_array_init(&a, buf, sizeof(buf), 17U + chsize, 0U);
    a.rd_index = 17U;

    csm_request req;
    memset(&req, 0, sizeof(req));
    req.channel_id = channel;
    req.llc.dsap = sap;

    REQUIRE(csm_sec_auth_encrypt(&a, &req, csm_sys_get_system_title(), sc, ic)
            == CSM_SEC_OK);

    out[0] = sc.sh_byte;
    out[1] = (uint8_t)((ic >> 24) & 0xFFU);
    out[2] = (uint8_t)((ic >> 16) & 0xFFU);
    out[3] = (uint8_t)((ic >> 8) & 0xFFU);
    out[4] = (uint8_t)(ic & 0xFFU);
    memcpy(&out[5], &buf[17U + chsize], 12U);
}

/* Client-side verification of a server f(challenge) = SC || IC || tag reply. */
static bool verify_f_challenge(const uint8_t *challenge, uint8_t chsize,
                               const uint8_t *f, uint8_t sap, uint8_t channel)
{
    uint8_t dbuf[12U + 1U + 4U + CSM_DEF_CHALLENGE_SIZE + 12U];
    memset(dbuf, 0, sizeof(dbuf));
    dbuf[12] = f[0];                 /* SC */
    memcpy(&dbuf[13], &f[1], 4U);    /* IC */
    memcpy(&dbuf[17], challenge, chsize);
    memcpy(&dbuf[17U + chsize], &f[5], 12U); /* tag */

    csm_array d;
    csm_array_init(&d, dbuf, sizeof(dbuf), 17U + chsize + 12U, 0U);
    d.rd_index = 12U;

    csm_request req;
    memset(&req, 0, sizeof(req));
    req.channel_id = channel;
    req.llc.dsap = sap;

    return csm_sec_auth_decrypt(&d, &req, csm_sys_get_system_title()) == CSM_SEC_OK;
}

/* Auth-only GMAC over an 8-octet challenge, mirroring HLS mechanism 5:
 * f(challenge) = SC || IC || GMAC(SC || AK || challenge).
 * Encrypt produces the tag; decrypt over the same inputs must verify it. */
TEST_CASE("Security auth GMAC round-trip verifies", "[crypto][security][hls]")
{
    csm_sys_init();

    const uint8_t challenge[8] = { 0x50U, 0x36U, 0x77U, 0x52U,
                                   0x4AU, 0x32U, 0x31U, 0x46U }; // "P6wRJ21F"
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

    REQUIRE(csm_sec_auth_encrypt(&earr, &req, csm_sys_get_system_title(), sc, ic)
            == CSM_SEC_OK);

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

    REQUIRE(csm_sec_auth_decrypt(&darr, &req, csm_sys_get_system_title())
            == CSM_SEC_OK);

    /* A tampered tag must be rejected. */
    dbuf[17U + chsize] ^= 0xFFU;
    csm_array darr2;
    csm_array_init(&darr2, dbuf, sizeof(dbuf), 17U + chsize + 12U, 0U);
    darr2.rd_index = 12U;
    REQUIRE(csm_sec_auth_decrypt(&darr2, &req, csm_sys_get_system_title())
            == CSM_SEC_AUTH_FAILURE);
}

/* End-to-end: client builds f(StoC); server pass 3 must verify it (loopback).
 * This is the first positive coverage of the HLS pass 3 success path. */
TEST_CASE("HLS pass 3 verifies a client GMAC reply", "[hls][channel]")
{
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

    const uint8_t stoc[8] = { 0x50U, 0x36U, 0x77U, 0x52U,
                              0x4AU, 0x32U, 0x31U, 0x46U };
    asso->handshake.stoc.size = 8U;
    memcpy(asso->handshake.stoc.value, stoc, 8U);
    /* Client and server share the test HAL system title. */
    memcpy(asso->client_app_title, csm_sys_get_system_title(), CSM_DEF_APP_TITLE_SIZE);

    const uint32_t ic = 0x00000001U;
    uint8_t f_stoc[17];
    build_f_challenge(stoc, 8U, ic, 1U, 1U, f_stoc);

    /* reply_to_HLS payload SC || IC || tag in a buffer with offset headroom. */
    uint8_t big[256];
    memset(big, 0, sizeof(big));
    csm_array pkt;
    csm_array_init(&pkt, big, sizeof(big), 0U, 128U);
    REQUIRE(csm_array_write_buff(&pkt, f_stoc, 17U) == TRUE);

    csm_request req;
    memset(&req, 0, sizeof(req));
    req.channel_id = 1U;
    req.llc.dsap = 1U;

    REQUIRE(csm_channel_hls_pass3_ctx(&ctx, &pkt, &req) == TRUE);
}

/* Server pass 4 generates f(CtoS); the client must verify it. First positive
 * coverage of the HLS pass 4 success path. */
TEST_CASE("HLS pass 4 reply verifies on the client", "[hls][channel]")
{
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

    const uint8_t ctos[8] = { 0x41U, 0x42U, 0x43U, 0x44U,
                              0x45U, 0x46U, 0x47U, 0x48U };
    asso->handshake.ctos.size = 8U;
    memcpy(asso->handshake.ctos.value, ctos, 8U);
    asso->invocation_counter = 5U;

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
    REQUIRE(verify_f_challenge(ctos, 8U, f_ctos, 1U, 1U) == true);
}
