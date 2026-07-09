/**
 * OpenDLMS reader — small C API for TCP wrapper client sessions.
 *
 * This header restores the reader API used by examples/reader_lab.
 */

#ifndef OPENDLMS_READER_H
#define OPENDLMS_READER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "csm_association.h"
#include "csm_server.h"
#include "opendlms_reader_auth.h"

#define OPENDLMS_READER_RX_TIMEOUT_MS      5000U
#define OPENDLMS_READER_TCP_LOGICAL_DEVICE 1U

typedef enum {
	OPENDLMS_READER_TRANSPORT_TCP_WRAPPER = 0
} opendlms_reader_transport_t;

typedef struct {
	void *ctx;
	int (*write)(void *ctx, const uint8_t *buf, uint32_t len);
	int (*read)(void *ctx, uint8_t *buf, uint32_t len, uint32_t timeout_ms);
	uint32_t rx_timeout_ms;
} opendlms_reader_io_t;

typedef struct {
	csm_asso_config *associations;
	uint8_t association_count;
} opendlms_reader_t;

typedef struct {
	opendlms_reader_t *reader;
	opendlms_reader_io_t io;
	csm_asso_config association;
	opendlms_reader_auth_t auth;
	uint16_t source_wport;
	uint16_t dest_wport;
	opendlms_reader_transport_t transport_type;
	uint8_t sync_invocation_counter;
	csm_transport transport;
	csm_client *client;
	uint8_t invoke_id;
} opendlms_reader_session_t;

int opendlms_reader_init(opendlms_reader_t *reader, csm_asso_config *associations, uint8_t association_count);
void opendlms_reader_session_init(opendlms_reader_session_t *session, opendlms_reader_t *reader, opendlms_reader_io_t io, const csm_asso_config *association);
void opendlms_reader_session_set_transport(
    opendlms_reader_session_t *session, opendlms_reader_transport_t transport_type, uint16_t source_wport, uint16_t dest_wport
);
void opendlms_reader_session_set_auth(opendlms_reader_session_t *session, const opendlms_reader_auth_t *auth);
void opendlms_reader_session_set_invocation_counter_sync(opendlms_reader_session_t *session, uint8_t enabled);
int opendlms_reader_connect(opendlms_reader_session_t *session);
int opendlms_reader_get(opendlms_reader_session_t *session, uint16_t class_id, const csm_obis_code *obis, uint8_t attr_id, csm_response *response);
int opendlms_reader_set(
    opendlms_reader_session_t *session, uint16_t class_id, const csm_obis_code *obis, uint8_t attr_id, const uint8_t *data, uint32_t data_len,
    csm_response *response
);
int opendlms_reader_action(
    opendlms_reader_session_t *session, uint16_t class_id, const csm_obis_code *obis, uint8_t method_id, const uint8_t *data, uint32_t data_len,
    csm_response *response
);
void opendlms_reader_disconnect(opendlms_reader_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* OPENDLMS_READER_H */
