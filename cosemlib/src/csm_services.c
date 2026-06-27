/**
 * Cosem services coder/decoder
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_services.h"
#include "csm_axdr_codec.h"
#include "csm_block_transfer.h"
#include <string.h>

static csm_db_access_handler database = NULL;

/* ── Block transfer constants ───────────────────────────────────────────── */

#define SVC_GET_RESPONSE_WITH_BLOCK    0x04U
#define SVC_MAX_BLOCK_DATA_SIZE        1024U  ///< Max data to buffer for block transfer

int svc_exception_response_encoder_ex(csm_array *array, uint8_t state_err, uint8_t service_err)
{
    int valid = csm_array_write_u8(array, AXDR_EXCEPTION_RESPONSE);
    valid = valid && csm_array_write_u8(array, state_err);
    valid = valid && csm_array_write_u8(array, service_err);
    return valid;
}

int svc_exception_response_encoder(csm_array *array)
{
    return svc_exception_response_encoder_ex(array, 1U, 1U);
}


enum data_access_result
{
    SRV_RESULT_SUCCESS              = 0U,
    SRV_RESULT_HARDWARE_FAULT       = 1U,
    SRV_RESULT_TEMPORARY_FAILURE    = 2U,
    SRV_RESULT_READ_WRITE_DENIED    = 3U,
    SRV_RESULT_OBJECT_UNDEFINED     = 4U,
    SRV_RESULT_OTHER_REASON         = 250U
};

int svc_data_access_result_encoder(csm_array *array, csm_db_code code)
{
    uint8_t result;

    switch (code)
    {
    case CSM_OK:
        result = SRV_RESULT_SUCCESS;
        break;
    case CSM_ERR_TEMPORARY_FAILURE:
        result = SRV_RESULT_TEMPORARY_FAILURE;
        break;
    case CSM_ERR_UNAUTHORIZED_ACCESS:
        result = SRV_RESULT_READ_WRITE_DENIED;
        break;
    case CSM_ERR_OBJECT_NOT_FOUND:
        result = SRV_RESULT_OBJECT_UNDEFINED;
        break;
    default:
        result = SRV_RESULT_OTHER_REASON;
        break;
    }

    return csm_array_write_u8(array, result);
}


int svc_is_normal_request(uint8_t type)
{
    int istype = FALSE;

    // Same type for SET/GET/ACTION
    if (type == 1U)
    {
        istype = TRUE;
    }

    return istype;
}

int svc_is_next_request(uint8_t type, enum csm_service service)
{
    int istype = FALSE;

    // Same type for SET/GET/ACTION
    if ((type == 2U) && (service != SVC_ACTION))
    {
        istype = TRUE;
    }

    return istype;
}


int svc_decode_request(csm_request *request, csm_array *array)
{
    uint8_t type = 0U;
    enum csm_service service = request->db_request.service;
    memset(&request->db_request, 0, sizeof(request->db_request));
    request->db_request.service = service;

    int valid = csm_array_read_u8(array, &type);
    valid = valid && csm_array_read_u8(array, &request->sender_invoke_id); // save the invoke ID to reuse the same

    if (valid)
    {
        if (svc_is_normal_request(type))
        {
            request->type = SVC_REQUEST_NORMAL;
            valid = valid && csm_array_read_u16(array, &request->db_request.logical_name.class_id);
            valid = valid && csm_array_read_buff(array, &request->db_request.logical_name.obis.A, 6U);
            valid = valid && csm_array_read_u8(array, (uint8_t*)&request->db_request.logical_name.id);

            if (request->db_request.service != SVC_ACTION)
            {
                // GET and SET services can have selective access parameter (option)
                valid = valid && csm_array_read_u8(array, &request->db_request.sel_access.enable);

                if (request->db_request.sel_access.enable)
                {
                    // Retrieve selective access data, user side decoding
                    valid = valid && csm_hal_decode_selective_access(request, array);
                }
            }

            if (request->db_request.service != SVC_GET)
            {
                // SET and ACTION services can have data in the request
                valid = valid && csm_array_read_u8(array, &request->db_request.additional_data.enable);
                request->db_request.additional_data.data = *array;
                request->db_request.additional_data.data.offset += array->rd_index;
                request->db_request.additional_data.data.wr_index = array->wr_index - array->rd_index;
                request->db_request.additional_data.data.rd_index = 0U;
            }
        }
        else if (svc_is_next_request(type, request->db_request.service))
        {
            request->type = SVC_REQUEST_NEXT;
            valid = valid && csm_array_read_u32(array, &request->db_request.block_number); // save the invoke ID to reuse the same
        }
    }

    return valid;
}

