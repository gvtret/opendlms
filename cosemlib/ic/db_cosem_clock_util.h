/**
 * DLMS/COSEM Clock utility functions
 *
 * Helper functions for date-time encoding/decoding per Blue Book 4.1.6.1.
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#ifndef DB_COSEM_CLOCK_UTIL_H
#define DB_COSEM_CLOCK_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "db_cosem_ic.h"

#define DB_CLOCK_DT_LEN 12U

typedef struct {
	uint16_t year;
	uint8_t month;
	uint8_t day_of_week;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t hundredths;
	int16_t deviation;
	uint8_t status;
} db_cosem_clock_data_t;

void db_cosem_clock_set_datetime(const uint8_t *dt12, db_ic_inst_t *inst);
void db_cosem_clock_get_datetime(uint8_t *dt12, const db_ic_inst_t *inst);
void db_cosem_clock_set_epoch(uint32_t seconds);
uint32_t db_cosem_clock_get_epoch(const db_ic_inst_t *inst);

#ifdef __cplusplus
}
#endif

#endif /* DB_COSEM_CLOCK_UTIL_H */
