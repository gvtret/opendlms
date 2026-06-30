
extern "C"
{
    #include "csm_array.h"
}
#include "catch.hpp"

#include <cstring>

static const uint8_t basic_array[10] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U};


void csm_array_basic_test(void)
{
    uint32_t size = sizeof(basic_array);
    csm_array array;

    csm_array_init(&array, (uint8_t*)&basic_array[0], size, size - 3U, 0U);

    REQUIRE(size == array.size);

    uint32_t remaining = csm_array_unread(&array);
    REQUIRE(7 == remaining);

    for (uint32_t i = 0U; i < remaining; i++)
    {
        uint8_t byte;
        uint8_t ret = csm_array_get(&array, i, &byte);
        REQUIRE(TRUE == ret);
        REQUIRE(basic_array[i] == byte);
    }

    // Now test with the offset parameter
    uint32_t offset = 4U;
    csm_array_init(&array, (uint8_t*)&basic_array[0], size, size - 3U, offset);

    remaining = csm_array_unread(&array);
    REQUIRE(6 == remaining);

    for (uint32_t i = 0U; i < remaining; i++)
    {
        uint8_t byte;
        uint8_t ret = csm_array_get(&array, i, &byte);
        REQUIRE(TRUE == ret);
        REQUIRE(basic_array[i+offset] == byte);
    }

    //csm_array_dump(&array2);
}

// This test case tries to go out of bounds and see if everything's well protected
void over_limits(void)
{
    uint32_t size = sizeof(basic_array);
    csm_array array;
    uint8_t buffer[4] = {1U, 2U, 3U, 4U};
    uint8_t out[4] = {};

    csm_array_init(&array, buffer, sizeof(buffer), sizeof(buffer), 0U);

    REQUIRE(TRUE == csm_array_read_buff(&array, out, sizeof(out)));
    REQUIRE(sizeof(buffer) == array.rd_index);
    REQUIRE(0 == std::memcmp(buffer, out, sizeof(buffer)));

    csm_array_init(&array, buffer, sizeof(buffer), 2U, 0U);
    REQUIRE(FALSE == csm_array_read_buff(&array, out, 3U));
    REQUIRE(0U == array.rd_index);

    csm_array_init(&array, buffer, sizeof(buffer), 0U, 0U);
    REQUIRE(TRUE == csm_array_write_u32(&array, 0x01020304U));
    REQUIRE(sizeof(buffer) == array.wr_index);
    REQUIRE(FALSE == csm_array_write_u8(&array, 0x05U));
}

void null_safety(void)
{
    csm_array array;
    uint8_t buffer[2] = {0xAAU, 0x55U};
    uint8_t byte = 0xFFU;

    csm_array_init(&array, buffer, sizeof(buffer), sizeof(buffer), 0U);
    REQUIRE(FALSE == csm_array_get(NULL, 0U, &byte));
    REQUIRE(0xFFU == byte);
    REQUIRE(FALSE == csm_array_get(&array, 0U, NULL));

    REQUIRE(FALSE == csm_array_get(&array, sizeof(buffer), &byte));
    REQUIRE(0U == byte);

    REQUIRE(FALSE == csm_array_set(NULL, 0U, 0x11U));
    REQUIRE(FALSE == csm_array_set(&array, sizeof(buffer), 0x11U));
    REQUIRE(TRUE == csm_array_set(&array, 1U, 0x11U));
    REQUIRE(0x11U == buffer[1]);

    REQUIRE(FALSE == csm_array_read_buff(NULL, buffer, 1U));
    REQUIRE(FALSE == csm_array_read_buff(&array, NULL, 1U));
    REQUIRE(FALSE == csm_array_write_buff(NULL, buffer, 1U));
    REQUIRE(FALSE == csm_array_write_buff(&array, NULL, 1U));
    REQUIRE(TRUE == csm_array_write_buff(&array, NULL, 0U));

    REQUIRE(FALSE == csm_array_writer_jump(NULL, 1U));
    REQUIRE(FALSE == csm_array_reader_jump(NULL, 1U));
    REQUIRE(0U == csm_array_unread(NULL));
    REQUIRE(0U == csm_array_free_size(NULL));
    REQUIRE(0U == csm_array_written(NULL));
    REQUIRE(csm_array_rd_data(NULL) == nullptr);
    REQUIRE(csm_array_wr_data(NULL) == nullptr);

    uint16_t value16 = 0U;
    uint32_t value32 = 0U;
    REQUIRE(FALSE == csm_array_read_u8(NULL, &byte));
    REQUIRE(FALSE == csm_array_read_u8(&array, NULL));
    REQUIRE(FALSE == csm_array_read_u16(NULL, &value16));
    REQUIRE(FALSE == csm_array_read_u16(&array, NULL));
    REQUIRE(FALSE == csm_array_read_u32(NULL, &value32));
    REQUIRE(FALSE == csm_array_read_u32(&array, NULL));
    csm_array_dump(NULL);

    csm_array_init(NULL, buffer, sizeof(buffer), 0U, 0U);

    csm_array_init(&array, NULL, sizeof(buffer), 0U, 0U);
    REQUIRE(0U == csm_array_free_size(&array));
    REQUIRE(FALSE == csm_array_write_u8(&array, 0x01U));
    REQUIRE(csm_array_wr_data(&array) == nullptr);

    csm_array_init(&array, buffer, sizeof(buffer), 0U, sizeof(buffer) + 1U);
    REQUIRE(0U == csm_array_free_size(&array));
    REQUIRE(FALSE == csm_array_write_u8(&array, 0x01U));
    REQUIRE(csm_array_rd_data(&array) == nullptr);

    array.size = 1U;
    array.offset = 2U;
    array.wr_index = 0U;
    REQUIRE(0U == csm_array_free_size(&array));
    REQUIRE(0U == csm_array_written(&array));

    csm_array_init(&array, buffer, sizeof(buffer), 0U, 0U);
    REQUIRE(FALSE == csm_array_writer_jump(&array, UINT32_MAX));
    REQUIRE(sizeof(buffer) == array.wr_index);
    REQUIRE(FALSE == csm_array_write_buff(&array, buffer, UINT32_MAX));

    csm_array_init(&array, buffer, sizeof(buffer), sizeof(buffer), 0U);
    REQUIRE(FALSE == csm_array_reader_jump(&array, UINT32_MAX));
    REQUIRE(array.rd_index == array.wr_index);
}


TEST_CASE( "Cosem: array utility tests", "[csm_array_tests]" )
{
    csm_array_basic_test();
    over_limits();
    null_safety();
}
