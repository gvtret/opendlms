cmake_minimum_required(VERSION 3.10)

project(libmetersim LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../../../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()
if(NOT TARGET cosemserver)
    add_subdirectory(../../../server ${CMAKE_BINARY_DIR}/cosemserver)
endif()

set(METER_SIM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../examples/metersimulator)
add_executable(${PROJECT_NAME}

    libmetersim.c
    ${METER_SIM_SRC}/src/cosem_server_hal.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${METER_SIM_SRC}/src
    ${METER_SIM_SRC}/system
)

target_link_libraries(${PROJECT_NAME} PUBLIC cosemserver cosemlib)

# Emscripten options
set_target_properties(${PROJECT_NAME} PROPERTIES
    COMPILE_FLAGS "-s WASM=1"
    LINK_FLAGS "-s WASM=1 -s EXPORTED_FUNCTIONS='[\"_libmetersim_initialize\"]' -s EXPORTED_RUNTIME_METHODS='[\"ccall\", \"cwrap\"]'"
)
