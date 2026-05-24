cmake_minimum_required(VERSION 3.10)

# Nom du projet
project(libmetersim LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../../../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()
if(NOT TARGET cosemserver)
    add_subdirectory(../../../server ${CMAKE_BINARY_DIR}/cosemserver)
endif()

# Ajouter les fichiers sources
set(METER_SIM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../examples/metersimulator)
add_executable(${PROJECT_NAME}

    libmetersim.c
    ${METER_SIM_SRC}/src/cosem_server_hal.c
)

# Inclure des répertoires d'en-têtes si nécessaire
target_include_directories(${PROJECT_NAME} PRIVATE
    ${METER_SIM_SRC}/src
    ${METER_SIM_SRC}/system
)

# External libraries
target_link_libraries(${PROJECT_NAME} PUBLIC cosemserver cosemlib)

# Spécifier les options de compilation pour Emscripten
set_target_properties(${PROJECT_NAME} PROPERTIES
    COMPILE_FLAGS "-s WASM=1"
    LINK_FLAGS "-s WASM=1 -s EXPORTED_FUNCTIONS='[\"_libmetersim_initialize\"]' -s EXPORTED_RUNTIME_METHODS='[\"ccall\", \"cwrap\"]'"
)