static csm_db_code svc_get_request_decoder(csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    csm_db_code code = CSM_ERR_BAD_ENCODING;

    CSM_LOG("[SVC] Decoding GET.request");

    request->db_request.service = SVC_GET;

    if (svc_decode_request(request, array))
    {
        /* Handle Get-Request-Next (block transfer continuation) */
        if (request->type == SVC_REQUEST_NEXT)
        {
            if (csm_block_is_active(&state->block_transfer) &&
                (state->block_transfer.invoke_id == request->sender_invoke_id))
            {
                CSM_LOG("[SVC] GET-Request-Next, block %lu",
                        (unsigned long)state->block_transfer.block_number);

                array->wr_index = 0U;
                if (csm_block_encode_next(&state->block_transfer, array, SVC_MAX_BLOCK_DATA_SIZE))
                {
                    code = CSM_OK;
                }
                else
                {
                    CSM_ERR("[SVC] Block encoding failed");
                    code = CSM_ERR_BAD_ENCODING;
                }
            }
            else
            {
                CSM_LOG("[SVC] No active block transfer for invoke_id %u", request->sender_invoke_id);
                array->wr_index = 0U;
                if (svc_exception_response_encoder(array))
                {
                    code = CSM_OK;
                }
            }
            return code;
        }

        /* Normal GET request */
        if (database != NULL)
        {
            /* Try normal response first */
            array->wr_index = 0U;
            CSM_LOG("[SVC] Encoding GET.response (normal)");

            int valid = csm_array_write_u8(array, AXDR_GET_RESPONSE);
            valid = valid && csm_array_write_u8(array, 1U); /* Response-Normal */
            valid = valid && csm_array_write_u8(array, request->sender_invoke_id);
            valid = valid && csm_array_write_u8(array, 0U); /* data result */

            if (valid)
            {
                code = database(ctx, array, array, request);
            }

            if (code != CSM_OK && code != CSM_OK_BLOCK)
            {
                array->wr_index = 0U;
                valid = csm_array_write_u8(array, AXDR_GET_RESPONSE);
                valid = valid && csm_array_write_u8(array, 1U);
                valid = valid && csm_array_write_u8(array, request->sender_invoke_id);
                valid = valid && csm_array_write_u8(array, 1U);
                valid = valid && csm_array_write_u8(array, CSM_ACCESS_RESULT_OBJECT_UNDEFINED);
                if (valid)
                {
                    code = CSM_OK;
                }
            }

            /* If data too large, start block transfer */
            if (code == CSM_OK_BLOCK)
            {
                CSM_LOG("[SVC] Data too large, starting block transfer");

                /* Re-encode with the full data using a temporary buffer */
                csm_array temp;
                uint8_t temp_buf[SVC_MAX_BLOCK_DATA_SIZE];
                temp.buff = temp_buf;
                temp.size = SVC_MAX_BLOCK_DATA_SIZE;
                temp.rd_index = 0U;
                temp.wr_index = 0U;
                temp.offset = 0U;

                /* Get the data from the database */
                code = database(ctx, &temp, &temp, request);

                if ((code == CSM_OK) || (code == CSM_OK_BLOCK))
                {
                    /* Start block transfer */
                    if (csm_block_start_server(&state->block_transfer,
                                               request->sender_invoke_id,
                                               temp.buff, temp.wr_index,
                                               0U))
                    {
                        /* Encode first block */
                        array->wr_index = 0U;
                        if (csm_block_encode_first(&state->block_transfer, array, SVC_MAX_BLOCK_DATA_SIZE))
                        {
                            code = CSM_OK;
                        }
                        else
                        {
                            CSM_ERR("[SVC] First block encoding failed");
                            code = CSM_ERR_BAD_ENCODING;
                        }
                    }
                    else
                    {
                        CSM_ERR("[SVC] Failed to start block transfer");
                        code = CSM_ERR_OBJECT_ERROR;
                    }
                }
            }
        }
        else
        {
            CSM_ERR("[SVC] Database pointer not set");
            code = CSM_ERR_OBJECT_ERROR;
        }
    }

    if (code != CSM_OK)
    {
        array->wr_index = 0U;
        if (svc_exception_response_encoder(array))
        {
            code = CSM_OK;
        }
        else
        {
            CSM_ERR("[SVC] Internal problem, cannot encode exception response");
        }
    }

    return code;
}


