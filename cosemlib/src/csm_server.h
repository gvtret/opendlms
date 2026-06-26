/**
 * \file csm_server.h
 * \brief High-level DLMS/COSEM server
 *
 *  Integrates transport, framing, channel/association, and IC dispatch
 *  into a simple poll-based server loop.
 *
 *  Usage (minimal server):
 *    csm_server server;
 *    csm_server_init(&server, &transport, 0, CSM_FRAMING_WRAPPER);
 *    csm_server_register_db(&server, my_db_access);
 *    while (running) {
 *        csm_server_poll(&server, 100);
 *    }
 *    csm_server_destroy(&server);
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef CSM_SERVER_H
#define CSM_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "csm_transport.h"
#include "csm_framing.h"
#include "csm_services.h"

/* ── Limits ─────────────────────────────────────────────────────────────── */

#define CSM_SERVER_MAX_CHANNELS  8
#define CSM_SERVER_MAX_PDU    2048

/* ── Server context ─────────────────────────────────────────────────────── */

typedef struct csm_server csm_server;

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * \brief Initialize a DLMS/COSEM server
 *
 * \param server     Server context (caller-allocated)
 * \param transport  Transport instance (TCP, HDLC, etc.)
 * \param channel    Primary channel index (typically 0)
 * \param framing    Framing type for this connection
 * \return 0 on success
 */
int csm_server_init(csm_server *server, csm_transport *transport,
                    uint8_t channel, csm_framing_type framing);

/**
 * \brief Register the database access handler
 *
 *  This callback is invoked for every incoming service request (GET/SET/ACTION).
 *
 * \param server     Server context
 * \param handler    Database access function (same signature as csm_db_access_handler)
 */
void csm_server_register_db(csm_server *server, csm_db_access_handler handler);

/**
 * \brief Run one iteration of the server event loop
 *
 *  Call this in a loop. It will:
 *  1. Receive the next PDU from the transport
 *  2. Process it through the channel/association layer
 *  3. Send the response back
 *
 * \param server     Server context
 * \param timeout_ms Maximum time to wait for incoming data
 * \return > 0 if a request was processed, 0 on timeout, < 0 on error
 */
int csm_server_poll(csm_server *server, uint32_t timeout_ms);

/**
 * \brief Send an unsolicited response (e.g., EventNotification)
 *
 * \param server     Server context
 * \param channel    Channel index
 * \param apdu       APDU to send
 * \param apdu_len   APDU length
 * \return bytes sent or error code
 */
int csm_server_send(csm_server *server, uint8_t channel,
                    const uint8_t *apdu, uint32_t apdu_len);

/**
 * \brief Destroy server and free resources
 */
void csm_server_destroy(csm_server *server);

/**
 * \brief Allocate and initialize a server
 *
 *  Heap-allocated convenience wrapper around csm_server_init.
 *  Free with csm_server_delete().
 *
 * \param transport  Transport instance
 * \param channel    Primary channel index
 * \param framing    Framing type
 * \return Allocated server, or NULL on failure
 */
csm_server *csm_server_create(csm_transport *transport, uint8_t channel,
                               csm_framing_type framing);

/**
 * \brief Free a server allocated with csm_server_create
 */
void csm_server_delete(csm_server *server);

/* ── High-level client ──────────────────────────────────────────────────── */

typedef struct csm_client csm_client;

/**
 * \brief Initialize a DLMS/COSEM client
 */
int csm_dlms_client_init(csm_client *client, csm_transport *transport,
                    uint8_t channel, csm_framing_type framing);

/**
 * \brief Connect to the server
 */
int csm_client_connect(csm_client *client, uint32_t timeout_ms);

/**
 * \brief Send a GET request and receive the response
 *
 * \param client     Client context
 * \param invoke_id  Invoke ID for the request
 * \param class_id   COSEM class ID
 * \param obis       OBIS code of the target object
 * \param attr_id    Attribute index
 * \param resp_buf   Output buffer for response PDU
 * \param resp_size  Size of output buffer
 * \return > 0 on success (response length), < 0 on error
 */
int csm_client_get(csm_client *client, uint8_t invoke_id,
                   uint16_t class_id, const csm_obis_code *obis,
                   uint8_t attr_id, uint8_t *resp_buf, uint32_t resp_size);

/**
 * \brief Send a SET request and receive the response
 */
int csm_client_set(csm_client *client, uint8_t invoke_id,
                   uint16_t class_id, const csm_obis_code *obis,
                   uint8_t attr_id, const uint8_t *data, uint32_t data_len,
                   uint8_t *resp_buf, uint32_t resp_size);

/**
 * \brief Send an ACTION request and receive the response
 */
int csm_client_action(csm_client *client, uint8_t invoke_id,
                      uint16_t class_id, const csm_obis_code *obis,
                      uint8_t method_id, const uint8_t *data, uint32_t data_len,
                      uint8_t *resp_buf, uint32_t resp_size);

/* ── Block Transfer Client API ──────────────────────────────────────────── */

/**
 * \brief GET with automatic block transfer collection
 *
 *  Sends a GET request and automatically collects all blocks
 *  if the response is a Get-Response-With-DataBlock.
 *  Assembles the complete response into resp_buf.
 *
 * \param client     Client context
 * \param invoke_id  Invoke ID for the request
 * \param class_id   COSEM class ID
 * \param obis       OBIS code of the target object
 * \param attr_id    Attribute index
 * \param resp_buf   Output buffer for assembled response
 * \param resp_size  Size of output buffer
 * \return > 0 on success (total data length), < 0 on error
 */
int csm_client_get_block(csm_client *client, uint8_t invoke_id,
                         uint16_t class_id, const csm_obis_code *obis,
                         uint8_t attr_id, uint8_t *resp_buf, uint32_t resp_size);

/**
 * \brief SET with automatic block transfer
 *
 *  Sends data in blocks if data_len exceeds PDU capacity.
 *  Automatically handles block numbering and acknowledgments.
 *
 * \param client     Client context
 * \param invoke_id  Invoke ID for the request
 * \param class_id   COSEM class ID
 * \param obis       OBIS code of the target object
 * \param attr_id    Attribute index
 * \param data       Data to write
 * \param data_len   Length of data
 * \param resp_buf   Output buffer for final response
 * \param resp_size  Size of output buffer
 * \return > 0 on success, < 0 on error
 */
int csm_client_set_block(csm_client *client, uint8_t invoke_id,
                         uint16_t class_id, const csm_obis_code *obis,
                         uint8_t attr_id, const uint8_t *data, uint32_t data_len,
                         uint8_t *resp_buf, uint32_t resp_size);

/**
 * \brief Disconnect and release the association
 */
int csm_client_disconnect(csm_client *client);

/**
 * \brief Destroy client
 */
void csm_client_destroy(csm_client *client);

/**
 * \brief Allocate and initialize a client
 *
 *  Heap-allocated convenience wrapper around csm_dlms_client_init.
 *  Free with csm_client_delete().
 *
 * \param transport  Transport instance
 * \param channel    Channel index
 * \param framing    Framing type
 * \return Allocated client, or NULL on failure
 */
csm_client *csm_client_create(csm_transport *transport, uint8_t channel,
                               csm_framing_type framing);

/**
 * \brief Configure the client association used by csm_client_connect().
 *
 *  Copies the supplied configuration into association slot 0.
 */
int csm_client_set_association(csm_client *client, const csm_asso_config *config);

/**
 * \brief Free a client allocated with csm_client_create
 */
void csm_client_delete(csm_client *client);

#ifdef __cplusplus
}
#endif

#endif /* CSM_SERVER_H */
