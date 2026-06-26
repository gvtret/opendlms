cmake_minimum_required(VERSION 3.10)
project(opendlms-native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Node.js headers (cmake-js downloads them)
set(CMAKE_JS_INC ${CMAKE_JS_INC} "C:/Users/trete/.cmake-js/node-x64/v24.17.0/include/node")
include_directories(${CMAKE_JS_INC})

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

# Native addon (LuaBridge only)
add_library(${PROJECT_NAME} SHARED
    src/binding.cpp
    src/lua_bridge.cpp
    src/lua_bridge_wrap.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    cosemlib
    lua53
    ${CMAKE_JS_LIB}
    "C:/Users/trete/.cmake-js/node-x64/v24.17.0/win-x64/node.lib"
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
