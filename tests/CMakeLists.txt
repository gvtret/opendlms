cmake_minimum_required(VERSION 3.10)
project(cosemtest LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()

add_executable(${PROJECT_NAME}
    main.cpp
    test_cosemlib.cpp
    test_cosem_read_by_block.cpp
    test_csm_array.cpp
    test_hdlc.cpp
    test_association.cpp
    test_aes128gcm.cpp
    test_block_transfer.cpp
    test_clock.cpp
    test_gost.cpp
    test_integration.cpp
    test_transport.cpp
    test_streebog_debug.cpp
    test_reader_auth.cpp
    ../client/lib/opendlms_reader_auth.c
    cosem_tests_hal.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC cosemlib cosemserver)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32)
endif()

enable_testing()
add_test(NAME cosemtest COMMAND ${PROJECT_NAME})

if(OPEN_DLMS_BUILD_CLIENT)
    add_executable(reader_hal_smoke
        reader_hal_smoke.c
        ../client/lib/reader_hal.c
    )
    target_include_directories(reader_hal_smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
    )
    target_link_libraries(reader_hal_smoke PUBLIC cosemlib)
    add_test(NAME reader_hal_smoke COMMAND reader_hal_smoke)
endif()
