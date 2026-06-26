/**
 * \file csm_server.c
 * \brief High-level DLMS/COSEM server and client implementation
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "csm_server.h"
#include "csm_channel.h"
#include "csm_services.h"
#include "csm_definitions.h"
#include "csm_association.h"
#include <string.h>
#include <stdlib.h>

/* ── Server internals ───────────────────────────────────────────────────── */

struct csm_server {
    csm_transport     *transport;
    csm_framing_type   framing;
    uint8_t            channel;
    csm_channel_ctx    chan_ctx;
    csm_channel        channels[CSM_SERVER_MAX_CHANNELS];
    csm_asso_state     asso_states[CSM_SERVER_MAX_CHANNELS];
    csm_asso_config    asso_configs[CSM_SERVER_MAX_CHANNELS];
    csm_db_context_t   db_ctx;
    uint8_t            rx_buf[CSM_SERVER_MAX_PDU];
    uint8_t            tx_buf[CSM_SERVER_MAX_PDU];
};

int csm_server_init(csm_server *server, csm_transport *transport,
                    uint8_t channel, csm_framing_type framing)
{
    if (!server || !transport) return -1;

    memset(server, 0, sizeof(*server));
    server->transport = transport;
    server->framing = framing;
    server->channel = channel;

    /* Initialize default association configs */
    for (uint8_t i = 0; i < CSM_SERVER_MAX_CHANNELS; i++)
    {
        server->asso_configs[i].llc.ssap = 0x00;
        server->asso_configs[i].llc.dsap = 0x01;
        server->asso_configs[i].conformance = 0xFFFFFFFFU;
        server->asso_configs[i].is_auto_connected = 0;
    }

    csm_channel_ctx_init(&server->chan_ctx, server->channels, CSM_SERVER_MAX_CHANNELS,
                         server->asso_states, server->asso_configs, CSM_SERVER_MAX_CHANNELS);

    memset(&server->db_ctx, 0, sizeof(server->db_ctx));

    return 0;
}

void csm_server_register_db(csm_server *server, csm_db_access_handler handler)
{
    if (server)
    {
        csm_channel_ctx_set_db(&server->chan_ctx, handler);
    }
}

int csm_server_poll(csm_server *server, uint32_t timeout_ms)
{
    if (!server || !server->transport) return -1;

    uint8_t ch = server->channel;

    /* Receive PDU from transport */
    int recv_len = CSM_TRANSPORT_RECV(server->transport, ch,
                                       server->rx_buf, sizeof(server->rx_buf),
                                       timeout_ms);

    if (recv_len <= 0) return recv_len;  /* Timeout or error */

    /* Deframe if needed */
    const uint8_t *apdu;
    uint32_t apdu_len;
    int rc = csm_framing_deframe(server->framing, server->rx_buf, recv_len,
                                  &apdu, &apdu_len);
    if (rc != 0) return -1;

    /* Build a csm_array pointing to the PDU (will be overwritten with response) */
    csm_array pkt;
    csm_array_init(&pkt, server->tx_buf, sizeof(server->tx_buf), 0, 0);

    /* Copy APDU into tx_buf for processing (csm_channel_execute reuses the buffer) */
    if (apdu_len > sizeof(server->tx_buf)) return -1;
    memcpy(server->tx_buf, apdu, apdu_len);

    /* Process through channel/association layer */
    int resp_len = csm_channel_execute_ctx(&server->chan_ctx, &server->db_ctx, ch, &pkt);

    if (resp_len > 0)
    {
        /* Frame the response */
        uint8_t framed[CSM_WRAPPER_MAX_LEN];
        int framed_len = csm_framing_frame(server->framing, 1,
                                            server->tx_buf, resp_len,
                                            framed, sizeof(framed));

        if (framed_len > 0)
        {
            /* Send response back */
            CSM_TRANSPORT_SEND(server->transport, ch, framed, framed_len);
        }
    }

    return resp_len;
}

int csm_server_send(csm_server *server, uint8_t channel,
                    const uint8_t *apdu, uint32_t apdu_len)
{
    if (!server || !server->transport) return -1;

    uint8_t framed[CSM_WRAPPER_MAX_LEN];
    int framed_len = csm_framing_frame(server->framing, 1,
                                        apdu, apdu_len,
                                        framed, sizeof(framed));
    if (framed_len < 0) return framed_len;

    return CSM_TRANSPORT_SEND(server->transport, channel, framed, framed_len);
}

void csm_server_destroy(csm_server *server)
{
    if (!server) return;
    memset(server, 0, sizeof(*server));
}