static const uint32_t gResponseNormalHeaderSize = 6U; // Offset where data can be returned for an Action

static csm_db_code svc_set_or_action_execute(csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    csm_db_code code = CSM_ERR_BAD_ENCODING;
    (void) state;

    if (request->type == SVC_REQUEST_NORMAL)
    {
        if (database != NULL)
        {
            CSM_LOG("[SVC] Encoding SET/ACTION.response");

            // The output data will point to a different area into our working buffer
            // This will help us to encode the data
            csm_array output = *array;
            uint32_t reply_size = 0U;
            output.offset += gResponseNormalHeaderSize; // begin to encode the reply just after the response header
            output.rd_index = 0U;
            output.wr_index = 0U;

            csm_array *input = (request->db_request.additional_data.enable != 0U)
                ? &request->db_request.additional_data.data
                : array;
            code = database(ctx, input, &output, request);

            reply_size = output.wr_index;

            // Encode the response
            output.offset -= gResponseNormalHeaderSize;
            output.wr_index = 0U;

            uint8_t service_resp = (request->db_request.service == SVC_SET) ? AXDR_SET_RESPONSE : AXDR_ACTION_RESPONSE;
            int valid = csm_array_write_u8(&output, service_resp);
            valid = valid && csm_array_write_u8(&output, 1U); /* Response-Normal (type 1) */
            valid = valid && csm_array_write_u8(&output, request->sender_invoke_id);
            valid = svc_data_access_result_encoder(&output, code);

            if (request->db_request.service == SVC_ACTION)
            {
                // Encode additional data if any
                if (reply_size > 0U)
                {
                    valid = valid && csm_array_write_u8(&output, 1U); // presence flag for optional return-parameters
                    valid = valid && csm_array_write_u8(&output, 0U); // Data
                    valid = valid && csm_array_writer_jump(&output, reply_size); // Virtually add the data (already encoded in the buffer)
                }
                else
                {
                    valid = valid && csm_array_write_u8(&output, 0U); // presence flag for optional return-parameters
                }
            }

            // Update size to send to output channel
            array->wr_index = output.wr_index;

            if (!valid)
            {
                code = CSM_ERR_BAD_ENCODING;
            }
            else
            {
                code = CSM_OK;
            }
        }
        else
        {
            CSM_ERR("[SVC][SET] Database pointer not set");
            code = CSM_ERR_OBJECT_ERROR;
        }
    }

    if (code != CSM_OK)
    {
        array->wr_index = 0U;
        if (svc_exception_response_encoder(array))
        {
            code = CSM_OK;
        }
        else
        {
            CSM_ERR("[SVC][SET] Internal problem, cannot encore exception response");
        }
    }

    return code;
}

static csm_db_code svc_set_or_action_decoder(csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    if (!svc_decode_request(request, array))
    {
        return CSM_ERR_BAD_ENCODING;
    }

    return svc_set_or_action_execute(ctx, state, request, array);
}

