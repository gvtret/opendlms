# OpenDLMS client-side targets.
cmake_minimum_required(VERSION 3.10)
project(opendlms_client LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()

add_library(opendlms_reader STATIC
    lib/opendlms_reader.c
    lib/opendlms_reader_auth.c
    lib/reader_hal.c
)

target_include_directories(opendlms_reader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/opendlms>
)

target_link_libraries(opendlms_reader PUBLIC cosemlib)

if(WIN32)
    target_link_libraries(opendlms_reader PUBLIC advapi32)
endif()

if(WIN32)
    target_link_libraries(opendlms_reader PUBLIC advapi32)
endif()

set_target_properties(opendlms_reader PROPERTIES
    PUBLIC_HEADER "include/opendlms_reader.h;include/opendlms_reader_auth.h;include/reader_hal.h"
)

include(GNUInstallDirs)

install(TARGETS opendlms_reader
    EXPORT OpenDLMSConfig
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/opendlms
)

if(OPEN_DLMS_BUILD_LEGACY_JSON_CLIENT)
    find_path(OPEN_DLMS_GURUX_JSON_INCLUDE_DIR JsonReader.h)
    find_path(OPEN_DLMS_GURUX_COMMON_INCLUDE_DIR Util.h)

    if(NOT OPEN_DLMS_GURUX_JSON_INCLUDE_DIR OR NOT OPEN_DLMS_GURUX_COMMON_INCLUDE_DIR)
        message(FATAL_ERROR
            "OPEN_DLMS_BUILD_LEGACY_JSON_CLIENT requires Gurux JSON/Common headers "
            "(JsonReader.h, JsonValue.h, JsonWriter.h, Util.h). "
            "Set OPEN_DLMS_GURUX_JSON_INCLUDE_DIR and OPEN_DLMS_GURUX_COMMON_INCLUDE_DIR "
            "or disable OPEN_DLMS_BUILD_LEGACY_JSON_CLIENT.")
    endif()

    add_executable(cosemclient
        src/main.cpp
        src/cosem_client_hal.c
        lib/AxdrPrinter.cpp
        lib/CosemClient.cpp
        lib/Transport.cpp
        lib/Configuration.cpp
    )

    target_include_directories(cosemclient PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${OPEN_DLMS_GURUX_JSON_INCLUDE_DIR}
        ${OPEN_DLMS_GURUX_COMMON_INCLUDE_DIR}
    )

    target_link_libraries(cosemclient PUBLIC cosemlib)
endif()