csm_server *csm_server_create(csm_transport *transport, uint8_t channel,
                               csm_framing_type framing)
{
    csm_server *server = (csm_server *)malloc(sizeof(csm_server));
    if (!server) return NULL;
    if (csm_server_init(server, transport, channel, framing) != 0)
    {
        free(server);
        return NULL;
    }
    return server;
}

void csm_server_delete(csm_server *server)
{
    if (!server) return;
    csm_server_destroy(server);
    free(server);
}

/* ── Client internals ───────────────────────────────────────────────────── */

struct csm_client {
    csm_transport     *transport;
    csm_framing_type   framing;
    uint8_t            channel;
    csm_channel_ctx    chan_ctx;
    csm_channel        channels[CSM_SERVER_MAX_CHANNELS];
    csm_asso_state     asso_states[CSM_SERVER_MAX_CHANNELS];
    csm_asso_config    asso_configs[CSM_SERVER_MAX_CHANNELS];
    csm_db_context_t   db_ctx;
    uint8_t            rx_buf[CSM_SERVER_MAX_PDU];
    uint8_t            tx_buf[CSM_SERVER_MAX_PDU];
};

static int client_send_recv(csm_client *client, uint8_t *apdu, uint32_t apdu_len,
                            uint8_t *resp_buf, uint32_t resp_size);

int csm_dlms_client_init(csm_client *client, csm_transport *transport,
                    uint8_t channel, csm_framing_type framing)
{
    if (!client || !transport) return -1;

    memset(client, 0, sizeof(*client));
    client->transport = transport;
    client->framing = framing;
    client->channel = channel;

    for (uint8_t i = 0; i < CSM_SERVER_MAX_CHANNELS; i++)
    {
        client->asso_configs[i].llc.ssap = 0x01;
        client->asso_configs[i].llc.dsap = 0x00;
        client->asso_configs[i].conformance = 0xFFFFFFFFU;
        client->asso_configs[i].is_auto_connected = 0;
        client->asso_configs[i].application_context = (uint8_t)LN_REF;
        client->asso_configs[i].authentication = (uint8_t)CSM_AUTH_LOWEST_LEVEL;
    }

    csm_channel_ctx_init(&client->chan_ctx, client->channels, CSM_SERVER_MAX_CHANNELS,
                         client->asso_states, client->asso_configs, CSM_SERVER_MAX_CHANNELS);

    memset(&client->db_ctx, 0, sizeof(client->db_ctx));

    return 0;
}

int csm_client_connect(csm_client *client, uint32_t timeout_ms)
{
    if (!client || !client->transport) return -1;

    int rc = CSM_TRANSPORT_OPEN(client->transport, client->channel);
    if (rc != CSM_TRANSPORT_OK) return rc;

    csm_asso_state *asso = &client->asso_states[0];
    asso->config = &client->asso_configs[0];
    asso->ref = (client->asso_configs[0].application_context != 0U)
        ? (enum csm_referencing)client->asso_configs[0].application_context
        : LN_REF;
    asso->auth_level = (enum csm_auth_level)client->asso_configs[0].authentication;

    csm_array req;
    csm_array_init(&req, client->tx_buf, sizeof(client->tx_buf), 0, 0);
    if (!csm_asso_encoder(asso, &req, CSM_ASSO_AARQ)) return -1;

    int resp_len = client_send_recv(client, client->tx_buf, req.wr_index,
                                    client->rx_buf, sizeof(client->rx_buf));
    if (resp_len <= 0) return resp_len;

    csm_array resp;
    csm_array_init(&resp, client->rx_buf, sizeof(client->rx_buf), (uint32_t)resp_len, 0);
    if (!csm_asso_decoder(asso, &resp, CSM_ASSO_AARE)) return -1;
    if (!asso->handshake.accepted) return -1;

    asso->state_cf = CF_ASSOCIATED;
    (void)timeout_ms;
    return CSM_TRANSPORT_OK;
}