static csm_db_code svc_set_request_decoder(csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    csm_db_code code = CSM_ERR_BAD_ENCODING;

    request->db_request.service = SVC_SET;
    CSM_LOG("[SVC] Decoding SET.request");

    if (svc_decode_request(request, array))
    {
        /* Handle Set-Request-Next (block transfer continuation) */
        if (request->type == SVC_REQUEST_NEXT)
        {
            if (csm_block_is_active(&state->block_transfer) &&
                (state->block_transfer.invoke_id == request->sender_invoke_id))
            {
                CSM_LOG("[SVC] SET-Request-Next, block %lu",
                        (unsigned long)state->block_transfer.block_number);

                /* Extract data from this block */
                const uint8_t *data_ptr = &array->buff[array->rd_index];
                uint32_t data_size = array->wr_index - array->rd_index;

                /* Add data to receive buffer */
                if (csm_block_receive_data(&state->block_transfer, data_ptr, data_size, request->db_request.block_number))
                {
                    /* Encode response acknowledgment */
                    array->wr_index = 0U;
                    if (csm_block_encode_set_response(&state->block_transfer, array))
                    {
                        /* If this was the last block, process the accumulated data */
                        if (state->block_transfer.last_block)
                        {
                            const uint8_t *accumulated_data;
                            uint32_t accumulated_size;

                            if (csm_block_get_received(&state->block_transfer, &accumulated_data, &accumulated_size))
                            {
                                /* Create a temporary array with accumulated data */
                                csm_array data_array;
                                data_array.buff = (uint8_t *)accumulated_data;
                                data_array.size = accumulated_size;
                                data_array.rd_index = 0U;
                                data_array.wr_index = accumulated_size;
                                data_array.offset = 0U;

                                /* Invoke database handler */
                                if (database != NULL)
                                {
                                    csm_array output = *array;
                                    uint32_t reply_size = 0U;
                                    output.offset += gResponseNormalHeaderSize;
                                    output.rd_index = 0U;
                                    output.wr_index = 0U;

                                    code = database(ctx, &data_array, &output, request);
                                    reply_size = output.wr_index;

                                    /* Update array with response */
                                    array->wr_index = 0U;
                                    output.offset -= gResponseNormalHeaderSize;
                                    output.rd_index = 0U;
                                    output.wr_index = 0U;

                                    int valid = csm_array_write_u8(&output, AXDR_SET_RESPONSE);
                                    valid = valid && csm_array_write_u8(&output, 1U); /* Response-Normal */
                                    valid = valid && csm_array_write_u8(&output, request->sender_invoke_id);
                                    valid = svc_data_access_result_encoder(&output, code);

                                    array->wr_index = output.wr_index;
                                }
                                else
                                {
                                    code = CSM_ERR_OBJECT_ERROR;
                                }
                            }
                            else
                            {
                                code = CSM_ERR_BAD_ENCODING;
                            }
                        }
                        else
                        {
                            code = CSM_OK;
                        }
                    }
                    else
                    {
                        CSM_ERR("[SVC] Block encoding failed");
                        code = CSM_ERR_BAD_ENCODING;
                    }
                }
                else
                {
                    CSM_ERR("[SVC] Block receive failed");
                    code = CSM_ERR_BAD_ENCODING;
                }
            }
            else
            {
                CSM_LOG("[SVC] No active block transfer for invoke_id %u", request->sender_invoke_id);
                array->wr_index = 0U;
                if (svc_exception_response_encoder(array))
                {
                    code = CSM_OK;
                }
            }
            return code;
        }

        /* Normal SET request */
        if (database != NULL)
        {
            /* Check if data needs block transfer */
            uint32_t data_size = csm_array_written(&request->db_request.additional_data.data);

            if (data_size > SVC_MAX_BLOCK_DATA_SIZE)
            {
                CSM_LOG("[SVC] Data too large for single SET, starting block transfer");

                /* Start receive mode */
                if (csm_block_start_receive(&state->block_transfer, request->sender_invoke_id, 0U))
                {
                    /* Add the initial data chunk */
                    const uint8_t *initial_data = request->db_request.additional_data.data.buff;
                    uint32_t initial_size = request->db_request.additional_data.data.wr_index;

                    if (csm_block_receive_data(&state->block_transfer, initial_data, initial_size, 0U))
                    {
                        /* Encode acknowledgment for first block */
                        array->wr_index = 0U;
                        if (csm_block_encode_set_response(&state->block_transfer, array))
                        {
                            code = CSM_OK;
                        }
                        else
                        {
                            CSM_ERR("[SVC] First block encoding failed");
                            code = CSM_ERR_BAD_ENCODING;
                        }
                    }
                    else
                    {
                        CSM_ERR("[SVC] Initial data receive failed");
                        code = CSM_ERR_BAD_ENCODING;
                    }
                }
                else
                {
                    CSM_ERR("[SVC] Failed to start block receive");
                    code = CSM_ERR_OBJECT_ERROR;
                }
            }
            else
            {
                /* Data fits in single block, use normal SET */
                return svc_set_or_action_execute(ctx, state, request, array);
            }
        }
        else
        {
            CSM_ERR("[SVC][SET] Database pointer not set");
            code = CSM_ERR_OBJECT_ERROR;
        }
    }

    if (code != CSM_OK)
    {
        array->wr_index = 0U;
        if (svc_exception_response_encoder(array))
        {
            code = CSM_OK;
        }
        else
        {
            CSM_ERR("[SVC][SET] Internal problem, cannot encode exception response");
        }
    }

    return code;
}


static csm_db_code svc_action_request_decoder(csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    request->db_request.service = SVC_ACTION;
    CSM_LOG("[SVC] Decoding ACTION.request");
    return svc_set_or_action_decoder(ctx, state, request, array);
}


