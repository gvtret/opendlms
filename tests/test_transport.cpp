/**
 * \file test_transport.cpp
 * \brief Tests for transport abstraction and framing layer
 *
 * Copyright (c) 2024, OpenDLMS contributors
 * SPDX-License-Identifier: MIT
 */

extern "C" {
#include "csm_transport.h"
#include "csm_framing.h"
#include "csm_array.h"
#include "csm_server.h"
}

#include "catch.hpp"
#include <cstring>

/* ══════════════════════════════════════════════════════════════════════════ */
/* COSEM-TCP Wrapper framing tests                                          */
/* ══════════════════════════════════════════════════════════════════════════ */

TEST_CASE("Wrapper: frame command", "[transport][wrapper]")
{
    uint8_t apdu[] = { 0xC0, 0x01, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x02, 0x00 };
    uint8_t out[64];

    int len = csm_wrapper_frame_command(apdu, sizeof(apdu), out, sizeof(out));
    REQUIRE(len == 3 + (int)sizeof(apdu));
    REQUIRE(out[0] == 0xE6);
    REQUIRE(out[1] == 0xE6);
    REQUIRE(out[2] == 0x00);
    REQUIRE(memcmp(out + 3, apdu, sizeof(apdu)) == 0);
}

TEST_CASE("Wrapper: frame response", "[transport][wrapper]")
{
    uint8_t apdu[] = { 0xC4, 0x01, 0x01, 0x00, 0x09, 0x00, 0x01, 0x00, 0x00, 0xFF };
    uint8_t out[64];

    int len = csm_wrapper_frame_response(apdu, sizeof(apdu), out, sizeof(out));
    REQUIRE(len == 3 + (int)sizeof(apdu));
    REQUIRE(out[0] == 0xE6);
    REQUIRE(out[1] == 0xE7);
    REQUIRE(out[2] == 0x00);
    REQUIRE(memcmp(out + 3, apdu, sizeof(apdu)) == 0);
}

TEST_CASE("Wrapper: deframe command", "[transport][wrapper]")
{
    uint8_t data[] = { 0xE6, 0xE6, 0x00, 0xC0, 0x01, 0x01, 0x02 };
    const uint8_t *apdu;
    uint32_t apdu_len;

    int rc = csm_wrapper_deframe(data, sizeof(data), &apdu, &apdu_len);
    REQUIRE(rc == CSM_TRANSPORT_OK);
    REQUIRE(apdu_len == 4);
    REQUIRE(apdu[0] == 0xC0);
    REQUIRE(apdu[1] == 0x01);
}

TEST_CASE("Wrapper: deframe response", "[transport][wrapper]")
{
    uint8_t data[] = { 0xE6, 0xE7, 0x00, 0xC4, 0x01 };
    const uint8_t *apdu;
    uint32_t apdu_len;

    int rc = csm_wrapper_deframe(data, sizeof(data), &apdu, &apdu_len);
    REQUIRE(rc == CSM_TRANSPORT_OK);
    REQUIRE(apdu_len == 2);
    REQUIRE(apdu[0] == 0xC4);
}

TEST_CASE("Wrapper: deframe bad prefix", "[transport][wrapper]")
{
    uint8_t data[] = { 0x00, 0x00, 0x00, 0xC0 };
    const uint8_t *apdu;
    uint32_t apdu_len;

    int rc = csm_wrapper_deframe(data, sizeof(data), &apdu, &apdu_len);
    REQUIRE(rc == CSM_TRANSPORT_ERR_FRAMING);
}