static int client_send_recv(csm_client *client, uint8_t *apdu, uint32_t apdu_len,
                            uint8_t *resp_buf, uint32_t resp_size)
{
    uint8_t ch = client->channel;

    /* Frame and send */
    uint8_t framed[CSM_WRAPPER_MAX_LEN];
    int framed_len = csm_framing_frame(client->framing, 0,
                                        apdu, apdu_len,
                                        framed, sizeof(framed));
    if (framed_len < 0) return framed_len;

    int sent = CSM_TRANSPORT_SEND(client->transport, ch, framed, framed_len);
    if (sent < 0) return sent;

    /* Receive response */
    uint8_t rx_framed[CSM_WRAPPER_MAX_LEN];
    int recv_len = CSM_TRANSPORT_RECV(client->transport, ch,
                                       rx_framed, sizeof(rx_framed), 5000);
    if (recv_len <= 0) return recv_len;

    /* Deframe */
    const uint8_t *resp_apdu;
    uint32_t resp_apdu_len;
    int rc = csm_framing_deframe(client->framing, rx_framed, recv_len,
                                  &resp_apdu, &resp_apdu_len);
    if (rc != 0) return -1;

    if (resp_apdu_len > resp_size) return -1;
    memcpy(resp_buf, resp_apdu, resp_apdu_len);
    return (int)resp_apdu_len;
}

int csm_client_get(csm_client *client, uint8_t invoke_id,
                   uint16_t class_id, const csm_obis_code *obis,
                   uint8_t attr_id, uint8_t *resp_buf, uint32_t resp_size)
{
    if (!client) return -1;

    /* Build GET request */
    csm_array req;
    csm_array_init(&req, client->tx_buf, sizeof(client->tx_buf), 0, 0);

    csm_array_write_u8(&req, 0xC0);  /* GET-request */
    csm_array_write_u8(&req, 0x01);  /* type: normal */
    csm_array_write_u8(&req, invoke_id);
    csm_array_write_u16(&req, class_id);
    csm_array_write_u8(&req, obis->A);
    csm_array_write_u8(&req, obis->B);
    csm_array_write_u8(&req, obis->C);
    csm_array_write_u8(&req, obis->D);
    csm_array_write_u8(&req, obis->E);
    csm_array_write_u8(&req, obis->F);
    csm_array_write_u8(&req, attr_id);
    csm_array_write_u8(&req, 0x00);  /* No selective access */

    return client_send_recv(client, client->tx_buf, req.wr_index,
                            resp_buf, resp_size);
}

int csm_client_set(csm_client *client, uint8_t invoke_id,
                   uint16_t class_id, const csm_obis_code *obis,
                   uint8_t attr_id, const uint8_t *data, uint32_t data_len,
                   uint8_t *resp_buf, uint32_t resp_size)
{
    if (!client) return -1;

    csm_array req;
    csm_array_init(&req, client->tx_buf, sizeof(client->tx_buf), 0, 0);

    csm_array_write_u8(&req, 0xC1);  /* SET-request */
    csm_array_write_u8(&req, 0x01);  /* type: normal */
    csm_array_write_u8(&req, invoke_id);
    csm_array_write_u16(&req, class_id);
    csm_array_write_u8(&req, obis->A);
    csm_array_write_u8(&req, obis->B);
    csm_array_write_u8(&req, obis->C);
    csm_array_write_u8(&req, obis->D);
    csm_array_write_u8(&req, obis->E);
    csm_array_write_u8(&req, obis->F);
    csm_array_write_u8(&req, attr_id);
    csm_array_write_u8(&req, 0x00);  /* No selective access */
    csm_array_write_buff(&req, data, data_len);

    return client_send_recv(client, client->tx_buf, req.wr_index,
                            resp_buf, resp_size);
}

int csm_client_action(csm_client *client, uint8_t invoke_id,
                      uint16_t class_id, const csm_obis_code *obis,
                      uint8_t method_id, const uint8_t *data, uint32_t data_len,
                      uint8_t *resp_buf, uint32_t resp_size)
{
    if (!client) return -1;

    csm_array req;
    csm_array_init(&req, client->tx_buf, sizeof(client->tx_buf), 0, 0);

    csm_array_write_u8(&req, 0xC3);  /* ACTION-request */
    csm_array_write_u8(&req, 0x01);  /* type: normal */
    csm_array_write_u8(&req, invoke_id);
    csm_array_write_u16(&req, class_id);
    csm_array_write_u8(&req, obis->A);
    csm_array_write_u8(&req, obis->B);
    csm_array_write_u8(&req, obis->C);
    csm_array_write_u8(&req, obis->D);
    csm_array_write_u8(&req, obis->E);
    csm_array_write_u8(&req, obis->F);
    csm_array_write_u8(&req, method_id);
    if (data && data_len > 0)
    {
        csm_array_write_buff(&req, data, data_len);
    }

    return client_send_recv(client, client->tx_buf, req.wr_index,
                            resp_buf, resp_size);
}

