/**
 * DLMS/COSEM Communications Setup Interface Class handlers
 *
 * Class 19 - IEC Local Port Setup:
 *   Attr 1: logical_name (octet-string, static)
 *   Attr 2: comm_speed (enum, static)
 *
 * Class 23 - IEC HDLC Setup:
 *   Attr 1: logical_name (octet-string, static)
 *   Attr 2: channel (unsigned8, static)
 *   Attr 3: phy (structure, static)
 *   Attr 4: llc (structure, static)
 *   Attr 5: mac (structure, static)
 *
 * Class 41 - TCP-UDP Setup:
 *   Attr 1: logical_name (octet-string, static)
 *   Attr 2: ip_address (octet-string, static)
 *   Attr 3: subnet_mask (octet-string, static)
 *   Attr 4: gateway_ip_address (octet-string, static)
 *   Attr 5: use_dns (boolean, static)
 *   Attr 6: primary_dns_address (octet-string, static)
 *   Attr 7: secondary_dns_address (octet-string, static)
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "db_cosem_ic.h"
#include "csm_config.h"
#include "csm_axdr_codec.h"
#include <string.h>

#define COMMS_MAX_INSTANCES     4U
#define COMMS_IP_LEN            4U
#define COMMS_MAC_MAX           12U

/* ========================= IEC Local Port Setup (Class 19) ========================= */

typedef struct {
    uint8_t comm_speed;
} db_ic_iec_local_data;

static db_ic_iec_local_data iec_local_pool[COMMS_MAX_INSTANCES];
static uint8_t iec_local_count = 0U;

static db_ic_inst_t iec_local_inst_tmp;

static const db_ic_attr_descr iec_local_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_ENUM },
};

static const db_ic_object_descr iec_local_descr = {
    .attributes   = iec_local_attrs,
    .methods      = NULL,
    .class_id     = 19,
    .obis         = { 0, 0, 10, 0, 0, 255 },
    .attr_count   = 2,
    .method_count = 0,
    .version      = 0
};

static db_ic_inst_t *iec_local_create(const csm_obis_code *obis)
{
    (void) obis;
    if (iec_local_count >= COMMS_MAX_INSTANCES) { return NULL; }

    db_ic_iec_local_data *d = &iec_local_pool[iec_local_count];
    memset(d, 0, sizeof(db_ic_iec_local_data));
    d->comm_speed = 0U;
    iec_local_count++;

    memset(&iec_local_inst_tmp, 0, sizeof(db_ic_inst_t));
    iec_local_inst_tmp.descr   = &iec_local_descr;
    iec_local_inst_tmp.data    = d;
    iec_local_inst_tmp.version = 0U;
    return &iec_local_inst_tmp;
}

static csm_db_code iec_local_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                       uint8_t attr_id, uint8_t method_id,
                                       csm_array *in, csm_array *out)
{
    (void) method_id;
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_iec_local_data *d = (db_ic_iec_local_data *)inst->data;

    if (op == IC_OP_GET)
    {
        if (attr_id == 1U)
        {
            const csm_obis_code *obis = inst->has_obis
                ? &inst->obis : &inst->descr->obis;
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, 6U);
            valid = valid && csm_array_write_u8(out, obis->A);
            valid = valid && csm_array_write_u8(out, obis->B);
            valid = valid && csm_array_write_u8(out, obis->C);
            valid = valid && csm_array_write_u8(out, obis->D);
            valid = valid && csm_array_write_u8(out, obis->E);
            valid = valid && csm_array_write_u8(out, obis->F);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 2U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_ENUM);
            valid = valid && csm_array_write_u8(out, d->comm_speed);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_ENUM) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &d->comm_speed)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_iec_local = {
    .class_id  = 19,
    .name      = "IEC Local Port Setup",
    .version   = 0,
    .create    = iec_local_create,
    .dispatch  = iec_local_dispatch
};

void db_ic_register_iec_local_port_setup(void) { db_ic_register(&ic_iec_local); }

/* ========================= IEC HDLC Setup (Class 23) ========================= */

typedef struct {
    uint8_t channel;
    uint8_t phy_buf[16];
    uint8_t phy_len;
    uint8_t llc_buf[16];
    uint8_t llc_len;
    uint8_t mac_buf[COMMS_MAC_MAX];
    uint8_t mac_len;
} db_ic_iec_hdlc_data;

static db_ic_iec_hdlc_data iec_hdlc_pool[COMMS_MAX_INSTANCES];
static uint8_t iec_hdlc_count = 0U;

static db_ic_inst_t iec_hdlc_inst_tmp;

static const db_ic_attr_descr iec_hdlc_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_UNSIGNED8 },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_STRUCTURE },
    { DB_ACCESS_GET | DB_ACCESS_SET,  5, AXDR_TAG_STRUCTURE },
};

