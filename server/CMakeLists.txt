cmake_minimum_required(VERSION 3.10)

project(cosemserver LANGUAGES C CXX)

if(NOT TARGET cosemlib)
    add_subdirectory(../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)
endif()

add_library(${PROJECT_NAME}

    # Application level
    application/app_calendar.c
    application/app_database.c

    # Server wiring
    lib/db_cosem_push.c
    lib/db_cosem_event_notify.c
    lib/db_cosem_catalog_legacy.c

    # Database objects
    database/db_cosem_clock.c
    database/db_cosem_associations.c
    database/db_cosem_image_transfer.c

)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/application
    ${CMAKE_CURRENT_LIST_DIR}/database
)

target_link_libraries(${PROJECT_NAME} PUBLIC cosemlib)
