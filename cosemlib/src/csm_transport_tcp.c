/**
 * \file csm_transport_tcp.c
 * \brief COSEM-TCP transport implementation (IEC 62056-5-3)
 *
 *  Uses POSIX sockets. Platform-specific socket code is isolated here.
 *  For Windows, a thin abstraction is provided via os_util.h.
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "csm_transport_tcp.h"
#include "csm_framing.h"
#include <string.h>

/* ── Platform socket abstraction ─────────────────────────────────────────── */

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define SOCK_INVALID INVALID_SOCKET
    #define SOCK_CLOSE closesocket
    #define SOCK_ERR WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int socket_t;
    #define SOCK_INVALID (-1)
    #define SOCK_CLOSE close
    #define SOCK_ERR errno
#endif

#define CSM_TCP_CONTEXT_POOL_SIZE 16U

/* ── Internal channel state ─────────────────────────────────────────────── */

typedef struct {
    socket_t fd;                    /*!< Socket file descriptor */
    int      connected;            /*!< 1 if connected, 0 otherwise */
    uint8_t  recv_buf[CSM_TCP_RECV_BUF]; /*!< Receive ring buffer */
    uint32_t recv_len;             /*!< Bytes in recv buffer */
} csm_tcp_channel;

/* ── TCP context ────────────────────────────────────────────────────────── */

struct csm_tcp_context {
    csm_framing_type  framing;      /*!< Framing type */
    int               is_server;    /*!< 1 = server (listener), 0 = client */
    socket_t          listen_fd;    /*!< Server: listening socket */
    char              host[256];    /*!< Client: remote host */
    uint16_t          port;         /*!< Port number */
    csm_tcp_channel   channels[CSM_TCP_MAX_CLIENTS];
    uint8_t           channel_count;
    csm_transport_event_fn event_cb; /*!< Event callback */
    void             *event_ctx;    /*!< Event callback context */
};

static csm_tcp_context tcp_context_pool[CSM_TCP_CONTEXT_POOL_SIZE];
static uint8_t tcp_context_used[CSM_TCP_CONTEXT_POOL_SIZE];

/* ── Platform helpers ───────────────────────────────────────────────────── */

static void socket_set_nonblocking(socket_t fd)
{
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

static void socket_set_blocking(socket_t fd)
{
#ifdef _WIN32
    u_long mode = 0;
    ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
    {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
}

static int socket_set_nodelay(socket_t fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(flag));
}

static int socket_set_reuseaddr(socket_t fd)
{
    int flag = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&flag, sizeof(flag));
}

/* ── Internal: find free channel slot ───────────────────────────────────── */

static int tcp_find_free_channel(csm_tcp_context *ctx)
{
    for (uint8_t i = 0; i < CSM_TCP_MAX_CLIENTS; i++)
    {
        if (!ctx->channels[i].connected)
        {
            return i;
        }
    }
    return -1;
}

static csm_tcp_context *tcp_alloc_context(void)
{
    for (uint8_t i = 0; i < CSM_TCP_CONTEXT_POOL_SIZE; i++)
    {
        if (!tcp_context_used[i])
        {
            tcp_context_used[i] = 1U;
            return &tcp_context_pool[i];
        }
    }
    return NULL;
}

static void tcp_release_context(csm_tcp_context *ctx)
{
    for (uint8_t i = 0; i < CSM_TCP_CONTEXT_POOL_SIZE; i++)
    {
        if (ctx == &tcp_context_pool[i])
        {
            tcp_context_used[i] = 0U;
            return;
        }
    }
}

/* ── Internal: close a channel ──────────────────────────────────────────── */

static void tcp_close_channel(csm_tcp_context *ctx, uint8_t channel)
{
    if (channel < CSM_TCP_MAX_CLIENTS && ctx->channels[channel].connected)
    {
        SOCK_CLOSE(ctx->channels[channel].fd);
        ctx->channels[channel].fd = SOCK_INVALID;
        ctx->channels[channel].connected = 0;
        ctx->channels[channel].recv_len = 0;

        if (ctx->event_cb)
        {
            ctx->event_cb(channel, CSM_TRANSPORT_EVT_DISCONNECTED,
                          NULL, 0, ctx->event_ctx);
        }
    }
}

/* ── Internal: receive from socket into channel buffer ──────────────────── */