static const db_ic_object_descr iec_hdlc_descr = {
    .attributes   = iec_hdlc_attrs,
    .methods      = NULL,
    .class_id     = 23,
    .obis         = { 0, 0, 22, 0, 0, 255 },
    .attr_count   = 5,
    .method_count = 0,
    .version      = 0
};

static db_ic_inst_t *iec_hdlc_create(const csm_obis_code *obis)
{
    (void) obis;
    if (iec_hdlc_count >= COMMS_MAX_INSTANCES) { return NULL; }

    db_ic_iec_hdlc_data *d = &iec_hdlc_pool[iec_hdlc_count];
    memset(d, 0, sizeof(db_ic_iec_hdlc_data));
    iec_hdlc_count++;

    memset(&iec_hdlc_inst_tmp, 0, sizeof(db_ic_inst_t));
    iec_hdlc_inst_tmp.descr   = &iec_hdlc_descr;
    iec_hdlc_inst_tmp.data    = d;
    iec_hdlc_inst_tmp.version = 0U;
    return &iec_hdlc_inst_tmp;
}

static csm_db_code iec_hdlc_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                       uint8_t attr_id, uint8_t method_id,
                                       csm_array *in, csm_array *out)
{
    (void) method_id;
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_iec_hdlc_data *d = (db_ic_iec_hdlc_data *)inst->data;

    if (op == IC_OP_GET)
    {
        if (attr_id == 1U)
        {
            const csm_obis_code *obis = inst->has_obis
                ? &inst->obis : &inst->descr->obis;
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, 6U);
            valid = valid && csm_array_write_u8(out, obis->A);
            valid = valid && csm_array_write_u8(out, obis->B);
            valid = valid && csm_array_write_u8(out, obis->C);
            valid = valid && csm_array_write_u8(out, obis->D);
            valid = valid && csm_array_write_u8(out, obis->E);
            valid = valid && csm_array_write_u8(out, obis->F);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 2U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_UNSIGNED8);
            valid = valid && csm_array_write_u8(out, d->channel);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_buff(out, d->phy_buf, d->phy_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_buff(out, d->llc_buf, d->llc_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 5U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_STRUCTURE);
            valid = valid && csm_array_write_buff(out, d->mac_buf, d->mac_len);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_UNSIGNED8) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &d->channel)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_iec_hdlc = {
    .class_id  = 23,
    .name      = "IEC HDLC Setup",
    .version   = 0,
    .create    = iec_hdlc_create,
    .dispatch  = iec_hdlc_dispatch
};

void db_ic_register_iec_hdlc_setup(void) { db_ic_register(&ic_iec_hdlc); }

/* ========================= TCP-UDP Setup (Class 41) ========================= */

typedef struct {
    uint8_t ip_address[COMMS_IP_LEN];
    uint8_t subnet_mask[COMMS_IP_LEN];
    uint8_t gateway_ip[COMMS_IP_LEN];
    uint8_t use_dns;
    uint8_t primary_dns[COMMS_IP_LEN];
    uint8_t secondary_dns[COMMS_IP_LEN];
} db_ic_tcp_udp_data;

static db_ic_tcp_udp_data tcp_udp_pool[COMMS_MAX_INSTANCES];
static uint8_t tcp_udp_count = 0U;

static db_ic_inst_t tcp_udp_inst_tmp;

void db_ic_comms_reset_count(void)
{
    iec_local_count = 0U;
    iec_hdlc_count = 0U;
    tcp_udp_count = 0U;
}

static const db_ic_attr_descr tcp_udp_attrs[] = {
    { DB_ACCESS_GET,                  1, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  2, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  3, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  4, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  5, AXDR_TAG_BOOLEAN },
    { DB_ACCESS_GET | DB_ACCESS_SET,  6, AXDR_TAG_OCTETSTRING },
    { DB_ACCESS_GET | DB_ACCESS_SET,  7, AXDR_TAG_OCTETSTRING },
};

static const db_ic_object_descr tcp_udp_descr = {
    .attributes   = tcp_udp_attrs,
    .methods      = NULL,
    .class_id     = 41,
    .obis         = { 0, 0, 25, 0, 0, 255 },
    .attr_count   = 7,
    .method_count = 0,
    .version      = 0
};

static db_ic_inst_t *tcp_udp_create(const csm_obis_code *obis)
{
    (void) obis;
    if (tcp_udp_count >= COMMS_MAX_INSTANCES) { return NULL; }

    db_ic_tcp_udp_data *d = &tcp_udp_pool[tcp_udp_count];
    memset(d, 0, sizeof(db_ic_tcp_udp_data));
    tcp_udp_count++;

    memset(&tcp_udp_inst_tmp, 0, sizeof(db_ic_inst_t));
    tcp_udp_inst_tmp.descr   = &tcp_udp_descr;
    tcp_udp_inst_tmp.data    = d;
    tcp_udp_inst_tmp.version = 0U;
    return &tcp_udp_inst_tmp;
}

