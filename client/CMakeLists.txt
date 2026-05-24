# Client tool — requires Gurux JSON library (JsonReader, JsonValue).
# Build is opt-in via OPEN_DLMS_BUILD_CLIENT.
cmake_minimum_required(VERSION 3.10)
project(cosemclient LANGUAGES C CXX)

add_subdirectory(../cosemlib cosemlib)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/cosem_client_hal.c
    lib/AxdrPrinter.cpp
    lib/CosemClient.cpp
    lib/Transport.cpp
    lib/Configuration.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(${PROJECT_NAME} PUBLIC cosemlib)
