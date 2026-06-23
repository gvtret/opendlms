/**
 * \file csm_transport_tcp.h
 * \brief COSEM-TCP transport (IEC 62056-5-3)
 *
 *  Implements the TCP transport with COSEM-TCP wrapper framing.
 *  Supports both server (listener) and client (connector) modes.
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef CSM_TRANSPORT_TCP_H
#define CSM_TRANSPORT_TCP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "csm_transport.h"
#include "csm_framing.h"

/* ── TCP transport configuration ────────────────────────────────────────── */

#define CSM_TCP_MAX_CLIENTS  8
#define CSM_TCP_BACKLOG      8
#define CSM_TCP_RECV_BUF     4096

/**
 * \brief TCP transport context (opaque to users)
 */
typedef struct csm_tcp_context csm_tcp_context;

/**
 * \brief Create a TCP transport for server mode (listening)
 *
 *  Binds to the specified port and listens for incoming connections.
 *  The transport will accept up to CSM_TCP_MAX_CLIENTS connections.
 *
 *  Usage:
 *    csm_transport tcp;
 *    csm_transport_tcp_server_init(&tcp, 4060, CSM_FRAMING_WRAPPER);
 *    // ... main loop: recv on channel 0, process, send response ...
 *    csm_transport_tcp_destroy(&tcp);
 *
 * \param transport  Output: transport instance
 * \param port       TCP port to listen on (e.g., 4060 for DLMS)
 * \param framing    Framing type (CSM_FRAMING_WRAPPER for standard COSEM-TCP)
 * \return CSM_TRANSPORT_OK on success
 */
int csm_transport_tcp_server_init(csm_transport *transport, uint16_t port,
                                   csm_framing_type framing);

/**
 * \brief Create a TCP transport for client mode (connecting)
 *
 *  Usage:
 *    csm_transport tcp;
 *    csm_transport_tcp_client_init(&tcp, "192.168.1.100", 4060, CSM_FRAMING_WRAPPER);
 *    CSM_TRANSPORT_OPEN(&tcp, 0);
 *    // ... send AARQ, receive AARE, etc. ...
 *    CSM_TRANSPORT_CLOSE(&tcp, 0);
 *    csm_transport_tcp_destroy(&tcp);
 *
 * \param transport  Output: transport instance
 * \param host       Remote hostname or IP address
 * \param port       Remote TCP port
 * \param framing    Framing type
 * \return CSM_TRANSPORT_OK on success
 */
int csm_transport_tcp_client_init(csm_transport *transport, const char *host,
                                   uint16_t port, csm_framing_type framing);

/**
 * \brief Set the event callback for asynchronous operation
 *
 *  If set, the transport will use non-blocking I/O and invoke the callback
 *  when data arrives or connections change. If not set, the transport uses
 *  blocking I/O with csm_transport_ops.recv().
 *
 * \param transport  Transport instance
 * \param callback   Event callback function (NULL to disable)
 * \param user_ctx   User context passed to callback
 */
void csm_transport_tcp_set_event_cb(csm_transport *transport,
                                     csm_transport_event_fn callback,
                                     void *user_ctx);

/**
 * \brief Accept a pending connection (server mode, manual accept)
 *
 *  In blocking mode, this accepts the next pending connection.
 *  The returned channel can be used for send/recv.
 *
 * \param transport  Transport instance
 * \param timeout_ms Timeout in milliseconds
 * \return Channel index (>= 0) on success, CSM_TRANSPORT_ERR_TIMEOUT on timeout
 */
int csm_transport_tcp_accept(csm_transport *transport, uint32_t timeout_ms);

/**
 * \brief Connect to a remote server (client mode, manual connect)
 *
 * \param transport  Transport instance
 * \param timeout_ms Timeout in milliseconds
 * \return CSM_TRANSPORT_OK on success, error code on failure
 */
int csm_transport_tcp_connect(csm_transport *transport, uint32_t timeout_ms);

/**
 * \brief Destroy TCP transport and free all resources
 */
void csm_transport_tcp_destroy(csm_transport *transport);

/**
 * \brief Get the vtable for TCP transport (for use with csm_transport directly)
 */
const csm_transport_ops *csm_transport_tcp_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* CSM_TRANSPORT_TCP_H */
