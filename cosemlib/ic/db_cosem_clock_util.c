/**
 * DLMS/COSEM Clock utility functions
 *
 * Helper functions for date-time encoding/decoding per Blue Book 4.1.6.1.
 *
 * The 12-byte date-time format:
 *   Byte 0: Year high (FF if not used)
 *   Byte 1: Year low
 *   Byte 2: Month
 *   Byte 3: Day of week
 *   Byte 4: Day
 *   Byte 5: Hour
 *   Byte 6: Minute
 *   Byte 7: Second
 *   Byte 8: Hundredths (FF if not used)
 *   Byte 9: Deviation high
 *   Byte 10: Deviation low
 *   Byte 11: Clock status (FF if valid)
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the MIT license.
 * See LICENSE.txt for more details.
 *
 */

#include "db_cosem_clock_util.h"
#include "csm_config.h"
#include <string.h>

static uint32_t days_in_month(uint16_t year, uint8_t month) {
	static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

	if (month == 0U || month > 12U) {
		return 0U;
	}

	uint32_t d = days[month - 1U];
	if (month == 2U) {
		if ((year % 4U == 0U) && ((year % 100U != 0U) || (year % 400U == 0U))) {
			d = 29U;
		}
	}
	return d;
}

static uint8_t calc_day_of_week(uint16_t year, uint8_t month, uint8_t day) {
	if (month < 3U) {
		month += 12U;
		year--;
	}

	uint32_t q = day;
	uint32_t m = month;
	uint32_t k = year % 100U;
	uint32_t j = year / 100U;

	uint32_t h = (q + ((13U * (m + 1U)) / 5U) + k + (k / 4U) + (j / 4U) + (5U * j)) % 7U;
	uint8_t dow = (uint8_t)((h + 6U) % 7U);
	if (dow == 0U) {
		dow = 7U;
	}
	return dow;
}

static uint32_t epoch_from_broken(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
	uint32_t days = 0U;
	uint16_t y;

	for (y = 1970U; y < year; y++) {
		days += 365U;
		if ((y % 4U == 0U) && ((y % 100U != 0U) || (y % 400U == 0U))) {
			days += 1U;
		}
	}

	for (uint8_t m = 1U; m < month; m++) {
		days += days_in_month(year, m);
	}

	days += (uint32_t)(day - 1U);

	return (days * 86400U) + ((uint32_t)hour * 3600U) + ((uint32_t)minute * 60U) + (uint32_t)second;
}

static void broken_from_epoch(uint32_t epoch, uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *minute, uint8_t *second) {
	uint32_t days = epoch / 86400U;
	uint32_t secs = epoch % 86400U;

	*hour = (uint8_t)(secs / 3600U);
	*minute = (uint8_t)((secs % 3600U) / 60U);
	*second = (uint8_t)(secs % 60U);

	uint16_t y = 1970U;
	while (days >= 365U) {
		uint32_t dy = 365U;
		if ((y % 4U == 0U) && ((y % 100U != 0U) || (y % 400U == 0U))) {
			dy = 366U;
		}
		if (days < dy) {
			break;
		}
		days -= dy;
		y++;
	}

	*year = y;
	uint8_t m = 1U;
	while (m <= 12U) {
		uint32_t dm = days_in_month(y, m);
		if (days < dm) {
			break;
		}
		days -= dm;
		m++;
	}
	*month = m;
	*day = (uint8_t)(days + 1U);
}

void db_cosem_clock_set_datetime(const uint8_t *dt12, db_ic_inst_t *inst) {
	if (inst == NULL || inst->data == NULL) {
		return;
	}

	db_cosem_clock_data_t *clk = (db_cosem_clock_data_t *)inst->data;

	if (dt12[0] == 0xFFU) {
		clk->year = 2000U + (uint16_t)dt12[1];
	} else {
		clk->year = ((uint16_t)dt12[0] << 8) | (uint16_t)dt12[1];
	}

	clk->month = dt12[2];
	clk->day_of_week = dt12[3];
	clk->day = dt12[4];
	clk->hour = dt12[5];
	clk->minute = dt12[6];
	clk->second = dt12[7];
	clk->hundredths = (dt12[8] == 0xFFU) ? 0U : dt12[8];
	clk->deviation = (int16_t)(((uint16_t)dt12[9] << 8) | (uint16_t)dt12[10]);
	clk->status = dt12[11];

	if (clk->day_of_week == 0U) {
		clk->day_of_week = calc_day_of_week(clk->year, clk->month, clk->day);
	}
}

void db_cosem_clock_get_datetime(uint8_t *dt12, const db_ic_inst_t *inst) {
	if (inst == NULL || inst->data == NULL) {
		memset(dt12, 0xFFU, DB_CLOCK_DT_LEN);
		return;
	}

	const db_cosem_clock_data_t *clk = (const db_cosem_clock_data_t *)inst->data;

	if (clk->year >= 200U) {
		dt12[0] = (uint8_t)(clk->year >> 8);
	} else {
		dt12[0] = 0xFFU;
	}
	dt12[1] = (uint8_t)(clk->year & 0xFFU);
	dt12[2] = clk->month;
	dt12[3] = clk->day_of_week;
	dt12[4] = clk->day;
	dt12[5] = clk->hour;
	dt12[6] = clk->minute;
	dt12[7] = clk->second;
	dt12[8] = (clk->hundredths == 0U) ? 0xFFU : clk->hundredths;
	dt12[9] = (uint8_t)((uint16_t)clk->deviation >> 8);
	dt12[10] = (uint8_t)((uint16_t)clk->deviation & 0xFFU);
	dt12[11] = clk->status;
}

void db_cosem_clock_set_epoch(uint32_t seconds) {
	uint16_t year;
	uint8_t month, day, hour, minute, second;

	broken_from_epoch(seconds, &year, &month, &day, &hour, &minute, &second);

	CSM_LOG("[Clock] Epoch %lu = %u-%02u-%02u %02u:%02u:%02u", (unsigned long)seconds, year, month, day, hour, minute, second);

	(void)year;
	(void)month;
	(void)day;
	(void)hour;
	(void)minute;
	(void)second;
}

uint32_t db_cosem_clock_get_epoch(const db_ic_inst_t *inst) {
	if (inst == NULL || inst->data == NULL) {
		return 0U;
	}

	const db_cosem_clock_data_t *clk = (const db_cosem_clock_data_t *)inst->data;

	return epoch_from_broken(clk->year, clk->month, clk->day, clk->hour, clk->minute, clk->second);
}