uint8_t csm_get_request_type(csm_request *request)
{
    uint8_t type = 0U; /* Invalid type */

    /* IEC 62056-5-3: type 1 = Normal, type 2 = With DataBlock (Next) */
    if (request->type == SVC_REQUEST_NORMAL)
    {
        type = 1U;
    }
    else
    {
        type = 2U;
    }

    return type;
}

/*
Action request (Here, MD5 pass 3):

C3 01 41

  000F
  00 00 28 00 00 FF

  01 // method 1

  01 // Have Data (true)

  0910 41C845AEEA55D9C9CDE708AF0BA5B4BD

 */

int svc_request_encoder(csm_request *request, csm_array *array)
{
    uint8_t tag = (request->db_request.service == SVC_GET) ? AXDR_GET_REQUEST :  (request->db_request.service == SVC_SET) ? AXDR_SET_REQUEST : AXDR_ACTION_REQUEST;
    int valid = csm_array_write_u8(array, tag);
    valid = valid && csm_array_write_u8(array, csm_get_request_type(request));

    if (request->type == SVC_REQUEST_NORMAL)
    {
        valid = valid && csm_array_write_u8(array, request->sender_invoke_id);
        valid = valid && csm_array_write_u16(array, request->db_request.logical_name.class_id);
        valid = valid && csm_array_write_buff(array, (const uint8_t *)&request->db_request.logical_name.obis.A, 6U);
        valid = valid && csm_array_write_u8(array, request->db_request.logical_name.id);

        // Additional data only for SET and ACTION

        if (tag != AXDR_ACTION_REQUEST)
        {
            valid = valid && csm_array_write_u8(array, request->db_request.sel_access.enable); // use selective access or not

            if (request->db_request.sel_access.enable)
            {
                if (request->db_request.sel_access.data.buff != NULL)
                {
                    valid = valid && csm_array_write_buff(array, request->db_request.sel_access.data.buff, csm_array_written(&request->db_request.sel_access.data));
                }
                else
                {
                    valid = FALSE;
                }
            }
        }
        else
        {
            // Data is optional for ACTION
            valid = valid && csm_array_write_u8(array, request->db_request.additional_data.enable); // use data or not
        }

        // Add data, valid for ACTION or SET only
        if (tag != AXDR_GET_REQUEST)
        {
            uint32_t data_size = csm_array_written(&request->db_request.additional_data.data);
            if ((request->db_request.additional_data.enable) && (data_size > 0U))
            {
                if (request->db_request.additional_data.data.buff != NULL)
                {
                    valid = valid && csm_array_write_buff(array, request->db_request.additional_data.data.buff, csm_array_written(&request->db_request.additional_data.data));
                }
                else
                {
                    valid = FALSE;
                }
            }
            else
            {
                if (tag == AXDR_SET_REQUEST)
                {
                    // No data (NULL)
                    valid = valid && csm_array_write_u8(array, AXDR_TAG_NULL);
                }
            }
        }

    }
    else if (request->type == SVC_REQUEST_NEXT)
    {
        valid = valid && csm_array_write_u8(array, request->sender_invoke_id); // save the invoke ID to reuse the same
        valid = valid && csm_array_write_u32(array, request->db_request.block_number); // save the invoke ID to reuse the same
    }
    else
    {
        CSM_LOG("[SVC] Service not supported");
    }
    return valid;
}


typedef csm_db_code (*svc_func)(csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array);


typedef struct
{
    uint8_t tag;
    svc_func decoder;   //!< Used by the server implementation

} csm_service_handler;

static const csm_service_handler services[] =
{
    { AXDR_GET_REQUEST, svc_get_request_decoder },
    { AXDR_SET_REQUEST, svc_set_request_decoder },
    { AXDR_ACTION_REQUEST, svc_action_request_decoder }
};

#define NUMBER_OF_SERVICES (sizeof(services) / sizeof(services[0]))

void csm_services_init(const csm_db_access_handler db_access)
{
    database = db_access;
}

int csm_services_hls_execute(csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    CSM_LOG("[SVC] Received HLS Pass 3");
    return csm_server_services_execute(ctx, state, request, array);
}

int csm_server_services_execute(csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    return csm_server_services_execute_handler(database, ctx, state, request, array);
}

int csm_services_hls_execute_handler(csm_db_access_handler handler, csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    CSM_LOG("[SVC] Received HLS Pass 3 (handler)");
    return csm_server_services_execute_handler(handler, ctx, state, request, array);
}

