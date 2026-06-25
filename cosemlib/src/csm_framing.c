/**
 * \file csm_framing.c
 * \brief APDU framing implementation for DLMS/COSEM transports
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "csm_framing.h"
#include "csm_transport.h"
#include "hdlc.h"
#include <string.h>

/* ── COSEM-TCP Wrapper (IEC 62056-5-3 §5.1.4) ──────────────────────────── */

static const uint8_t wrapper_cmd_prefix[3] = { 0xE6, 0xE6, 0x00 };
static const uint8_t wrapper_rsp_prefix[3] = { 0xE6, 0xE7, 0x00 };

int csm_wrapper_frame_command(const uint8_t *apdu, uint32_t apdu_len,
                              uint8_t *out, uint32_t out_size)
{
    if (!apdu || !out || apdu_len > CSM_FRAMING_MAX_PDU)
    {
        return CSM_TRANSPORT_ERR;
    }

    uint32_t total = CSM_WRAPPER_CMD_PREFIX_LEN + apdu_len;
    if (total > out_size)
    {
        return CSM_TRANSPORT_ERR_OVERFLOW;
    }

    memcpy(out, wrapper_cmd_prefix, CSM_WRAPPER_CMD_PREFIX_LEN);
    memcpy(out + CSM_WRAPPER_CMD_PREFIX_LEN, apdu, apdu_len);

    return (int)total;
}

int csm_wrapper_frame_response(const uint8_t *apdu, uint32_t apdu_len,
                                uint8_t *out, uint32_t out_size)
{
    if (!apdu || !out || apdu_len > CSM_FRAMING_MAX_PDU)
    {
        return CSM_TRANSPORT_ERR;
    }

    uint32_t total = CSM_WRAPPER_RSP_PREFIX_LEN + apdu_len;
    if (total > out_size)
    {
        return CSM_TRANSPORT_ERR_OVERFLOW;
    }

    memcpy(out, wrapper_rsp_prefix, CSM_WRAPPER_RSP_PREFIX_LEN);
    memcpy(out + CSM_WRAPPER_RSP_PREFIX_LEN, apdu, apdu_len);

    return (int)total;
}

int csm_wrapper_deframe(const uint8_t *data, uint32_t data_len,
                        const uint8_t **apdu, uint32_t *apdu_len)
{
    if (!data || !apdu || !apdu_len || data_len < 3)
    {
        return CSM_TRANSPORT_ERR;
    }

    /* Check LLC prefix: E6 E6 00 (command) or E6 E7 00 (response) */
    if (data[0] != 0xE6 || (data[1] != 0xE6 && data[1] != 0xE7) || data[2] != 0x00)
    {
        return CSM_TRANSPORT_ERR_FRAMING;
    }

    *apdu = data + 3;
    *apdu_len = data_len - 3;

    return CSM_TRANSPORT_OK;
}

/* ── HDLC framing ───────────────────────────────────────────────────────── */

int csm_hdlc_find_frame(const uint8_t *stream, uint32_t stream_len,
                         const uint8_t **frame, uint32_t *frame_len,
                         uint32_t *consumed)
{
    if (!stream || !frame || !frame_len || !consumed)
    {
        return CSM_TRANSPORT_ERR;
    }

    *frame = NULL;
    *frame_len = 0;
    *consumed = 0;

    /* Find opening flag 0x7E */
    uint32_t start = 0;
    while (start < stream_len && stream[start] != CSM_HDLC_FLAG)
    {
        start++;
    }

    if (start >= stream_len)
    {
        return CSM_TRANSPORT_ERR_TIMEOUT;  /* No flag found */
    }

    start++;  /* Skip opening flag */

    /* Find closing flag */
    uint32_t end = start;
    while (end < stream_len && stream[end] != CSM_HDLC_FLAG)
    {
        end++;
    }

    if (end >= stream_len)
    {
        return CSM_TRANSPORT_ERR_TIMEOUT;  /* Incomplete frame */
    }

    /* Frame content is between flags, handle byte stuffing */
    *frame = stream + start;
    *frame_len = end - start;
    *consumed = end + 1;  /* Include closing flag */

    return CSM_TRANSPORT_OK;
}

/* ── Generic framing dispatch ───────────────────────────────────────────── */

int csm_framing_frame(csm_framing_type type, uint8_t direction,
                       const uint8_t *apdu, uint32_t apdu_len,
                       uint8_t *out, uint32_t out_size)
{
    switch (type)
    {
    case CSM_FRAMING_WRAPPER:
        if (direction == 0)
            return csm_wrapper_frame_command(apdu, apdu_len, out, out_size);
        else
            return csm_wrapper_frame_response(apdu, apdu_len, out, out_size);

    case CSM_FRAMING_NONE:
        if (apdu_len > out_size)
            return CSM_TRANSPORT_ERR_OVERFLOW;
        memcpy(out, apdu, apdu_len);
        return (int)apdu_len;

    case CSM_FRAMING_HDLC:
    {
        /* HDLC framing — use hdlc module */
        hdlc_t hdlc;
        hdlc_init(&hdlc);
        hdlc.sender = HDLC_SERVER;
        hdlc.cmd_resp = (direction == 0) ? 0 : 1;

        int result = hdlc_encode(&hdlc, out, (uint16_t)out_size, HDLC_PACKET_TYPE_I,
                                 apdu, (uint16_t)apdu_len);
        if (result == HDLC_OK)
        {
            return (int)hdlc.data_size;
        }
        return CSM_TRANSPORT_ERR;
    }

    default:
        return CSM_TRANSPORT_ERR;
    }
}

int csm_framing_deframe(csm_framing_type type,
                         const uint8_t *data, uint32_t data_len,
                         const uint8_t **apdu, uint32_t *apdu_len)
{
    switch (type)
    {
    case CSM_FRAMING_WRAPPER:
        return csm_wrapper_deframe(data, data_len, apdu, apdu_len);

    case CSM_FRAMING_NONE:
        *apdu = data;
        *apdu_len = data_len;
        return CSM_TRANSPORT_OK;

    case CSM_FRAMING_HDLC:
    {
        /* HDLC deframing — use hdlc module */
        hdlc_t hdlc;
        hdlc_init(&hdlc);

        int result = hdlc_decode(&hdlc, data, (uint16_t)data_len);
        if (result == HDLC_OK)
        {
            *apdu = &data[hdlc.data_index];
            *apdu_len = hdlc.data_size;
            return CSM_TRANSPORT_OK;
        }
        return CSM_TRANSPORT_ERR;
    }

    default:
        return CSM_TRANSPORT_ERR;
    }
}