static int tcp_fill_recv_buf(csm_tcp_context *ctx, uint8_t channel)
{
    csm_tcp_channel *ch = &ctx->channels[channel];
    if (!ch->connected) return CSM_TRANSPORT_ERR_CONN;

    uint32_t space = sizeof(ch->recv_buf) - ch->recv_len;
    if (space == 0) return CSM_TRANSPORT_ERR_OVERFLOW;

    int n = (int)recv(ch->fd, (char *)(ch->recv_buf + ch->recv_len), space, 0);
    if (n <= 0)
    {
        if (n == 0)  /* Connection closed by peer */
        {
            tcp_close_channel(ctx, channel);
            return CSM_TRANSPORT_ERR_CONN;
        }
        return CSM_TRANSPORT_ERR_IO;
    }

    ch->recv_len += (uint32_t)n;
    return CSM_TRANSPORT_OK;
}

static int tcp_wait_readable(socket_t fd, uint32_t timeout_ms)
{
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);

    struct timeval tv;
    struct timeval *tv_ptr = NULL;
    if (timeout_ms != UINT32_MAX)
    {
        tv.tv_sec = (long)(timeout_ms / 1000U);
        tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);
        tv_ptr = &tv;
    }

    int rc = select((int)fd + 1, &read_set, NULL, NULL, tv_ptr);
    if (rc == 0) return CSM_TRANSPORT_ERR_TIMEOUT;
    if (rc < 0) return CSM_TRANSPORT_ERR_IO;
    return CSM_TRANSPORT_OK;
}

static int tcp_wait_writable(socket_t fd, uint32_t timeout_ms)
{
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(fd, &write_set);

    struct timeval tv;
    struct timeval *tv_ptr = NULL;
    if (timeout_ms != UINT32_MAX)
    {
        tv.tv_sec = (long)(timeout_ms / 1000U);
        tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);
        tv_ptr = &tv;
    }

    int rc = select((int)fd + 1, NULL, &write_set, NULL, tv_ptr);
    if (rc == 0) return CSM_TRANSPORT_ERR_TIMEOUT;
    if (rc < 0) return CSM_TRANSPORT_ERR_IO;
    return CSM_TRANSPORT_OK;
}

static int tcp_connect_in_progress(int err)
{
#ifdef _WIN32
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEINVAL;
#else
    return err == EINPROGRESS || err == EWOULDBLOCK;
#endif
}

static int tcp_socket_connect_error(socket_t fd)
{
    int err = 0;
#ifdef _WIN32
    int len = sizeof(err);
#else
    socklen_t len = sizeof(err);
#endif
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len) < 0)
    {
        return CSM_TRANSPORT_ERR_IO;
    }
    return err == 0 ? CSM_TRANSPORT_OK : CSM_TRANSPORT_ERR_IO;
}

/* ── Internal: extract one framed PDU from recv buffer ──────────────────── */