int csm_server_services_execute_handler(csm_db_access_handler handler, csm_db_context_t *ctx, csm_asso_state *state, csm_request *request, csm_array *array)
{
    int number_of_bytes = 0;

    /* Temporarily set global for service decoders that use it directly.
     * Future improvement: refactor service decoders to accept handler as parameter
     * to eliminate this thread-safety workaround. */
    csm_db_access_handler saved_db = database;
    database = handler;

    if (handler != NULL)
    {
        uint8_t tag;
        if (csm_array_read_u8(array, &tag))
        {
            for (uint32_t i = 0U; i < NUMBER_OF_SERVICES; i++)
            {
                const csm_service_handler *srv = &services[i];
                if ((srv->tag == tag) && (srv->decoder != NULL))
                {
                    CSM_LOG("[SVC] Found service");
                    if (srv->decoder(ctx, state, request, array) == CSM_OK)
                    {
                        number_of_bytes = array->wr_index;
                    }
                    else
                    {
                        CSM_ERR("[SVC] Encoding error!");
                    }
                    break;
                }
            }
        }
    }

    /* Restore global */
    database = saved_db;

    return number_of_bytes;
}


int svc_is_valid_data_access_result(uint8_t result)
{
    int valid = FALSE;
    switch (result)
    {
        case CSM_ACCESS_RESULT_SUCCESS:
        case CSM_ACCESS_RESULT_HARDWARE_FAULT:
        case CSM_ACCESS_RESULT_TEMPORARY_FAILURE:
        case CSM_ACCESS_RESULT_READ_WRITE_DENIED:
        case CSM_ACCESS_RESULT_OBJECT_UNDEFINED:
        case CSM_ACCESS_RESULT_OBJECT_CLASS_INCONSISTENT:
        case CSM_ACCESS_RESULT_OBJECT_UNAVAILABLE:
        case CSM_ACCESS_RESULT_TYPE_UNMATCHED:
        case CSM_ACCESS_RESULT_SCOPE_OF_ACCESS_VIOLATED:
        case CSM_ACCESS_RESULT_DATA_BLOCK_UNAVAILABLE:
        case CSM_ACCESS_RESULT_LONG_GET_ABORTED:
        case CSM_ACCESS_RESULT_NO_LONG_GET_IN_PROGRESS:
        case CSM_ACCESS_RESULT_LONG_SET_ABORTED:
        case CSM_ACCESS_RESULT_NO_LONG_SET_IN_PROGRESS:
        case CSM_ACCESS_RESULT_DATA_BLOCK_NUMBER_INVALID:
        case CSM_ACCESS_RESULT_OTHER_REASON:
            valid = TRUE;
            break;
        default:
            break;
    }
    return valid;
}

int svc_is_valid_action_result(uint8_t result)
{
    int valid = FALSE;
    switch (result)
    {
        case CSM_ACTION_RESULT_SUCCESS:
        case CSM_ACTION_RESULT_HARDWARE_FAULT:
        case CSM_ACTION_RESULT_TEMPORARY_FAILURE:
        case CSM_ACTION_RESULT_READ_WRITE_DENIED:
        case CSM_ACTION_RESULT_OBJECT_UNDEFINED:
        case CSM_ACTION_RESULT_OBJECT_CLASS_INCONSISTENT:
        case CSM_ACTION_RESULT_OBJECT_UNAVAILABLE:
        case CSM_ACTION_RESULT_TYPE_UNMATCHED:
        case CSM_ACTION_RESULT_SCOPE_OF_ACCESS_VIOLATED:
        case CSM_ACTION_RESULT_DATA_BLOCK_UNAVAILABLE:
        case CSM_ACTION_RESULT_LONG_ACTION_ABORTED:
        case CSM_ACTION_RESULT_NO_LONG_ACTION_IN_PROGRESS:
        case CSM_ACTION_RESULT_OTHER_REASON:
            valid = TRUE;
            break;
        default:
            break;
    }
    return valid;
}


int svc_is_normal_response(uint8_t type)
{
    int istype = FALSE;

    // Same type for SET/GET/ACTION
    if (type == 1U)
    {
        istype = TRUE;
    }

    return istype;
}

int svc_is_data_block_response(uint8_t type)
{
    int istype = FALSE;

    // Same type for SET/GET/ACTION
    if (type == 2U)
    {
        istype = TRUE;
    }

    return istype;
}


