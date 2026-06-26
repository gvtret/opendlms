# reader_lab — TCP lab meter client using opendlms_reader API
cmake_minimum_required(VERSION 3.10)
project(reader_lab LANGUAGES C)

option(OPEN_DLMS_BUILD_READER_LAB "Build reader_lab TCP client for lab meter profiles" ON)

if(NOT TARGET cosemlib)
    add_subdirectory(../../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()

if(OPEN_DLMS_BUILD_READER_LAB)
    add_executable(reader_lab
        src/main.c
        ../../client/lib/reader_hal.c
        ../../client/lib/opendlms_reader.c
        ../../client/lib/opendlms_reader_auth.c
    )

    target_link_libraries(reader_lab PRIVATE cosemlib)

    if(WIN32)
        target_link_libraries(reader_lab PRIVATE ws2_32)
    endif()

    target_include_directories(reader_lab PRIVATE
        src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../client/include
        ${COSEMLIB_INCLUDE_DIRS}
    )
endif()