int csm_client_disconnect(csm_client *client)
{
    if (!client || !client->transport) return -1;
    CSM_TRANSPORT_CLOSE(client->transport, client->channel);
    return 0;
}

void csm_client_destroy(csm_client *client)
{
    if (!client) return;
    memset(client, 0, sizeof(*client));
}

csm_client *csm_client_create(csm_transport *transport, uint8_t channel,
                               csm_framing_type framing)
{
    csm_client *client = (csm_client *)malloc(sizeof(csm_client));
    if (!client) return NULL;
    if (csm_dlms_client_init(client, transport, channel, framing) != 0)
    {
        free(client);
        return NULL;
    }
    return client;
}

int csm_client_set_association(csm_client *client, const csm_asso_config *config)
{
    if (!client || !config) return -1;
    client->asso_configs[0] = *config;
    if (client->asso_configs[0].application_context == 0U)
    {
        client->asso_configs[0].application_context = (uint8_t)LN_REF;
    }
    return 0;
}

void csm_client_delete(csm_client *client)
{
    if (!client) return;
    csm_client_destroy(client);
    free(client);
}

/* ── Block Transfer Client API ──────────────────────────────────────────── */

#include "csm_block_transfer.h"

/* Block transfer limits */
#define CSM_CLIENT_MAX_BLOCK_SIZE   512U
#define CSM_CLIENT_MAX_PDU          CSM_SERVER_MAX_PDU

/*
 * Decode response type to check if it's a block response.
 * Returns:
 *   0 = normal response (data follows)
 *   1 = block response (need to collect more blocks)
 *  -1 = error
 */
static int client_decode_response_type(const uint8_t *resp, uint32_t resp_len,
                                       uint8_t *invoke_id, uint8_t *last_block,
                                       uint32_t *block_number)
{
    if (resp_len < 8U) return -1;

    uint8_t tag = resp[0];
    uint8_t type = resp[1];

    /* Check for GET/SET/ACTION response with block */
    if ((tag == 0xC4U) || (tag == 0xC5U) || (tag == 0xC7U))
    {
        if (type == 0x04U) /* With DataBlock */
        {
            *invoke_id = resp[2];
            *last_block = resp[3];
            *block_number = ((uint32_t)resp[4] << 24) |
                           ((uint32_t)resp[5] << 16) |
                           ((uint32_t)resp[6] << 8) |
                           (uint32_t)resp[7];
            return 1; /* Block response */
        }
        else if (type == 0x01U) /* Normal response */
        {
            return 0; /* Normal response */
        }
    }

    return -1; /* Unknown/error */
}

int csm_client_get_block(csm_client *client, uint8_t invoke_id,
                         uint16_t class_id, const csm_obis_code *obis,
                         uint8_t attr_id, uint8_t *resp_buf, uint32_t resp_size)
{
    if (!client || !resp_buf || resp_size == 0U) return -1;

    /* Build and send initial GET request */
    int resp_len = csm_client_get(client, invoke_id, class_id, obis, attr_id,
                                  resp_buf, resp_size);
    if (resp_len <= 0) return resp_len;

    /* Check if response is a block response */
    uint8_t resp_invoke_id;
    uint8_t last_block;
    uint32_t block_number;
    int type = client_decode_response_type(resp_buf, (uint32_t)resp_len,
                                           &resp_invoke_id, &last_block, &block_number);

    if (type != 1)
    {
        /* Normal response - return as-is (skip first 6 bytes: tag+type+invoke_id+result) */
        return resp_len;
    }

    /* Block response - collect all blocks */
    uint32_t total_offset = 0U;

    /* Copy data from first block (skip 8-byte header) */
    if ((uint32_t)resp_len > 8U)
    {
        uint32_t data_len = (uint32_t)resp_len - 8U;
        if (data_len > resp_size) return -1;
        memmove(resp_buf, resp_buf + 8U, data_len);
        total_offset = data_len;
    }

    /* Collect remaining blocks */
    while (!last_block)
    {
        /* Build GET-Request-Next */
        csm_array req;
        csm_array_init(&req, client->tx_buf, sizeof(client->tx_buf), 0, 0);

        csm_array_write_u8(&req, 0xC0U);  /* GET-request */
        csm_array_write_u8(&req, 0x02U);  /* type: next */
        csm_array_write_u8(&req, invoke_id);
        csm_array_write_u32(&req, block_number);

        /* Send and receive */
        uint8_t rx_buf[CSM_CLIENT_MAX_PDU];
        int rx_len = client_send_recv(client, client->tx_buf, req.wr_index,
                                      rx_buf, sizeof(rx_buf));
        if (rx_len <= 0) return rx_len;

        /* Decode block response */
        type = client_decode_response_type(rx_buf, (uint32_t)rx_len,
                                           &resp_invoke_id, &last_block, &block_number);
        if (type != 1) return -1; /* Expected block response */

        /* Copy data from this block (skip 8-byte header) */
        if ((uint32_t)rx_len > 8U)
        {
            uint32_t data_len = (uint32_t)rx_len - 8U;
            if ((total_offset + data_len) > resp_size) return -1;
            memcpy(resp_buf + total_offset, rx_buf + 8U, data_len);
            total_offset += data_len;
        }
    }

    return (int)total_offset;
}

