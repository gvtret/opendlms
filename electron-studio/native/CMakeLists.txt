cmake_minimum_required(VERSION 3.10)
project(opendlms-native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Node.js
include_directories(${CMAKE_JS_INC})

# Add cosemlib
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../cosemlib ${CMAKE_BINARY_DIR}/cosemlib)

# Lua 5.3 (embedded from studio/lua/src)
set(LUA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../studio/lua/src)
file(GLOB LUA_SOURCES ${LUA_DIR}/*.c)
# Exclude standalone interpreters
list(FILTER LUA_SOURCES EXCLUDE REGEX "lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "luac\\.c$")

add_library(lua53 STATIC ${LUA_SOURCES})
target_include_directories(lua53 PUBLIC ${LUA_DIR})
target_compile_definitions(lua53 PRIVATE LUA_USE_POSIX)
if(WIN32)
    target_compile_definitions(lua53 PRIVATE LUA_USE_WINDOWS)
endif()

# Native addon
add_library(${PROJECT_NAME} SHARED
    src/binding.cpp
    src/server_wrap.cpp
    src/client_wrap.cpp
    src/transport_wrap.cpp
    src/lua_bridge.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    cosemlib
    lua53
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
