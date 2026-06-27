/**
 * DLMS/COSEM Interface Class (IC) dispatch and registry framework
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef DB_COSEM_IC_H
#define DB_COSEM_IC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "csm_definitions.h"
#include "csm_services.h"

#define DB_IC_MAX_INSTANCES     64U
#define DB_IC_MAX_CLASSES       40U
#define DB_IC_MAX_ATTRS         16U
#define DB_IC_MAX_METHODS       8U

/* Access permission bits (extends DB_ACCESS_GET/SET from csm_services.h) */
#ifndef DB_ACCESS_ACTION
#define DB_ACCESS_ACTION    (uint16_t)4U
#endif

#ifndef DB_ACCESS_GETSET
#define DB_ACCESS_GETSET    (uint16_t)3U
#endif

typedef enum {
    IC_OP_GET   = 0,
    IC_OP_SET   = 1,
    IC_OP_ACTION = 2
} db_ic_op_t;

/* Attribute descriptor for IC framework */
typedef struct {
    uint16_t access;
    uint8_t  id;
    uint8_t  type;  /* AXDR tag */
} db_ic_attr_descr;

/* Method descriptor for IC framework */
typedef struct {
    uint16_t access;
    uint8_t  id;
    uint8_t  type;
} db_ic_method_descr;

/* Object descriptor */
typedef struct {
    const db_ic_attr_descr   *attributes;
    const db_ic_method_descr *methods;
    uint16_t class_id;
    csm_obis_code obis;
    uint8_t  attr_count;
    uint8_t  method_count;
    uint8_t  version;
} db_ic_object_descr;

/* IC instance runtime state */
typedef struct {
    const db_ic_object_descr *descr;
    csm_obis_code obis;         /* per-instance OBIS override */
    void     *data;
    uint8_t  version;
    uint8_t  has_obis;          /* 1 if obis override is set */
    void     *user_ctx;
} db_ic_inst_t;

/* Dispatch callback signature */
typedef csm_db_code (*db_ic_dispatch_fn)(db_ic_inst_t *inst, db_ic_op_t op,
                                         uint8_t attr_id, uint8_t method_id,
                                         csm_array *in, csm_array *out);

/* Factory function: create an IC instance from an OBIS code */
typedef db_ic_inst_t *(*db_ic_create_fn)(const csm_obis_code *obis);

/* IC class definition (registered in the IC registry) */
typedef struct {
    uint16_t class_id;
    const char *name;
    uint8_t version;
    const db_ic_object_descr *descr;  /* optional: used if create() doesn't set descr */
    db_ic_create_fn  create;
    db_ic_dispatch_fn dispatch;
} db_ic_class;

/* --- Public API --- */

int  db_ic_init(void);
int  db_ic_register(const db_ic_class *cls);
int  db_ic_create_inst(uint16_t class_id, const csm_obis_code *obis,
                       const void *init_data, void *user_ctx);
int  db_ic_find(uint16_t class_id, const csm_obis_code *obis,
                db_ic_inst_t **out);
int  db_ic_dispatch(db_ic_inst_t *inst, db_ic_op_t op,
                    uint8_t attr_id, uint8_t method_id,
                    csm_array *in, csm_array *out);
int  db_ic_count(void);
void db_ic_reset(void);

/* --- Built-in class registrations --- */

void db_ic_register_data(void);
void db_ic_register_register(void);
void db_ic_register_extended_register(void);
void db_ic_register_demand_register(void);
void db_ic_register_register_activation(void);
void db_ic_register_profile_generic(void);
void db_ic_register_clock(void);
void db_ic_register_association_ln(void);
void db_ic_register_image_transfer(void);
void db_ic_register_push_setup(void);
void db_ic_register_security_setup(void);
void db_ic_register_disconnect_control(void);
void db_ic_register_limiter(void);
void db_ic_register_schedule(void);
void db_ic_register_special_days(void);
void db_ic_register_script_table(void);
void db_ic_register_activity_calendar(void);
void db_ic_register_register_monitor(void);
void db_ic_register_single_action_schedule(void);
void db_ic_register_sap_assignment(void);
void db_ic_register_comms(void);
void db_ic_register_utility_tables(void);
void db_ic_register_compact_data(void);
void db_ic_register_register_table(void);
void db_ic_register_status_mapping(void);
void db_ic_register_parameter_monitor(void);
void db_ic_register_arbitrator(void);
void db_ic_register_sensor_manager(void);
void db_ic_register_data_protection(void);
void db_ic_register_profile_filter(void);
void db_ic_register_table_manager(void);

/* Helper: register all built-in classes */
void db_ic_register_all_builtins(void);

/* Helper: reset all per-class instance counters (called by db_ic_init) */
void db_ic_reset_all_counts(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_COSEM_IC_H */
