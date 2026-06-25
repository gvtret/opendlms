cmake_minimum_required(VERSION 3.10)
project(opendlms-native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Node.js
include_directories(${CMAKE_JS_INC})

# Add cosemlib
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)

# Native addon
add_library(${PROJECT_NAME} SHARED
    src/binding.cpp
    src/server_wrap.cpp
    src/client_wrap.cpp
    src/transport_wrap.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    cosemlib
    ${CMAKE_JS_LIB}
)

# Node.js addon properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    OUTPUT_NAME "opendlms-native"
)

# cmake-js integration
if(DEFINED CMAKE_JS_VERSION)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NODE_ADDON_API_ENABLE_CPP_EXCEPTIONS
    )
endif()
