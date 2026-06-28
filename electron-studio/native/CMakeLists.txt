cmake_minimum_required(VERSION 3.10)
project(opendlms-native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Node.js/Electron headers and import library are provided by cmake-js.
include_directories(${CMAKE_JS_INC})
if(NOT EXISTS "${CMAKE_JS_INC}/node_api.h")
    file(GLOB CMAKE_JS_NODE_API_HEADERS "$ENV{USERPROFILE}/.cmake-js/node-*/v*/include/node/node_api.h")
    if(CMAKE_JS_NODE_API_HEADERS)
        list(GET CMAKE_JS_NODE_API_HEADERS 0 NODE_API_HEADER)
        get_filename_component(NODE_API_INCLUDE_DIR "${NODE_API_HEADER}" DIRECTORY)
        include_directories("${NODE_API_INCLUDE_DIR}")
    endif()
endif()

# node-addon-api
find_path(NAPI_INCLUDE_DIR napi.h
    HINTS $ENV{NVM_SYMLINK}/node_modules/node-addon-api
)
if(NAPI_INCLUDE_DIR)
    include_directories(${NAPI_INCLUDE_DIR})
endif()

# cosemlib
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)

# Lua 5.3
set(LUA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../lua/src)
file(GLOB LUA_SOURCES ${LUA_DIR}/*.c)
list(FILTER LUA_SOURCES EXCLUDE REGEX "lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "luac\\.c$")

add_library(lua53 STATIC ${LUA_SOURCES})
target_include_directories(lua53 PUBLIC ${LUA_DIR})
if(WIN32)
    target_compile_definitions(lua53 PRIVATE LUA_USE_WINDOWS)
else()
    target_compile_definitions(lua53 PRIVATE LUA_USE_POSIX)
endif()

# Native addon
add_library(${PROJECT_NAME} SHARED
    ${CMAKE_JS_SRC}
    src/binding.cpp
    src/lua_bridge.cpp
    src/lua_bridge_wrap.cpp
    src/transport_wrap.cpp
    src/client_wrap.cpp
    src/server_wrap.cpp
    src/data_wrap.cpp
    src/block_wrap.cpp
    src/native_hal.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    cosemlib
    lua53
    ${CMAKE_JS_LIB}
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    OUTPUT_NAME "opendlms-native"
)

if(DEFINED CMAKE_JS_VERSION)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NODE_ADDON_API_ENABLE_CPP_EXCEPTIONS
    )
endif()
