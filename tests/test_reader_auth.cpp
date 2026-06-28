#include "catch.hpp"
#include "opendlms_reader_auth.h"

TEST_CASE("Reader auth presets tolerate null output", "[reader]")
{
    opendlms_reader_auth_public(nullptr);
    opendlms_reader_auth_reader(nullptr);
    opendlms_reader_auth_configurator(nullptr);
    opendlms_reader_auth_configurator_plain(nullptr);
    REQUIRE(true);
}

TEST_CASE("Reader auth presets map roles to association parameters", "[reader]")
{
    opendlms_reader_auth_t auth;

    opendlms_reader_auth_public(&auth);
    REQUIRE(auth.auth_level == CSM_AUTH_LOWEST_LEVEL);
    REQUIRE(auth.application_context == LN_REF);
    REQUIRE(auth.ciphering == 0U);

    opendlms_reader_auth_reader(&auth);
    REQUIRE(auth.auth_level == CSM_AUTH_LOW_LEVEL);
    REQUIRE(auth.application_context == LN_REF);
    REQUIRE(auth.ciphering == 0U);

    opendlms_reader_auth_configurator(&auth);
    REQUIRE(auth.auth_level == CSM_AUTH_HIGH_LEVEL_GMAC);
    REQUIRE(auth.application_context == LN_REF_WITH_CYPHERING);
    REQUIRE(auth.ciphering == 1U);
    REQUIRE(auth.ic_class_id == 1U);
    REQUIRE(auth.ic_attribute_id == 2U);
    REQUIRE(auth.ic_obis.C == 43U);

    opendlms_reader_auth_configurator_plain(&auth);
    REQUIRE(auth.auth_level == CSM_AUTH_HIGH_LEVEL_GMAC);
    REQUIRE(auth.application_context == LN_REF);
    REQUIRE(auth.ciphering == 0U);
    REQUIRE(auth.ic_class_id == 1U);
    REQUIRE(auth.ic_attribute_id == 2U);
    REQUIRE(auth.ic_obis.C == 43U);
}
