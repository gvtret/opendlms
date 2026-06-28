#include "reader_hal.h"
#include "csm_definitions.h"

#include <stdint.h>

static int expect_ok(int rc)
{
    return rc == 0 ? 0 : 1;
}

static int expect_fail(int rc)
{
    return rc != 0 ? 0 : 1;
}

int main(void)
{
    uint8_t iv[12] = {0U};

    reader_hal_init();

    if (csm_hal_get_random_u8(9U, 3U) != 9U)
    {
        return 1;
    }

    if (csm_sys_gcm_init(0U, 63U, CSM_SEC_GUEK, CSM_SEC_ENCRYPT, iv, NULL, 0U) != 0)
    {
        return 2;
    }

    if (expect_ok(reader_hal_keyring_set_hex(
            48U,
            "303132333435363738393A3B3C3D3E3F",
            "00112233445566778899AABBCCDDEEFF",
            "8899aabbccddeeff0011223344556677")) != 0)
    {
        return 3;
    }

    if (expect_fail(reader_hal_keyring_set_hex(
            48U,
            "303132333435363738393A3B3C3D3E",
            NULL,
            NULL)) != 0)
    {
        return 4;
    }

    if (expect_fail(reader_hal_keyring_set_hex(
            48U,
            "303132333435363738393A3B3C3D3E3F00",
            NULL,
            NULL)) != 0)
    {
        return 5;
    }

    if (expect_fail(reader_hal_set_dedicated_key_hex(
            48U,
            "303132333435363738393A3B3C3D3E3Z")) != 0)
    {
        return 6;
    }

    if (expect_ok(reader_hal_set_dedicated_key_hex(
            48U,
            "31313131313131313131313131313131")) != 0)
    {
        return 7;
    }

    return 0;
}
