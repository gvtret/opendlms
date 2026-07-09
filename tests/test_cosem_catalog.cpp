/**
 * \file test_cosem_catalog.cpp
 *
 * \brief Catch2 tests for COSEM catalog/object listing
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include <cstring>
#include "catch.hpp"

#include "csm_array.h"
#include "csm_definitions.h"

TEST_CASE("COSEM catalog — OBIS code comparison", "[catalog]") {
	csm_obis_code a = {0, 0, 1, 0, 0, 255};
	csm_obis_code b = {0, 0, 1, 0, 0, 255};
	csm_obis_code c = {0, 0, 8, 0, 0, 255};

	REQUIRE(a.A == b.A);
	REQUIRE(a.C == b.C);
	REQUIRE(a.C != c.C);
}

TEST_CASE("COSEM catalog — array basic operations", "[catalog]") {
	uint8_t buf[64];
	csm_array arr;
	csm_array_init(&arr, buf, sizeof(buf), 0, 0);

	REQUIRE(csm_array_write_u8(&arr, 0x01) == TRUE);
	REQUIRE(csm_array_write_u8(&arr, 0x02) == TRUE);
	REQUIRE(csm_array_write_u8(&arr, 0x03) == TRUE);

	REQUIRE(csm_array_written(&arr) == 3U);

	uint8_t val = 0;
	REQUIRE(csm_array_read_u8(&arr, &val) == TRUE);
	REQUIRE(val == 0x01);
	REQUIRE(csm_array_read_u8(&arr, &val) == TRUE);
	REQUIRE(val == 0x02);
	REQUIRE(csm_array_read_u8(&arr, &val) == TRUE);
	REQUIRE(val == 0x03);
}
