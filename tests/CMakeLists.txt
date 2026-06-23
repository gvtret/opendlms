cmake_minimum_required(VERSION 3.10)
project(cosemtest LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()

add_executable(${PROJECT_NAME}
    main.cpp
    test_csm_array.cpp
    test_hdlc.cpp
    test_association.cpp
    test_gost.cpp
    test_integration.cpp
    test_transport.cpp
    cosem_tests_hal.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PUBLIC cosemlib cosemserver)

enable_testing()
add_test(NAME cosemtest COMMAND ${PROJECT_NAME})