static int tcp_extract_pdu(csm_tcp_context *ctx, uint8_t channel,
                            uint8_t *out, uint32_t out_size,
                            uint32_t *consumed)
{
    csm_tcp_channel *ch = &ctx->channels[channel];
    if (!out || out_size == 0U || !consumed) return CSM_TRANSPORT_ERR;
    if (ch->recv_len == 0) return 0;

    if (ctx->framing == CSM_FRAMING_WRAPPER)
    {
        /* Need at least 3 bytes for LLC prefix + 1 byte minimum APDU */
        if (ch->recv_len < 4) return 0;

        const uint8_t *apdu;
        uint32_t apdu_len;
        int rc = csm_wrapper_deframe(ch->recv_buf, ch->recv_len, &apdu, &apdu_len);
        if (rc == CSM_TRANSPORT_ERR_FRAMING)
        {
            /* Invalid LLC prefix — drop one byte and try again */
            memmove(ch->recv_buf, ch->recv_buf + 1, ch->recv_len - 1);
            ch->recv_len--;
            return 0;
        }

        if (rc != CSM_TRANSPORT_OK) return 0;

        /* Copy APDU to output */
        if (apdu_len > out_size) return CSM_TRANSPORT_ERR_OVERFLOW;
        memcpy(out, apdu, apdu_len);
        *consumed = (uint32_t)(apdu + apdu_len - ch->recv_buf);
        return (int)apdu_len;
    }
    else if (ctx->framing == CSM_FRAMING_TCP_WRAPPER)
    {
        if (ch->recv_len < CSM_TCP_WRAPPER_LEN) return 0;

        uint16_t apdu_len = (uint16_t)((ch->recv_buf[6] << 8U) | ch->recv_buf[7]);
        uint32_t total_len = CSM_TCP_WRAPPER_LEN + (uint32_t)apdu_len;
        if (total_len > sizeof(ch->recv_buf)) return CSM_TRANSPORT_ERR_OVERFLOW;
        if (ch->recv_len < total_len) return 0;

        const uint8_t *apdu;
        uint32_t decoded_len;
        int rc = csm_tcp_wrapper_deframe(ch->recv_buf, total_len, &apdu, &decoded_len, NULL, NULL);
        if (rc == CSM_TRANSPORT_ERR_FRAMING)
        {
            memmove(ch->recv_buf, ch->recv_buf + 1, ch->recv_len - 1);
            ch->recv_len--;
            return 0;
        }
        if (rc != CSM_TRANSPORT_OK) return rc;
        if (decoded_len > out_size) return CSM_TRANSPORT_ERR_OVERFLOW;

        memcpy(out, apdu, decoded_len);
        *consumed = total_len;
        return (int)decoded_len;
    }
    else if (ctx->framing == CSM_FRAMING_HDLC)
    {
        const uint8_t *frame;
        uint32_t frame_len;
        int rc = csm_hdlc_find_frame(ch->recv_buf, ch->recv_len,
                                      &frame, &frame_len, consumed);
        if (rc != CSM_TRANSPORT_OK) return 0;

        if (frame_len > out_size) return CSM_TRANSPORT_ERR_OVERFLOW;
        memcpy(out, frame, frame_len);
        return (int)frame_len;
    }
    else /* CSM_FRAMING_NONE — raw */
    {
        /* Return whatever is in the buffer */
        uint32_t len = ch->recv_len;
        if (len > out_size) len = out_size;
        memcpy(out, ch->recv_buf, len);
        *consumed = len;
        return (int)len;
    }
}

/* ── Vtable implementation ──────────────────────────────────────────────── */

