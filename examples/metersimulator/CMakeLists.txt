cmake_minimum_required(VERSION 3.10)

project(metersimulator LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()
if(NOT TARGET cosemserver)
    add_subdirectory(../../server ${CMAKE_BINARY_DIR}/cosemserver)
endif()

# Windows support
if(WIN32)
    add_definitions(-DUSE_WINDOWS_OS)
endif()

add_executable(${PROJECT_NAME}

    src/main.c
    src/cosem_server_hal.c

    ../../common/ip/tcp_server.c

    system/bsp_flash.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/system
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/ip
)

target_link_libraries(${PROJECT_NAME} PUBLIC cosemserver cosemlib)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32)
endif()