static int comms_read_octet_string(csm_array *in, uint8_t *buf, uint8_t max_len, uint8_t *out_len)
{
    uint8_t tag = 0xFFU;
    uint8_t len = 0U;
    if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_OCTETSTRING) { return FALSE; }
    if (!csm_array_read_u8(in, &len) || len > max_len) { return FALSE; }
    if (len > 0U && !csm_array_read_buff(in, buf, len)) { return FALSE; }
    *out_len = len;
    return TRUE;
}

static csm_db_code tcp_udp_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                                      uint8_t attr_id, uint8_t method_id,
                                      csm_array *in, csm_array *out)
{
    (void) method_id;
    if ((inst == NULL) || (inst->data == NULL)) { return CSM_ERR_OBJECT_NOT_FOUND; }
    db_ic_tcp_udp_data *d = (db_ic_tcp_udp_data *)inst->data;

    if (op == IC_OP_GET)
    {
        if (attr_id == 1U)
        {
            const csm_obis_code *obis = inst->has_obis
                ? &inst->obis : &inst->descr->obis;
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, 6U);
            valid = valid && csm_array_write_u8(out, obis->A);
            valid = valid && csm_array_write_u8(out, obis->B);
            valid = valid && csm_array_write_u8(out, obis->C);
            valid = valid && csm_array_write_u8(out, obis->D);
            valid = valid && csm_array_write_u8(out, obis->E);
            valid = valid && csm_array_write_u8(out, obis->F);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 2U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, COMMS_IP_LEN);
            valid = valid && csm_array_write_buff(out, d->ip_address, COMMS_IP_LEN);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 3U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, COMMS_IP_LEN);
            valid = valid && csm_array_write_buff(out, d->subnet_mask, COMMS_IP_LEN);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 4U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, COMMS_IP_LEN);
            valid = valid && csm_array_write_buff(out, d->gateway_ip, COMMS_IP_LEN);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 5U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_BOOLEAN);
            valid = valid && csm_array_write_u8(out, d->use_dns);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 6U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, COMMS_IP_LEN);
            valid = valid && csm_array_write_buff(out, d->primary_dns, COMMS_IP_LEN);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
        else if (attr_id == 7U)
        {
            int valid = csm_array_write_u8(out, AXDR_TAG_OCTETSTRING);
            valid = valid && csm_array_write_u8(out, COMMS_IP_LEN);
            valid = valid && csm_array_write_buff(out, d->secondary_dns, COMMS_IP_LEN);
            return valid ? CSM_OK : CSM_ERR_BAD_ENCODING;
        }
    }
    else if (op == IC_OP_SET)
    {
        if (attr_id == 2U)
        {
            uint8_t len = 0U;
            if (!comms_read_octet_string(in, d->ip_address, COMMS_IP_LEN, &len)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 3U)
        {
            uint8_t len = 0U;
            if (!comms_read_octet_string(in, d->subnet_mask, COMMS_IP_LEN, &len)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 4U)
        {
            uint8_t len = 0U;
            if (!comms_read_octet_string(in, d->gateway_ip, COMMS_IP_LEN, &len)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 5U)
        {
            uint8_t tag = 0xFFU;
            if (!csm_array_read_u8(in, &tag) || tag != AXDR_TAG_BOOLEAN) { return CSM_ERR_BAD_ENCODING; }
            if (!csm_array_read_u8(in, &d->use_dns)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 6U)
        {
            uint8_t len = 0U;
            if (!comms_read_octet_string(in, d->primary_dns, COMMS_IP_LEN, &len)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
        else if (attr_id == 7U)
        {
            uint8_t len = 0U;
            if (!comms_read_octet_string(in, d->secondary_dns, COMMS_IP_LEN, &len)) { return CSM_ERR_BAD_ENCODING; }
            return CSM_OK;
        }
    }
    return CSM_ERR_OBJECT_NOT_FOUND;
}

static const db_ic_class ic_tcp_udp = {
    .class_id  = 41,
    .name      = "TCP-UDP Setup",
    .version   = 0,
    .create    = tcp_udp_create,
    .dispatch  = tcp_udp_dispatch
};

void db_ic_register_tcp_udp_setup(void) { db_ic_register(&ic_tcp_udp); }

void db_ic_register_comms(void)
{
    db_ic_register_iec_local_port_setup();
    db_ic_register_iec_hdlc_setup();
    db_ic_register_tcp_udp_setup();
}
