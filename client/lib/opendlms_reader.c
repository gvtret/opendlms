/**
 * OpenDLMS reader — restored TCP wrapper reader API.
 */

#include "opendlms_reader.h"

#include "csm_array.h"
#include "csm_framing.h"
#include "csm_transport.h"
#include "reader_hal.h"

#include <string.h>

static int reader_transport_open(void *ctx, uint8_t channel)
{
    (void)ctx;
    (void)channel;
    return CSM_TRANSPORT_OK;
}

static int reader_transport_send(void *ctx, uint8_t channel, const uint8_t *data, uint32_t len)
{
    (void)channel;
    opendlms_reader_session_t *session = (opendlms_reader_session_t *)ctx;
    uint8_t framed[CSM_WRAPPER_MAX_LEN];

    if (!session || !session->io.write)
    {
        return CSM_TRANSPORT_ERR;
    }

    int framed_len = csm_tcp_wrapper_frame(session->source_wport, session->dest_wport,
                                           data, len, framed, sizeof(framed));
    if (framed_len < 0)
    {
        return framed_len;
    }

    return session->io.write(session->io.ctx, framed, (uint32_t)framed_len);
}

static int reader_transport_recv(void *ctx, uint8_t channel, uint8_t *buf, uint32_t buf_size,
                                 uint32_t timeout_ms)
{
    (void)channel;
    opendlms_reader_session_t *session = (opendlms_reader_session_t *)ctx;
    uint8_t header[CSM_TCP_WRAPPER_LEN];
    uint8_t frame[CSM_WRAPPER_MAX_LEN];
    const uint8_t *apdu = NULL;
    uint32_t apdu_len = 0U;
    uint16_t source_wport = 0U;
    uint16_t dest_wport = 0U;

    if (!session || !session->io.read || !buf)
    {
        return CSM_TRANSPORT_ERR;
    }

    int n = session->io.read(session->io.ctx, header, sizeof(header), timeout_ms);
    if (n != (int)sizeof(header))
    {
        return (n == 0) ? CSM_TRANSPORT_ERR_TIMEOUT : CSM_TRANSPORT_ERR_IO;
    }

    uint16_t header_apdu_len = (uint16_t)((header[6] << 8U) | header[7]);
    if ((header_apdu_len == 0U) || (header_apdu_len > buf_size) ||
        ((uint32_t)header_apdu_len + CSM_TCP_WRAPPER_LEN > sizeof(frame)))
    {
        return CSM_TRANSPORT_ERR_OVERFLOW;
    }

    memcpy(frame, header, sizeof(header));
    n = session->io.read(session->io.ctx, frame + CSM_TCP_WRAPPER_LEN, header_apdu_len, timeout_ms);
    if (n != (int)header_apdu_len)
    {
        return (n == 0) ? CSM_TRANSPORT_ERR_TIMEOUT : CSM_TRANSPORT_ERR_IO;
    }

    int rc = csm_tcp_wrapper_deframe(frame, (uint32_t)header_apdu_len + CSM_TCP_WRAPPER_LEN,
                                     &apdu, &apdu_len, &source_wport, &dest_wport);
    if (rc != CSM_TRANSPORT_OK)
    {
        return rc;
    }
    if ((source_wport != session->dest_wport) || (dest_wport != session->source_wport))
    {
        return CSM_TRANSPORT_ERR_FRAMING;
    }

    memcpy(buf, apdu, apdu_len);
    return (int)apdu_len;
}

static void reader_transport_close(void *ctx, uint8_t channel)
{
    (void)ctx;
    (void)channel;
}

static int reader_transport_is_connected(void *ctx, uint8_t channel)
{
    (void)ctx;
    (void)channel;
    return 1;
}

static void reader_transport_destroy(void *ctx)
{
    (void)ctx;
}

static const csm_transport_ops reader_transport_ops = {
    reader_transport_open,
    reader_transport_send,
    reader_transport_recv,
    reader_transport_close,
    reader_transport_is_connected,
    reader_transport_destroy
};

int opendlms_reader_init(opendlms_reader_t *reader,
                         csm_asso_config *associations,
                         uint8_t association_count)
{
    if (!reader || !associations || association_count == 0U)
    {
        return -1;
    }

    reader->associations = associations;
    reader->association_count = association_count;
    reader_hal_init();
    return 0;
}

void opendlms_reader_session_init(opendlms_reader_session_t *session,
                                  opendlms_reader_t *reader,
                                  opendlms_reader_io_t io,
                                  const csm_asso_config *association)
{
    if (!session)
    {
        return;
    }

    memset(session, 0, sizeof(*session));
    session->reader = reader;
    session->io = io;
    if (association)
    {
        session->association = *association;
    }
    session->source_wport = session->association.llc.ssap;
    session->dest_wport = session->association.llc.dsap;
    session->transport_type = OPENDLMS_READER_TRANSPORT_TCP_WRAPPER;
    session->invoke_id = 1U;
}