TEST_CASE("Wrapper: frame + deframe roundtrip", "[transport][wrapper]")
{
    uint8_t apdu[] = { 0xC0, 0x01, 0x03, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x01, 0x02, 0x03, 0x04 };
    uint8_t framed[64];

    int flen = csm_wrapper_frame_command(apdu, sizeof(apdu), framed, sizeof(framed));
    REQUIRE(flen > 0);

    const uint8_t *out_apdu;
    uint32_t out_len;
    int rc = csm_wrapper_deframe(framed, flen, &out_apdu, &out_len);
    REQUIRE(rc == CSM_TRANSPORT_OK);
    REQUIRE(out_len == sizeof(apdu));
    REQUIRE(memcmp(out_apdu, apdu, sizeof(apdu)) == 0);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Generic framing dispatch tests                                           */
/* ══════════════════════════════════════════════════════════════════════════ */

TEST_CASE("Framing: dispatch wrapper command", "[transport][framing]")
{
    uint8_t apdu[] = { 0xC0, 0x01 };
    uint8_t out[64];

    int len = csm_framing_frame(CSM_FRAMING_WRAPPER, 0, apdu, sizeof(apdu), out, sizeof(out));
    REQUIRE(len == 5);
    REQUIRE(out[0] == 0xE6);
    REQUIRE(out[1] == 0xE6);
    REQUIRE(out[2] == 0x00);
}

TEST_CASE("Framing: dispatch wrapper response", "[transport][framing]")
{
    uint8_t apdu[] = { 0xC4, 0x01 };
    uint8_t out[64];

    int len = csm_framing_frame(CSM_FRAMING_WRAPPER, 1, apdu, sizeof(apdu), out, sizeof(out));
    REQUIRE(len == 5);
    REQUIRE(out[1] == 0xE7);
}

TEST_CASE("Framing: raw passthrough", "[transport][framing]")
{
    uint8_t apdu[] = { 0xC0, 0x01, 0x02 };
    uint8_t out[64];

    int len = csm_framing_frame(CSM_FRAMING_NONE, 0, apdu, sizeof(apdu), out, sizeof(out));
    REQUIRE(len == 3);
    REQUIRE(memcmp(out, apdu, 3) == 0);
}

TEST_CASE("Framing: deframe raw", "[transport][framing]")
{
    uint8_t data[] = { 0xC0, 0x01, 0x02, 0x03 };
    const uint8_t *apdu;
    uint32_t apdu_len;

    int rc = csm_framing_deframe(CSM_FRAMING_NONE, data, sizeof(data), &apdu, &apdu_len);
    REQUIRE(rc == CSM_TRANSPORT_OK);
    REQUIRE(apdu_len == 4);
    REQUIRE(apdu == data);  /* No copy for raw framing */
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Transport helper macro tests                                             */
/* ══════════════════════════════════════════════════════════════════════════ */

TEST_CASE("Transport: null vtable safety", "[transport]")
{
    csm_transport t;
    t.ops = NULL;
    t.ctx = NULL;

    REQUIRE(CSM_TRANSPORT_OPEN(&t, 0) == CSM_TRANSPORT_ERR);
    REQUIRE(CSM_TRANSPORT_SEND(&t, 0, NULL, 0) == CSM_TRANSPORT_ERR);
    REQUIRE(CSM_TRANSPORT_RECV(&t, 0, NULL, 0, 0) == CSM_TRANSPORT_ERR);
    REQUIRE(CSM_TRANSPORT_IS_CONNECTED(&t, 0) == 0);
}

TEST_CASE("Transport: destroy clears state", "[transport]")
{
    /* Use a mock ops with NULL destroy to test the macro */
    static const csm_transport_ops mock_ops = { NULL, NULL, NULL, NULL, NULL, NULL };
    csm_transport t;
    t.ops = &mock_ops;
    t.ctx = (void *)0x1234;

    CSM_TRANSPORT_DESTROY(&t);
    REQUIRE(t.ops == nullptr);
    REQUIRE(t.ctx == nullptr);
}

typedef struct
{
    uint32_t last_timeout_ms;
    int send_calls;
    int recv_calls;
} timeout_transport_ctx_t;

static int timeout_transport_open(void *ctx, uint8_t channel)
{
    (void)ctx;
    (void)channel;
    return CSM_TRANSPORT_OK;
}

static int timeout_transport_send(void *ctx, uint8_t channel, const uint8_t *data, uint32_t len)
{
    timeout_transport_ctx_t *tctx = (timeout_transport_ctx_t *)ctx;
    (void)channel;
    (void)data;
    (void)len;
    tctx->send_calls++;
    return (int)len;
}

static int timeout_transport_recv(void *ctx, uint8_t channel, uint8_t *buf,
                                  uint32_t buf_size, uint32_t timeout_ms)
{
    timeout_transport_ctx_t *tctx = (timeout_transport_ctx_t *)ctx;
    (void)channel;
    (void)buf;
    (void)buf_size;
    tctx->recv_calls++;
    tctx->last_timeout_ms = timeout_ms;
    return CSM_TRANSPORT_ERR_TIMEOUT;
}

TEST_CASE("Client: connect uses configured receive timeout", "[transport][client]")
{
    static const csm_transport_ops timeout_ops = {
        timeout_transport_open,
        timeout_transport_send,
        timeout_transport_recv,
        NULL,
        NULL,
        NULL
    };
    timeout_transport_ctx_t ctx = {};
    csm_transport transport = { &timeout_ops, &ctx };
    csm_client *client = csm_client_create(&transport, 0, CSM_FRAMING_NONE);

    REQUIRE(client != nullptr);
    REQUIRE(csm_client_connect(client, 1234U) == CSM_TRANSPORT_ERR_TIMEOUT);
    REQUIRE(ctx.send_calls == 1);
    REQUIRE(ctx.recv_calls == 1);
    REQUIRE(ctx.last_timeout_ms == 1234U);

    csm_client_delete(client);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* HDLC framing tests                                                       */
/* ══════════════════════════════════════════════════════════════════════════ */

TEST_CASE("HDLC: find frame", "[transport][hdlc]")
{
    uint8_t stream[] = { 0x7E, 0x01, 0x02, 0x03, 0x7E, 0x04, 0x05 };
    const uint8_t *frame;
    uint32_t frame_len, consumed;

    int rc = csm_hdlc_find_frame(stream, sizeof(stream), &frame, &frame_len, &consumed);
    REQUIRE(rc == CSM_TRANSPORT_OK);
    REQUIRE(frame_len == 3);
    REQUIRE(frame[0] == 0x01);
    REQUIRE(consumed == 5);
}

TEST_CASE("HDLC: no frame", "[transport][hdlc]")
{
    uint8_t stream[] = { 0x01, 0x02, 0x03 };
    const uint8_t *frame;
    uint32_t frame_len, consumed;

    int rc = csm_hdlc_find_frame(stream, sizeof(stream), &frame, &frame_len, &consumed);
    REQUIRE(rc == CSM_TRANSPORT_ERR_TIMEOUT);
}

TEST_CASE("HDLC: empty stream", "[transport][hdlc]")
{
    const uint8_t *frame;
    uint32_t frame_len, consumed;

    int rc = csm_hdlc_find_frame(NULL, 0, &frame, &frame_len, &consumed);
    REQUIRE(rc == CSM_TRANSPORT_ERR);
}
