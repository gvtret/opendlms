#include "reader_hal.h"

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
    reader_hal_init();

    if (expect_ok(reader_hal_keyring_set_hex(
            48U,
            "303132333435363738393A3B3C3D3E3F",
            "00112233445566778899AABBCCDDEEFF",
            "8899aabbccddeeff0011223344556677")) != 0)
    {
        return 1;
    }

    if (expect_fail(reader_hal_keyring_set_hex(
            48U,
            "303132333435363738393A3B3C3D3E",
            NULL,
            NULL)) != 0)
    {
        return 2;
    }

    if (expect_fail(reader_hal_keyring_set_hex(
            48U,
            "303132333435363738393A3B3C3D3E3F00",
            NULL,
            NULL)) != 0)
    {
        return 3;
    }

    if (expect_fail(reader_hal_set_dedicated_key_hex(
            48U,
            "303132333435363738393A3B3C3D3E3Z")) != 0)
    {
        return 4;
    }

    if (expect_ok(reader_hal_set_dedicated_key_hex(
            48U,
            "31313131313131313131313131313131")) != 0)
    {
        return 5;
    }

    return 0;
}
