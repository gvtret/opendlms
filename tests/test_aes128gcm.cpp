#include "catch.hpp"
#include "gcm.h"
#include "csm_security.h"

#include <cstring>
#include <vector>

TEST_CASE("AES-128 GCM NIST tag vector", "[crypto][gcm]")
{
    const unsigned char key[16] = {
        0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
        0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08
    };
    const unsigned char iv[12] = {
        0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce,
        0xdb, 0xad, 0xde, 0xca, 0xf8, 0x88
    };
    const unsigned char plaintext[] = {
        0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5,
        0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26, 0x9a,
        0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda,
        0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72,
        0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53,
        0x2f, 0xcf, 0x0e, 0x24, 0x49, 0xa6, 0xb5, 0x25,
        0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57,
        0xba, 0x63, 0x7b, 0x39, 0x1a, 0xaf, 0xd2, 0x55
    };
    const unsigned char expected_tag[16] = {
        0x4d, 0x5c, 0x2a, 0xf3, 0x27, 0xcd, 0x64, 0xa6,
        0x2c, 0xf3, 0x5a, 0xbd, 0x2b, 0xa6, 0xfa, 0xb4
    };

    std::vector<unsigned char> ciphertext(sizeof(plaintext));
    unsigned char tag[16] = {};

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    REQUIRE(mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) == 0);
    REQUIRE(mbedtls_gcm_starts(&ctx, MBEDTLS_GCM_ENCRYPT, iv, sizeof(iv), nullptr, 0) == 0);

    uint32_t remaining = sizeof(plaintext);
    for (uint32_t offset = 0U; offset < sizeof(plaintext);)
    {
        uint32_t block_size = (remaining > 16U) ? 16U : remaining;
        REQUIRE(mbedtls_gcm_update(&ctx, block_size, plaintext + offset,
                                   ciphertext.data() + offset) == 0);
        offset += block_size;
        remaining -= block_size;
    }

    REQUIRE(mbedtls_gcm_finish(&ctx, tag, sizeof(tag)) == 0);
    REQUIRE(std::memcmp(tag, expected_tag, sizeof(expected_tag)) == 0);
    mbedtls_gcm_free(&ctx);
}

TEST_CASE("AES-128 GMAC GreenBook HLS5 tag vector", "[crypto][gcm]")
{
    const unsigned char key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    const unsigned char iv[12] = {
        0x4D, 0x4D, 0x4D, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x01
    };
    const unsigned char auth_key[16] = {
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
        0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF
    };
    const unsigned char expected_tag[12] = {
        0x1A, 0x52, 0xFE, 0x7D, 0xD3, 0xE7,
        0x27, 0x48, 0x97, 0x3C, 0x1E, 0x28
    };

    const unsigned char security_control = 0x10U;
    const unsigned char stoc[] = "P6wRJ21F";
    std::vector<unsigned char> aad(1U + sizeof(auth_key) + 8U);
    aad[0] = security_control;
    std::memcpy(&aad[1], auth_key, sizeof(auth_key));
    std::memcpy(&aad[1 + sizeof(auth_key)], stoc, 8U);

    unsigned char tag[16] = {};

    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    REQUIRE(mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) == 0);
    REQUIRE(mbedtls_gcm_starts(&ctx, MBEDTLS_GCM_ENCRYPT, iv, sizeof(iv),
                               aad.data(), aad.size()) == 0);
    REQUIRE(mbedtls_gcm_finish(&ctx, tag, sizeof(tag)) == 0);
    REQUIRE(std::memcmp(tag, expected_tag, sizeof(expected_tag)) == 0);
    mbedtls_gcm_free(&ctx);
}

TEST_CASE("Security auth helpers reject null and short-prefix inputs", "[crypto][security]")
{
    uint8_t buf[32] = {};
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 5U, 0U);
    csm_request request;
    memset(&request, 0, sizeof(request));
    request.llc.dsap = 1U;
    uint8_t system_title[8] = {};
    csm_sec_control_byte sc = {};

    REQUIRE(csm_sec_auth_decrypt(nullptr, &request, system_title) == CSM_SEC_ERROR);
    REQUIRE(csm_sec_auth_decrypt(&array, nullptr, system_title) == CSM_SEC_ERROR);
    REQUIRE(csm_sec_auth_decrypt(&array, &request, nullptr) == CSM_SEC_ERROR);
    REQUIRE(csm_sec_auth_decrypt(&array, &request, system_title) == CSM_SEC_ERROR);

    csm_array_init(&array, buf, sizeof(buf), 0U, 0U);
    REQUIRE(csm_sec_auth_encrypt(nullptr, &request, system_title, sc, 1U) == CSM_SEC_ERROR);
    REQUIRE(csm_sec_auth_encrypt(&array, nullptr, system_title, sc, 1U) == CSM_SEC_ERROR);
    REQUIRE(csm_sec_auth_encrypt(&array, &request, nullptr, sc, 1U) == CSM_SEC_ERROR);
    REQUIRE(csm_sec_auth_encrypt(&array, &request, system_title, sc, 1U) == CSM_SEC_ERROR);

    csm_array corrupt_decrypt = {};
    corrupt_decrypt.size = 5U;
    corrupt_decrypt.wr_index = 5U;
    REQUIRE(csm_sec_auth_decrypt(&corrupt_decrypt, &request, system_title) == CSM_SEC_ERROR);

    csm_array corrupt_encrypt = {};
    corrupt_encrypt.size = 20U;
    corrupt_encrypt.wr_index = 20U;
    corrupt_encrypt.rd_index = 17U;
    REQUIRE(csm_sec_auth_encrypt(&corrupt_encrypt, &request, system_title, sc, 1U) == CSM_SEC_ERROR);
}