int csm_client_set_block(csm_client *client, uint8_t invoke_id,
                         uint16_t class_id, const csm_obis_code *obis,
                         uint8_t attr_id, const uint8_t *data, uint32_t data_len,
                         uint8_t *resp_buf, uint32_t resp_size)
{
    if (!client) return -1;

    /* Calculate overhead: tag(1) + type(1) + invoke_id(1) + last_block(1) +
     * block_number(4) + class_id(2) + obis(6) + id(1) + sel_access(1) = 18 bytes */
    const uint32_t header_overhead = 18U;

    /* Check if data fits in a single block */
    if ((header_overhead + data_len) <= CSM_CLIENT_MAX_BLOCK_SIZE)
    {
        /* Single block - use normal SET */
        return csm_client_set(client, invoke_id, class_id, obis, attr_id,
                              data, data_len, resp_buf, resp_size);
    }

    /* Multi-block transfer needed */
    csm_block_state block_state;
    csm_block_init(&block_state);

    uint32_t offset = 0U;
    uint32_t block_number = 0U;
    uint8_t last_block = 0U;

    while (!last_block)
    {
        /* Calculate chunk size */
        uint32_t remaining = data_len - offset;
        uint32_t max_chunk = CSM_CLIENT_MAX_BLOCK_SIZE - header_overhead;
        uint32_t chunk = (remaining < max_chunk) ? remaining : max_chunk;
        last_block = ((offset + chunk) >= data_len) ? 1U : 0U;

        /* Build SET-Request-With-DataBlock */
        csm_array req;
        csm_array_init(&req, client->tx_buf, sizeof(client->tx_buf), 0, 0);

        csm_array_write_u8(&req, 0xC1U);  /* SET-request */
        csm_array_write_u8(&req, 0x02U);  /* type: with block */
        csm_array_write_u8(&req, invoke_id);
        csm_array_write_u8(&req, last_block);
        csm_array_write_u32(&req, block_number);

        /* Object identification (first block only) */
        if (block_number == 0U)
        {
            csm_array_write_u16(&req, class_id);
            csm_array_write_u8(&req, obis->A);
            csm_array_write_u8(&req, obis->B);
            csm_array_write_u8(&req, obis->C);
            csm_array_write_u8(&req, obis->D);
            csm_array_write_u8(&req, obis->E);
            csm_array_write_u8(&req, obis->F);
            csm_array_write_u8(&req, attr_id);
            csm_array_write_u8(&req, 0x00U);  /* No selective access */
        }

        /* Data chunk */
        csm_array_write_u8(&req, 0x09U);  /* Octet-string tag */
        csm_array_write_u8(&req, (uint8_t)chunk);
        csm_array_write_buff(&req, data + offset, chunk);

        /* Send and receive acknowledgment */
        uint8_t rx_buf[CSM_CLIENT_MAX_PDU];
        int rx_len = client_send_recv(client, client->tx_buf, req.wr_index,
                                      rx_buf, sizeof(rx_buf));
        if (rx_len <= 0) return rx_len;

        /* Verify response is SET-Response-With-DataBlock */
        if ((rx_len >= 9U) && (rx_buf[0] == 0xC5U) && (rx_buf[1] == 0x02U))
        {
            /* Check access result */
            if (rx_buf[8] != 0x00U)
            {
                return -1; /* Access error */
            }
        }
        else
        {
            return -1; /* Unexpected response */
        }

        offset += chunk;
        block_number++;
    }

    /* Copy final response to output buffer */
    if (resp_buf && resp_size > 0U)
    {
        /* Build a normal response for the caller */
        resp_buf[0] = 0xC5U;  /* SET-response */
        resp_buf[1] = 0x01U;  /* Normal response */
        resp_buf[2] = invoke_id;
        resp_buf[3] = 0x00U;  /* Success */
        return 4;
    }

    return 0;
}
