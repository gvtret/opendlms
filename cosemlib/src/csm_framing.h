/**
 * \file csm_framing.h
 * \brief APDU framing for DLMS/COSEM transports
 *
 *  Provides framing/unframing for:
 *    - COSEM LLC wrapper: LLC prefix E6 E6 00/E6 E7 00
 *    - COSEM-TCP WPDU (IEC 62056-5-3): version/source/destination/length
 *    - HDLC (IEC 62056-46): flag, address, control, info, FCS
 *    - Raw (no framing, for testing)
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef CSM_FRAMING_H
#define CSM_FRAMING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ── Max PDU sizes ──────────────────────────────────────────────────────── */

#define CSM_FRAMING_MAX_PDU 2048

/* ── COSEM-TCP wrapper (IEC 62056-5-3 §5.1.4) ──────────────────────────── */

#define CSM_WRAPPER_CMD_PREFIX_LEN 3 /* E6 E6 00 */
#define CSM_WRAPPER_RSP_PREFIX_LEN 3 /* E6 E7 00 */
#define CSM_TCP_WRAPPER_LEN        8
#define CSM_WRAPPER_MAX_LEN        (CSM_TCP_WRAPPER_LEN + CSM_FRAMING_MAX_PDU)

/**
 * \brief Frame an APDU with COSEM-TCP wrapper (command)
 *
 *  Output: E6 E6 00 || apdu
 *
 * \param apdu       Raw APDU data
 * \param apdu_len   APDU length
 * \param out        Output buffer (must be >= apdu_len + 3)
 * \param out_size   Output buffer size
 * \return Total framed length, or negative error code
 */
int csm_wrapper_frame_command(const uint8_t *apdu, uint32_t apdu_len, uint8_t *out, uint32_t out_size);

/**
 * \brief Frame an APDU with COSEM-TCP wrapper (response)
 *
 *  Output: E6 E7 00 || apdu
 */
int csm_wrapper_frame_response(const uint8_t *apdu, uint32_t apdu_len, uint8_t *out, uint32_t out_size);

/**
 * \brief Deframe a COSEM-TCP wrapped PDU
 *
 *  Validates LLC prefix and extracts APDU.
 *
 * \param data       Input data (with LLC prefix)
 * \param data_len   Input data length
 * \param apdu       Output: pointer to APDU (within input buffer, no copy)
 * \param apdu_len   Output: APDU length
 * \return CSM_TRANSPORT_OK on success, error code on failure
 */
int csm_wrapper_deframe(const uint8_t *data, uint32_t data_len, const uint8_t **apdu, uint32_t *apdu_len);

int csm_tcp_wrapper_frame(uint16_t source_wport, uint16_t dest_wport, const uint8_t *apdu, uint32_t apdu_len, uint8_t *out, uint32_t out_size);
int csm_tcp_wrapper_deframe(const uint8_t *data, uint32_t data_len, const uint8_t **apdu, uint32_t *apdu_len, uint16_t *source_wport, uint16_t *dest_wport);

/* ── LLC SAP constants ──────────────────────────────────────────────────── */

#define CSM_LLC_SAP_CMD     0xE6 /* Server SAP for commands */
#define CSM_LLC_SAP_RSP     0xE6 /* Server SAP for responses */
#define CSM_LLC_SAP_CMD_SUB 0x00 /* Sub-layer for commands */
#define CSM_LLC_SAP_RSP_SUB 0x00 /* Sub-layer for responses */

/* ── HDLC framing (IEC 62056-46) ────────────────────────────────────────── */

#define CSM_HDLC_FLAG 0x7E
#define CSM_HDLC_ESC  0x7D

/**
 * \brief Find the next HDLC frame in a byte stream
 *
 *  Scans for 0x7E flag, extracts frame, handles byte stuffing.
 *
 * \param stream     Input byte stream (may contain multiple frames)
 * \param stream_len Stream length
 * \param frame      Output: pointer to frame content (between flags, no 0x7E)
 * \param frame_len  Output: frame content length
 * \param consumed   Output: number of bytes consumed from stream
 * \return CSM_TRANSPORT_OK if frame found, CSM_TRANSPORT_ERR_TIMEOUT if no complete frame
 */
int csm_hdlc_find_frame(const uint8_t *stream, uint32_t stream_len, const uint8_t **frame, uint32_t *frame_len, uint32_t *consumed);

/* ── Generic framing interface ──────────────────────────────────────────── */

typedef enum {
	CSM_FRAMING_NONE = 0,    /*!< No framing (raw) */
	CSM_FRAMING_WRAPPER,     /*!< COSEM LLC wrapper (E6 E6/E7 00) */
	CSM_FRAMING_TCP_WRAPPER, /*!< COSEM-TCP WPDU wrapper */
	CSM_FRAMING_HDLC         /*!< HDLC framing */
} csm_framing_type;

/**
 * \brief Frame an APDU according to the framing type
 */
int csm_framing_frame(csm_framing_type type, uint8_t direction, const uint8_t *apdu, uint32_t apdu_len, uint8_t *out, uint32_t out_size);

/**
 * \brief Deframe a PDU according to the framing type
 */
int csm_framing_deframe(csm_framing_type type, const uint8_t *data, uint32_t data_len, const uint8_t **apdu, uint32_t *apdu_len);

#ifdef __cplusplus
}
#endif

#endif /* CSM_FRAMING_H */
