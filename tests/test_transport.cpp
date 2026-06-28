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
#include <chrono>
#include <thread>

extern "C" {
#include "csm_transport_tcp.h"
}

#if defined(_WIN32) || defined(WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET test_socket_t;
#define TEST_SOCKET_INVALID INVALID_SOCKET
static void test_close_socket(test_socket_t fd) { closesocket(fd); }
struct test_socket_runtime
{
    test_socket_runtime()
    {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }

    ~test_socket_runtime()
    {
        WSACleanup();
    }
};
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int test_socket_t;
#define TEST_SOCKET_INVALID (-1)
static void test_close_socket(test_socket_t fd) { close(fd); }
struct test_socket_runtime
{
};
#endif

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

TEST_CASE("TCP wrapper: frame + deframe roundtrip", "[transport][wrapper]")
{
    uint8_t apdu[] = { 0xC0, 0x01, 0x02, 0x03 };
    uint8_t framed[64];

    int flen = csm_tcp_wrapper_frame(0x0001U, 0x0010U, apdu, sizeof(apdu),
                                     framed, sizeof(framed));
    REQUIRE(flen == (int)(CSM_TCP_WRAPPER_LEN + sizeof(apdu)));
    REQUIRE(framed[0] == 0x00U);
    REQUIRE(framed[1] == 0x01U);
    REQUIRE(framed[2] == 0x00U);
    REQUIRE(framed[3] == 0x01U);
    REQUIRE(framed[4] == 0x00U);
    REQUIRE(framed[5] == 0x10U);
    REQUIRE(framed[6] == 0x00U);
    REQUIRE(framed[7] == sizeof(apdu));

    const uint8_t *out_apdu;
    uint32_t out_len;
    uint16_t source_wport = 0U;
    uint16_t dest_wport = 0U;
    int rc = csm_tcp_wrapper_deframe(framed, (uint32_t)flen, &out_apdu, &out_len,
                                     &source_wport, &dest_wport);
    REQUIRE(rc == CSM_TRANSPORT_OK);
    REQUIRE(source_wport == 0x0001U);
    REQUIRE(dest_wport == 0x0010U);
    REQUIRE(out_len == sizeof(apdu));
    REQUIRE(memcmp(out_apdu, apdu, sizeof(apdu)) == 0);
}

TEST_CASE("TCP wrapper: reject malformed frames", "[transport][wrapper]")
{
    uint8_t apdu[] = { 0xC0, 0x01 };
    uint8_t framed[64];
    const uint8_t *out_apdu;
    uint32_t out_len;

    int flen = csm_tcp_wrapper_frame(1U, 16U, apdu, sizeof(apdu), framed, sizeof(framed));
    REQUIRE(flen > 0);

    framed[1] = 0x02U;
    REQUIRE(csm_tcp_wrapper_deframe(framed, (uint32_t)flen, &out_apdu, &out_len,
                                    NULL, NULL) == CSM_TRANSPORT_ERR_FRAMING);

    framed[1] = 0x01U;
    framed[7] = 0x10U;
    REQUIRE(csm_tcp_wrapper_deframe(framed, (uint32_t)flen, &out_apdu, &out_len,
                                    NULL, NULL) == CSM_TRANSPORT_ERR_TIMEOUT);
}

static test_socket_t create_loopback_listener(uint16_t *port)
{
    test_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == TEST_SOCKET_INVALID)
    {
        return TEST_SOCKET_INVALID;
    }

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        test_close_socket(fd);
        return TEST_SOCKET_INVALID;
    }

    if (listen(fd, 1) < 0)
    {
        test_close_socket(fd);
        return TEST_SOCKET_INVALID;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(fd, (sockaddr *)&addr, &len) < 0)
    {
        test_close_socket(fd);
        return TEST_SOCKET_INVALID;
    }

    *port = ntohs(addr.sin_port);
    return fd;
}

TEST_CASE("TCP transport: receive split wrapper frame", "[transport][tcp]")
{
    test_socket_runtime sockets;
    uint16_t port = 0U;
    test_socket_t listen_fd = create_loopback_listener(&port);
    REQUIRE(listen_fd != TEST_SOCKET_INVALID);

    csm_transport transport;
    REQUIRE(csm_transport_tcp_client_init(&transport, "127.0.0.1", port,
                                          CSM_FRAMING_TCP_WRAPPER) == CSM_TRANSPORT_OK);
    REQUIRE(CSM_TRANSPORT_OPEN(&transport, 0U) == CSM_TRANSPORT_OK);

    sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    test_socket_t server_fd = accept(listen_fd, (sockaddr *)&peer, &peer_len);
    REQUIRE(server_fd != TEST_SOCKET_INVALID);

    uint8_t apdu[] = { 0xC4, 0x01, 0x02, 0x03 };
    uint8_t framed[64];
    int framed_len = csm_tcp_wrapper_frame(1U, 16U, apdu, sizeof(apdu), framed, sizeof(framed));
    REQUIRE(framed_len > 8);

    std::thread sender([server_fd, framed, framed_len]() {
        send(server_fd, (const char *)framed, 4, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send(server_fd, (const char *)(framed + 4), framed_len - 4, 0);
    });

    uint8_t out[16];
    int rc = CSM_TRANSPORT_RECV(&transport, 0U, out, sizeof(out), 1000U);

    sender.join();
    test_close_socket(server_fd);
    test_close_socket(listen_fd);
    csm_transport_tcp_destroy(&transport);

    REQUIRE(rc == (int)sizeof(apdu));
    REQUIRE(memcmp(out, apdu, sizeof(apdu)) == 0);
}

TEST_CASE("TCP transport: server accept respects receive timeout", "[transport][tcp]")
{
    test_socket_runtime sockets;
    csm_transport transport;

    REQUIRE(csm_transport_tcp_server_init(&transport, 0U,
                                          CSM_FRAMING_TCP_WRAPPER) == CSM_TRANSPORT_OK);
    REQUIRE(CSM_TRANSPORT_OPEN(&transport, 0U) == CSM_TRANSPORT_OK);

    uint8_t out[16];
    REQUIRE(CSM_TRANSPORT_RECV(&transport, 0U, out, sizeof(out), 20U) ==
            CSM_TRANSPORT_ERR_TIMEOUT);

    csm_transport_tcp_destroy(&transport);
}

TEST_CASE("TCP transport: manual accept respects timeout", "[transport][tcp]")
{
    test_socket_runtime sockets;
    csm_transport transport;

    REQUIRE(csm_transport_tcp_server_init(&transport, 0U,
                                          CSM_FRAMING_TCP_WRAPPER) == CSM_TRANSPORT_OK);
    REQUIRE(CSM_TRANSPORT_OPEN(&transport, 0U) == CSM_TRANSPORT_OK);
    REQUIRE(csm_transport_tcp_accept(&transport, 20U) == CSM_TRANSPORT_ERR_TIMEOUT);

    csm_transport_tcp_destroy(&transport);
}

TEST_CASE("TCP transport: public connect and accept reject null contexts", "[transport][tcp]")
{
    csm_transport transport;
    transport.ops = nullptr;
    transport.ctx = nullptr;

    REQUIRE(csm_transport_tcp_connect(nullptr, 0U) == CSM_TRANSPORT_ERR);
    REQUIRE(csm_transport_tcp_connect(&transport, 0U) == CSM_TRANSPORT_ERR);
    REQUIRE(csm_transport_tcp_accept(nullptr, 0U) == CSM_TRANSPORT_ERR);
    REQUIRE(csm_transport_tcp_accept(&transport, 0U) == CSM_TRANSPORT_ERR);
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