int svc_result_decoder(csm_response *response, csm_array *array)
{
    uint8_t result;
    int valid = csm_array_read_u8(array, &result);

    // for now, set it for everyone (default values)
    response->has_data = FALSE;
    response->access_result = CSM_ACCESS_RESULT_NOT_SET;

    if (response->service == SVC_ACTION)
    {
        /* Action is a SET followed by a GET
         * Two statuses: Action-Result (SET part) and Get-Data-Result (GET part)
         * Format: C7 01 <invoke_id> <action_result> <presence_flag> [<data_or_result>]
         */

        valid = valid && svc_is_valid_action_result(result);
        if (valid)
        {
            response->action_result = (csm_action_result)result;
            CSM_LOG("[SVC] Decoded service result");

            // Any Additional information for the GET part?
            valid = valid && csm_array_read_u8(array, &result);
            if (valid)
            {
                if (result == 1U)
                {
                    // Yes, there is a data result
                    valid = valid && svc_is_valid_data_access_result(result);

                    if (valid)
                    {
                        // Data or Data-Access-Result?
                        valid = valid && csm_array_read_u8(array, &result);

                        if (result == 1U)
                        {
                            response->access_result = (csm_data_access_result)result;
                            CSM_LOG("[SVC] Decoded access result");
                        }
                        else if (result == 0U)
                        {
                            // Next bytes are the data
                            response->has_data = TRUE;
                            response->access_result = CSM_ACCESS_RESULT_SUCCESS;
                            CSM_LOG("[SVC] Found data");
                        }
                        else
                        {
                            valid = FALSE;
                        }
                    }
                }
                else if (result != 0U)
                {
                    valid = FALSE;
                    CSM_ERR("[SVC] Bad Get-Data-Result value");
                }
            }
        }

    }
    else
    {
        valid = valid && svc_is_valid_data_access_result(result);
        if (valid)
        {
            response->access_result = (csm_data_access_result)result;
        }
        else
        {
            response->access_result = CSM_ACCESS_RESULT_NOT_SET;
        }
    }
    return valid;
}


//
// Get-Response-WithDataBlock: C4 02 C1 0000000001008201F40601001B0800FF15000000000
static int svc_response_decoder(csm_response *response, csm_array *array)
{
    uint8_t type = 0U;
    int valid = csm_array_read_u8(array, &type);
    valid = valid && csm_array_read_u8(array, &response->invoke_id);

    if (valid)
    {
        if (svc_is_normal_response(type))
        {
            response->type = SVC_RESPONSE_NORMAL;
            CSM_LOG("[SVC] Response-Normal");
            valid = valid && svc_result_decoder(response, array);
        }
        else if (svc_is_data_block_response(type))
        {
            response->type = SVC_RESPONSE_WITH_DATABLOCK;
            CSM_LOG("[SVC] Response-WithDataBlock");
            valid = valid && csm_array_read_u8(array, &response->last_block);
            valid = valid && csm_array_read_u32(array, &response->block_number);

            if (valid)
            {
                CSM_TRACE("[SVC/GET] Block number: %d\r\n", response->block_number);
                response->access_result = CSM_ACCESS_RESULT_SUCCESS;
            }

            // Now the read pointer is on the data
        }
        else
        {
            CSM_LOG("[SVC] Service not supported");
        }
    }

    return valid;
}

static int svc_get_response_decoder(csm_response *response, csm_array *array)
{
    response->service = SVC_GET;
    CSM_LOG("[SVC] Decoding GET.response");

    int valid = svc_response_decoder(response, array);

    /* If block response, accumulate data */
    if (valid && (response->type == SVC_RESPONSE_WITH_DATABLOCK))
    {
        /* Data starts at current read position */
        uint32_t data_offset = array->rd_index;
        uint32_t data_size = array->wr_index - data_offset;

        if (data_size > 0U)
        {
            /* Start receive if not already active */
            if (!csm_block_is_active(&response->block_state))
            {
                csm_block_start_get_receive(&response->block_state,
                                           response->invoke_id, 0U);
            }

            /* Accumulate this block's data */
            const uint8_t *data_ptr = &array->buff[data_offset];
            if (csm_block_get_receive_data(&response->block_state,
                                          data_ptr, data_size,
                                          response->last_block))
            {
                CSM_LOG("[SVC/GET] Accumulated %lu bytes, block %lu, last=%d",
                        (unsigned long)data_size,
                        (unsigned long)response->block_number,
                        (int)response->last_block);
            }
            else
            {
                CSM_ERR("[SVC/GET] Block accumulation failed");
                valid = 0;
            }
        }
    }

    return valid;
}

static int svc_set_response_decoder(csm_response *response, csm_array *array)
{
    response->service = SVC_SET;
    CSM_LOG("[SVC] Decoding SET.response");
    return svc_response_decoder(response, array);
}