static int tcp_open(void *ctx, uint8_t channel)
{
    csm_tcp_context *c = (csm_tcp_context *)ctx;
    if (channel >= CSM_TCP_MAX_CLIENTS) return CSM_TRANSPORT_ERR;

    if (c->is_server)
    {
        /* Server: listen and accept */
        c->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (c->listen_fd == SOCK_INVALID) return CSM_TRANSPORT_ERR_IO;

        socket_set_reuseaddr(c->listen_fd);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(c->port);

        if (bind(c->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            SOCK_CLOSE(c->listen_fd);
            c->listen_fd = SOCK_INVALID;
            return CSM_TRANSPORT_ERR_IO;
        }

        if (listen(c->listen_fd, CSM_TCP_BACKLOG) < 0)
        {
            SOCK_CLOSE(c->listen_fd);
            c->listen_fd = SOCK_INVALID;
            return CSM_TRANSPORT_ERR_IO;
        }

        return CSM_TRANSPORT_OK;
    }
    else
    {
        csm_transport transport = { NULL, c };
        return csm_transport_tcp_connect(&transport, UINT32_MAX);
    }
}

static int tcp_send(void *ctx, uint8_t channel, const uint8_t *data, uint32_t len)
{
    csm_tcp_context *c = (csm_tcp_context *)ctx;
    if (channel >= CSM_TCP_MAX_CLIENTS) return CSM_TRANSPORT_ERR;
    if (!c->channels[channel].connected) return CSM_TRANSPORT_ERR_CONN;

    /* Frame the data */
    uint8_t framed[CSM_WRAPPER_MAX_LEN];
    int framed_len = csm_framing_frame(c->framing, 1, data, len, framed, sizeof(framed));
    if (framed_len < 0) return framed_len;

    int sent = 0;
    while (sent < framed_len)
    {
        int n = (int)send(c->channels[channel].fd,
                          (const char *)(framed + sent), framed_len - sent, 0);
        if (n <= 0) return CSM_TRANSPORT_ERR_IO;
        sent += n;
    }

    return sent;
}

static int tcp_recv(void *ctx, uint8_t channel, uint8_t *buf, uint32_t buf_size,
                    uint32_t timeout_ms)
{
    csm_tcp_context *c = (csm_tcp_context *)ctx;
    if (channel >= CSM_TCP_MAX_CLIENTS) return CSM_TRANSPORT_ERR;
    if (!buf || buf_size == 0U) return CSM_TRANSPORT_ERR;

    /* Server: accept new connection if this channel is not connected */
    if (c->is_server && !c->channels[channel].connected)
    {
        struct sockaddr_in addr;
#ifdef _WIN32
        int addr_len = sizeof(addr);
#else
        socklen_t addr_len = sizeof(addr);
#endif
        int wait_rc = tcp_wait_readable(c->listen_fd, timeout_ms);
        if (wait_rc != CSM_TRANSPORT_OK) return wait_rc;

        socket_t client_fd = accept(c->listen_fd, (struct sockaddr *)&addr, &addr_len);
        if (client_fd == SOCK_INVALID) return CSM_TRANSPORT_ERR_TIMEOUT;

        socket_set_nodelay(client_fd);

        c->channels[channel].fd = client_fd;
        c->channels[channel].connected = 1;
        c->channels[channel].recv_len = 0;

        if (c->event_cb)
        {
            c->event_cb(channel, CSM_TRANSPORT_EVT_CONNECTED,
                        NULL, 0, c->event_ctx);
        }
    }

    if (!c->channels[channel].connected) return CSM_TRANSPORT_ERR_CONN;

    for (;;)
    {
        /* Try to extract a complete PDU from the receive buffer */
        uint32_t consumed = 0;
        int pdu_len = tcp_extract_pdu(c, channel, buf, buf_size, &consumed);

        if (pdu_len < 0)
        {
            return pdu_len;
        }

        if (pdu_len > 0 && consumed > 0)
        {
            /* Remove consumed bytes from buffer */
            memmove(c->channels[channel].recv_buf,
                    c->channels[channel].recv_buf + consumed,
                    c->channels[channel].recv_len - consumed);
            c->channels[channel].recv_len -= consumed;
            return pdu_len;
        }

        int wait_rc = tcp_wait_readable(c->channels[channel].fd, timeout_ms);
        if (wait_rc != CSM_TRANSPORT_OK) return wait_rc;

        /* No complete PDU yet — receive more data and try again. */
        int rc = tcp_fill_recv_buf(c, channel);
        if (rc != CSM_TRANSPORT_OK) return rc;
    }
}

static void tcp_close(void *ctx, uint8_t channel)
{
    csm_tcp_context *c = (csm_tcp_context *)ctx;
    if (channel < CSM_TCP_MAX_CLIENTS)
    {
        tcp_close_channel(c, channel);
    }
}

static int tcp_is_connected(void *ctx, uint8_t channel)
{
    csm_tcp_context *c = (csm_tcp_context *)ctx;
    if (channel >= CSM_TCP_MAX_CLIENTS) return 0;
    return c->channels[channel].connected;
}

static void tcp_destroy(void *ctx)
{
    csm_tcp_context *c = (csm_tcp_context *)ctx;
    if (!c) return;

    for (uint8_t i = 0; i < CSM_TCP_MAX_CLIENTS; i++)
    {
        if (c->channels[i].connected)
        {
            SOCK_CLOSE(c->channels[i].fd);
        }
    }

    if (c->listen_fd != SOCK_INVALID)
    {
        SOCK_CLOSE(c->listen_fd);
    }

    memset(c, 0, sizeof(*c));
    tcp_release_context(c);
}

static const csm_transport_ops tcp_ops = {
    .open         = tcp_open,
    .send         = tcp_send,
    .recv         = tcp_recv,
    .close        = tcp_close,
    .is_connected = tcp_is_connected,
    .destroy      = tcp_destroy
};

/* ── Public API ─────────────────────────────────────────────────────────── */

const csm_transport_ops *csm_transport_tcp_ops(void)
{
    return &tcp_ops;
}

static void tcp_init_context(csm_tcp_context *c, csm_framing_type framing)
{
    memset(c, 0, sizeof(*c));
    c->framing = framing;
    c->listen_fd = SOCK_INVALID;
    for (uint8_t i = 0; i < CSM_TCP_MAX_CLIENTS; i++)
    {
        c->channels[i].fd = SOCK_INVALID;
    }
}

int csm_transport_tcp_server_init(csm_transport *transport, uint16_t port,
                                   csm_framing_type framing)
{
    if (!transport) return CSM_TRANSPORT_ERR;

    csm_tcp_context *server_ctx = tcp_alloc_context();
    if (!server_ctx) return CSM_TRANSPORT_ERR_OVERFLOW;

    tcp_init_context(server_ctx, framing);
    server_ctx->is_server = 1;
    server_ctx->port = port;

    transport->ops = &tcp_ops;
    transport->ctx = server_ctx;

    return CSM_TRANSPORT_OK;
}

int csm_transport_tcp_client_init(csm_transport *transport, const char *host,
                                   uint16_t port, csm_framing_type framing)
{
    if (!transport || !host) return CSM_TRANSPORT_ERR;

    csm_tcp_context *client_ctx = tcp_alloc_context();
    if (!client_ctx) return CSM_TRANSPORT_ERR_OVERFLOW;

    tcp_init_context(client_ctx, framing);
    client_ctx->is_server = 0;
    client_ctx->port = port;

    size_t host_len = strlen(host);
    if (host_len >= sizeof(client_ctx->host)) host_len = sizeof(client_ctx->host) - 1;
    memcpy(client_ctx->host, host, host_len);
    client_ctx->host[host_len] = '\0';

    transport->ops = &tcp_ops;
    transport->ctx = client_ctx;

    return CSM_TRANSPORT_OK;
}

void csm_transport_tcp_set_event_cb(csm_transport *transport,
                                     csm_transport_event_fn callback,
                                     void *user_ctx)
{
    if (!transport || !transport->ctx) return;
    csm_tcp_context *c = (csm_tcp_context *)transport->ctx;
    c->event_cb = callback;
    c->event_ctx = user_ctx;
}

int csm_transport_tcp_accept(csm_transport *transport, uint32_t timeout_ms)
{
    if (!transport || !transport->ctx) return CSM_TRANSPORT_ERR;
    csm_tcp_context *c = (csm_tcp_context *)transport->ctx;

    if (!c->is_server || c->listen_fd == SOCK_INVALID)
    {
        return CSM_TRANSPORT_ERR;
    }

    int ch = tcp_find_free_channel(c);
    if (ch < 0) return CSM_TRANSPORT_ERR_OVERFLOW;

    struct sockaddr_in addr;
#ifdef _WIN32
    int addr_len = sizeof(addr);
#else
    socklen_t addr_len = sizeof(addr);
#endif
    int wait_rc = tcp_wait_readable(c->listen_fd, timeout_ms);
    if (wait_rc != CSM_TRANSPORT_OK) return wait_rc;

    socket_t client_fd = accept(c->listen_fd, (struct sockaddr *)&addr, &addr_len);
    if (client_fd == SOCK_INVALID) return CSM_TRANSPORT_ERR_TIMEOUT;

    socket_set_nodelay(client_fd);

    c->channels[ch].fd = client_fd;
    c->channels[ch].connected = 1;
    c->channels[ch].recv_len = 0;

    return ch;
}

int csm_transport_tcp_connect(csm_transport *transport, uint32_t timeout_ms)
{
    if (!transport || !transport->ctx) return CSM_TRANSPORT_ERR;
    csm_tcp_context *c = (csm_tcp_context *)transport->ctx;
    if (c->channels[0].connected) return CSM_TRANSPORT_OK;

    socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == SOCK_INVALID) return CSM_TRANSPORT_ERR_IO;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(c->port);

    if (inet_pton(AF_INET, c->host, &addr.sin_addr) != 1)
    {
        SOCK_CLOSE(fd);
        return CSM_TRANSPORT_ERR;
    }

    socket_set_nonblocking(fd);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        if (!tcp_connect_in_progress(SOCK_ERR))
        {
            SOCK_CLOSE(fd);
            return CSM_TRANSPORT_ERR_IO;
        }

        int wait_rc = tcp_wait_writable(fd, timeout_ms);
        if (wait_rc != CSM_TRANSPORT_OK)
        {
            SOCK_CLOSE(fd);
            return wait_rc;
        }

        int socket_rc = tcp_socket_connect_error(fd);
        if (socket_rc != CSM_TRANSPORT_OK)
        {
            SOCK_CLOSE(fd);
            return socket_rc;
        }
    }

    socket_set_blocking(fd);
    socket_set_nodelay(fd);

    c->channels[0].fd = fd;
    c->channels[0].connected = 1;
    c->channels[0].recv_len = 0;

    if (c->event_cb)
    {
        c->event_cb(0, CSM_TRANSPORT_EVT_CONNECTED, NULL, 0, c->event_ctx);
    }

    return CSM_TRANSPORT_OK;
}

void csm_transport_tcp_destroy(csm_transport *transport)
{
    if (transport && transport->ops)
    {
        tcp_destroy(transport->ctx);
        transport->ops = NULL;
        transport->ctx = NULL;
    }
}
