# Version minimale de CMake
cmake_minimum_required(VERSION 3.10)

# Nom du projet
project(cosemserver LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()

# Ajouter les fichiers sources
add_library(${PROJECT_NAME}

    # Application level of cosem
    application/app_calendar.c
    application/app_database.c

    # Server wiring
    lib/opendlms_meter.c
    lib/db_cosem_associations.c
    lib/db_cosem_push.c
    lib/db_cosem_event_notify.c
    lib/db_cosem_catalog_legacy.c

)

# Inclure des répertoires d'en-têtes si nécessaire
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/application
    ${CMAKE_CURRENT_LIST_DIR}/database
)

# External libraries
target_link_libraries(${PROJECT_NAME} PUBLIC cosemlib)