static int svc_action_response_decoder(csm_response *response, csm_array *array)
{
    response->service = SVC_ACTION;
    CSM_LOG("[SVC] Decoding ACTION.response");
    return svc_response_decoder(response, array);
}


/*
Get request with selective access by range (date)

 7EA04F000200230332BDE2E6E600
C0 01 C1 0007 00 01 62 02 00 FF 02
  01 Use sel access
    01 by range
       02 04 structure of 4

            02 04
                    12 0008 class id
                    09 06 00 00 01 00 00 FF
                    0F 02 attribute index
                    12 0000  data_index

                09 0C 07E1080107000000FFFFC400
                09 0C 07E10B0107000000FFFFC400

                01 00 selected_values (array null)

4CD77E


Selective access with undefined end date

 C0 01 41 0007 00 00 63 01 00 FF 02

   01
     01
        02 04
             02 04
                  12 0008
                  09 06 00 00 01 00 00 FF
                  0F 02
                  12 0000

             09 0C 07E10B1BFF000000FF800000
             09 0C FFFFFFFFFFFFFFFFFF8000FF

             0100


*/

int csm_client_encode_selective_access_by_range(csm_array *array, csm_object_t *restricting_object, csm_array *start, csm_array *end)
{
    int valid = csm_array_write_u8(array, 1U); // by range
    valid = valid && csm_array_write_u8(array, AXDR_TAG_STRUCTURE);
    valid = valid && csm_ber_write_len(array, 4U);

    // 1. restricting_object
    valid = valid && csm_axdr_wr_capture_object(array, restricting_object);

    // 2. start date
    valid = valid && csm_axdr_wr_octetstring(array, csm_array_rd_data(start), csm_array_written(start));
    // 3. end date
    valid = valid && csm_axdr_wr_octetstring(array, csm_array_rd_data(end), csm_array_written(end));

    // 4. selected values
    valid = valid && csm_array_write_u8(array, 0x01U); // selected values
    valid = valid && csm_array_write_u8(array, 0x00U); // array null

    return valid;
}


static int svc_exception_decoder(csm_response *response, csm_array *array)
{
    response->service = SVC_EXCEPTION;
    CSM_LOG("[SVC] Decoding Exception");

    int valid = csm_array_read_u8(array, &response->exception.state_err);
    valid = valid && csm_array_read_u8(array, &response->exception.service_err);
    return valid;
}


typedef int (*srv_resp_decoder_func)(csm_response *response, csm_array *array);

typedef struct
{
    uint8_t tag;
    srv_resp_decoder_func decoder;

} csm_client_service_handler;

static const csm_client_service_handler client_services[] =
{
    { AXDR_GET_RESPONSE, svc_get_response_decoder },
    { AXDR_SET_RESPONSE, svc_set_response_decoder },
    { AXDR_ACTION_RESPONSE, svc_action_response_decoder },
    { AXDR_EXCEPTION_RESPONSE, svc_exception_decoder }
};

#define NUMBER_OF_CLIENT_SERVICES (sizeof(client_services) / sizeof(client_services[0]))

int csm_client_decode(csm_response *response, csm_array *array)
{
    int valid = FALSE;
    uint8_t tag;

    response->service = SVC_UNKOWN;
    if (csm_array_read_u8(array, &tag))
    {
        for (uint32_t i = 0U; i < NUMBER_OF_CLIENT_SERVICES; i++)
        {
            const csm_client_service_handler *svc = &client_services[i];
            if ((svc->tag == tag) && (svc->decoder != NULL))
            {
                valid = svc->decoder(response, array);
                break;
            }
        }
    }

    return valid;
}

void csm_client_init(csm_request *request, csm_response *response)
{
    (void) request;

    response->type = 0U;
    response->block_number = 0U;
    response->invoke_id = 0U;
    response->last_block = 0U;
    csm_block_init(&response->block_state);
}

int csm_client_has_more_data(csm_response *response)
{
    int more_data = 0;
    if ((response->type == SVC_RESPONSE_WITH_DATABLOCK) &&
        (!response->last_block))
    {
        more_data = 1;
    }
    return more_data;
}

int csm_client_get_block_data(csm_response *response, const uint8_t **data,
                              uint32_t *data_size)
{
    if (response == NULL || data == NULL || data_size == NULL)
    {
        return 0;
    }

    return csm_block_get_received_data(&response->block_state, data, data_size);
}
