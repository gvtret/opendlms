# Version minimale de CMake
cmake_minimum_required(VERSION 3.10)

# Nom du projet
project(metersimulator LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()
if(NOT TARGET cosemserver)
    add_subdirectory(../../server ${CMAKE_BINARY_DIR}/cosemserver)
endif()

# Ajouter les fichiers sources
add_executable(${PROJECT_NAME}

    src/main.c
    src/cosem_server_hal.c

    ../../common/ip/tcp_server.c

    system/bsp_flash.c
)

# Inclure des répertoires d'en-têtes si nécessaire
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/system
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/ip
)

# External libraries
target_link_libraries(${PROJECT_NAME} PUBLIC cosemserver cosemlib)
