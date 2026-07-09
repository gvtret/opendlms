/**
 * \file test_push_loopback.cpp
 *
 * \brief Catch2 tests for TCP push loopback
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include <cstring>
#include "catch.hpp"

extern "C" {
#include "csm_array.h"
#include "csm_definitions.h"
}

TEST_CASE("Push — COSEM WPDU header encode/decode round-trip", "[push]") {
	/* Encode a minimal WPDU header: version(2) + length(2) + source(2) + dest(2) + LLC(3) */
	uint8_t buf[32];
	csm_array arr;
	csm_array_init(&arr, buf, sizeof(buf), 0, 0);

	/* WPDU version 1, source port 1, dest port 2, LLC E6 E6 00 */
	csm_array_write_u8(&arr, 0x00); /* version high */
	csm_array_write_u8(&arr, 0x01); /* version low */
	csm_array_write_u8(&arr, 0x00); /* length high */
	csm_array_write_u8(&arr, 0x0B); /* length low (11 = 3 LLC + 8 payload) */
	csm_array_write_u8(&arr, 0x00); /* source port high */
	csm_array_write_u8(&arr, 0x01); /* source port low */
	csm_array_write_u8(&arr, 0x00); /* dest port high */
	csm_array_write_u8(&arr, 0x02); /* dest port low */
	csm_array_write_u8(&arr, 0xE6); /* LLC byte 1 */
	csm_array_write_u8(&arr, 0xE6); /* LLC byte 2 */
	csm_array_write_u8(&arr, 0x00); /* LLC byte 3 */

	REQUIRE(csm_array_written(&arr) == 11U);

	/* Verify version */
	uint8_t ver_hi, ver_lo;
	csm_array_init(&arr, buf, sizeof(buf), 11, 0);
	REQUIRE(csm_array_read_u8(&arr, &ver_hi) == TRUE);
	REQUIRE(csm_array_read_u8(&arr, &ver_lo) == TRUE);
	REQUIRE(ver_hi == 0x00);
	REQUIRE(ver_lo == 0x01);
}