void opendlms_reader_session_set_transport(opendlms_reader_session_t *session,
                                           opendlms_reader_transport_t transport_type,
                                           uint16_t source_wport,
                                           uint16_t dest_wport)
{
    if (!session)
    {
        return;
    }

    session->transport_type = transport_type;
    session->source_wport = source_wport;
    session->dest_wport = dest_wport;
}

void opendlms_reader_session_set_auth(opendlms_reader_session_t *session,
                                      const opendlms_reader_auth_t *auth)
{
    if (!session || !auth)
    {
        return;
    }

    session->auth = *auth;
    session->association.application_context = auth->application_context;
    session->association.authentication = auth->auth_level;
}

void opendlms_reader_session_set_invocation_counter_sync(opendlms_reader_session_t *session,
                                                         uint8_t enabled)
{
    if (session)
    {
        session->sync_invocation_counter = enabled ? 1U : 0U;
    }
}

int opendlms_reader_connect(opendlms_reader_session_t *session)
{
    if (!session || session->transport_type != OPENDLMS_READER_TRANSPORT_TCP_WRAPPER)
    {
        return -1;
    }
    if (session->sync_invocation_counter != 0U)
    {
        return -1;
    }

    session->transport.ops = &reader_transport_ops;
    session->transport.ctx = session;
    session->client = csm_client_create(&session->transport, 0, CSM_FRAMING_NONE);
    if (!session->client)
    {
        return -1;
    }

    if (csm_client_set_association(session->client, &session->association) != 0)
    {
        opendlms_reader_disconnect(session);
        return -1;
    }

    if (csm_client_connect(session->client, session->io.rx_timeout_ms) != 0)
    {
        opendlms_reader_disconnect(session);
        return -1;
    }

    session->invoke_id = 1U;
    return 0;
}

int opendlms_reader_get(opendlms_reader_session_t *session,
                        uint16_t class_id,
                        const csm_obis_code *obis,
                        uint8_t attr_id,
                        csm_response *response)
{
    uint8_t resp_buf[CSM_SERVER_MAX_PDU];

    if (!session || !session->client || !obis || !response)
    {
        return -1;
    }

    int rc = csm_client_get_block(session->client, session->invoke_id++, class_id, obis,
                                  attr_id, resp_buf, sizeof(resp_buf));
    if (rc <= 0)
    {
        return -1;
    }

    csm_client_init(NULL, response);
    csm_array array;
    csm_array_init(&array, resp_buf, sizeof(resp_buf), (uint32_t)rc, 0);
    return csm_client_decode(response, &array) ? 0 : -1;
}

int opendlms_reader_set(opendlms_reader_session_t *session,
                        uint16_t class_id,
                        const csm_obis_code *obis,
                        uint8_t attr_id,
                        const uint8_t *data,
                        uint32_t data_len,
                        csm_response *response)
{
    uint8_t resp_buf[CSM_SERVER_MAX_PDU];

    if (!session || !session->client || !obis || !data || data_len == 0U || !response)
    {
        return -1;
    }

    int rc = csm_client_set(session->client, session->invoke_id++, class_id, obis,
                            attr_id, data, data_len, resp_buf, sizeof(resp_buf));
    if (rc <= 0)
    {
        return -1;
    }

    csm_client_init(NULL, response);
    csm_array array;
    csm_array_init(&array, resp_buf, sizeof(resp_buf), (uint32_t)rc, 0);
    return csm_client_decode(response, &array) ? 0 : -1;
}

int opendlms_reader_action(opendlms_reader_session_t *session,
                           uint16_t class_id,
                           const csm_obis_code *obis,
                           uint8_t method_id,
                           const uint8_t *data,
                           uint32_t data_len,
                           csm_response *response)
{
    uint8_t resp_buf[CSM_SERVER_MAX_PDU];

    if (!session || !session->client || !obis || ((data == NULL) && (data_len != 0U)) || !response)
    {
        return -1;
    }

    int rc = csm_client_action(session->client, session->invoke_id++, class_id, obis,
                               method_id, data, data_len, resp_buf, sizeof(resp_buf));
    if (rc <= 0)
    {
        return -1;
    }

    csm_client_init(NULL, response);
    csm_array array;
    csm_array_init(&array, resp_buf, sizeof(resp_buf), (uint32_t)rc, 0);
    return csm_client_decode(response, &array) ? 0 : -1;
}

void opendlms_reader_disconnect(opendlms_reader_session_t *session)
{
    if (!session)
    {
        return;
    }

    if (session->client)
    {
        csm_client_disconnect(session->client);
        csm_client_delete(session->client);
        session->client = NULL;
    }
    session->transport.ops = NULL;
    session->transport.ctx = NULL;
}
