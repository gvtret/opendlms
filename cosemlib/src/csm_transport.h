/**
 * \file csm_transport.h
 * \brief Transport abstraction for DLMS/COSEM
 *
 *  Defines the transport vtable and framing interface that sits between
 *  raw I/O (TCP socket, serial port) and the channel/association layer.
 *
 *  Architecture:
 *    Application → csm_server_poll() → Channel/Association → Framing → Transport
 *
 *  Transport implementations:
 *    - csm_transport_tcp: COSEM-TCP (IEC 62056-5-3) with wrapper framing
 *    - csm_transport_hdlc: HDLC over serial (IEC 62056-46)
 *    - csm_transport_raw: Raw TCP (no framing, for testing)
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef CSM_TRANSPORT_H
#define CSM_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ── Limits ─────────────────────────────────────────────────────────────── */

#define CSM_TRANSPORT_MAX_CHANNELS     8
#define CSM_TRANSPORT_MAX_PDU_SIZE   2048
#define CSM_TRANSPORT_DEFAULT_TIMEOUT 5000

/* ── Transport error codes ──────────────────────────────────────────────── */

#define CSM_TRANSPORT_OK            0
#define CSM_TRANSPORT_ERR          -1
#define CSM_TRANSPORT_ERR_TIMEOUT  -2
#define CSM_TRANSPORT_ERR_CONN     -3
#define CSM_TRANSPORT_ERR_IO       -4
#define CSM_TRANSPORT_ERR_FRAMING  -5
#define CSM_TRANSPORT_ERR_OVERFLOW -6

/* ── Transport events (for event-driven mode) ───────────────────────────── */

typedef enum {
    CSM_TRANSPORT_EVT_CONNECTED,     /*!< New connection accepted (server) or established (client) */
    CSM_TRANSPORT_EVT_DATA,          /*!< Data available on a channel */
    CSM_TRANSPORT_EVT_DISCONNECTED,  /*!< Connection closed */
    CSM_TRANSPORT_EVT_ERROR          /*!< I/O error */
} csm_transport_event;

/**
 * \brief Event callback invoked by the transport when an event occurs
 *
 * \param channel    Channel index (0-based)
 * \param event      Event type
 * \param data       Event-specific data (NULL for most events)
 * \param data_len   Length of event data
 * \param user_ctx   User context pointer (passed during transport registration)
 */
typedef void (*csm_transport_event_fn)(uint8_t channel, csm_transport_event event,
                                       const uint8_t *data, uint32_t data_len,
                                       void *user_ctx);

/* ── Transport vtable ───────────────────────────────────────────────────── */

/**
 * \brief Transport vtable — abstracts I/O operations
 *
 *  Each transport backend (TCP, HDLC, etc.) implements this vtable.
 *  The channel manager calls these functions to send/receive APDUs.
 *
 *  The send function takes a complete, framed PDU ready for the wire.
 *  The recv function receives bytes and returns the next complete PDU.
 *  Both operate on a per-channel basis (channel 0 = first connection).
 */
typedef struct csm_transport_ops {
    /**
     * \brief Open a transport connection (client mode) or start listening (server mode)
     *
     * \param ctx       Transport context (socket fd, etc.)
     * \param channel   Channel index to use
     * \return CSM_TRANSPORT_OK on success, negative error code on failure
     */
    int (*open)(void *ctx, uint8_t channel);

    /**
     * \brief Send a framed PDU on a channel
     *
     * \param ctx       Transport context
     * \param channel   Channel index
     * \param data      Pointer to framed PDU bytes
     * \param len       Length of PDU in bytes
     * \return Number of bytes sent, or negative error code
     */
    int (*send)(void *ctx, uint8_t channel, const uint8_t *data, uint32_t len);

    /**
     * \brief Receive the next complete PDU from a channel (blocking)
     *
     * \param ctx       Transport context
     * \param channel   Channel index
     * \param buf       Output buffer for received PDU
     * \param buf_size  Size of output buffer
     * \param timeout_ms Timeout in milliseconds (0 = non-blocking, UINT32_MAX = infinite)
     * \return Number of bytes received, 0 on timeout, or negative error code
     */
    int (*recv)(void *ctx, uint8_t channel, uint8_t *buf, uint32_t buf_size,
                uint32_t timeout_ms);

    /**
     * \brief Close a transport connection
     *
     * \param ctx       Transport context
     * \param channel   Channel index
     */
    void (*close)(void *ctx, uint8_t channel);

    /**
     * \brief Check if a channel is connected
     *
     * \param ctx       Transport context
     * \param channel   Channel index
     * \return 1 if connected, 0 if not
     */
    int (*is_connected)(void *ctx, uint8_t channel);

    /**
     * \brief Free transport resources
     *
     * \param ctx       Transport context
     */
    void (*destroy)(void *ctx);
} csm_transport_ops;

/* ── Transport instance ─────────────────────────────────────────────────── */

/**
 * \brief Transport instance — pairs a vtable with its context
 */
typedef struct csm_transport {
    const csm_transport_ops *ops;   /*!< Transport operations */
    void *ctx;                      /*!< Transport-specific context (cast to implementation type) */
} csm_transport;

/* ── Helper macros ──────────────────────────────────────────────────────── */

#define CSM_TRANSPORT_OPEN(t, ch)     ((t)->ops ? (t)->ops->open((t)->ctx, (ch)) : CSM_TRANSPORT_ERR)
#define CSM_TRANSPORT_SEND(t, ch, d, l) ((t)->ops ? (t)->ops->send((t)->ctx, (ch), (d), (l)) : CSM_TRANSPORT_ERR)
#define CSM_TRANSPORT_RECV(t, ch, b, s, ms) ((t)->ops ? (t)->ops->recv((t)->ctx, (ch), (b), (s), (ms)) : CSM_TRANSPORT_ERR)
#define CSM_TRANSPORT_CLOSE(t, ch)   do { if ((t)->ops && (t)->ops->close) (t)->ops->close((t)->ctx, (ch)); } while(0)
#define CSM_TRANSPORT_IS_CONNECTED(t, ch) ((t)->ops && (t)->ops->is_connected ? (t)->ops->is_connected((t)->ctx, (ch)) : 0)
#define CSM_TRANSPORT_DESTROY(t)     do { if ((t)->ops) { if ((t)->ops->destroy) (t)->ops->destroy((t)->ctx); (t)->ops = NULL; (t)->ctx = NULL; } } while(0)

#ifdef __cplusplus
}
#endif

#endif /* CSM_TRANSPORT_H */
