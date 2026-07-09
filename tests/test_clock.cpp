
extern "C" {
#include "clock.h"
}
#include "catch.hpp"

TEST_CASE("Clock", "[clock_test]") {
	REQUIRE(clk_dow(2015U, 3U, 29U) == SUNDAY);
	REQUIRE(clk_last_dow(2015U, 3U, SUNDAY) == 29U);
	REQUIRE(clk_dow(2024U, 2U, 29U) == THURSDAY);
	REQUIRE(clk_last_dow(2024U, 2U, THURSDAY) == 29U);
	REQUIRE(clk_is_valid_date(2024U, 2U, 29U) == 1U);
	REQUIRE(clk_is_valid_date(2023U, 2U, 29U) == 0U);
}
