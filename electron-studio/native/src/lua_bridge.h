/**
 * \file lua_bridge.h
 * \brief Lua scripting bridge for OpenDLMS Studio
 *
 *  Embeds Lua 5.3 and exposes DLMS/COSEM functions:
 *    connect(host, port) -> true/false
 *    disconnect()
 *    getCosem(classId, obis, attrId) -> data|nil, error
 *    setCosem(classId, obis, attrId, data) -> true/false, error
 *    print(...)
 *    delay(ms)
 *    hex(data) -> string
 *    obis("A.B.C.D.E.F") -> table
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef LUA_BRIDGE_H
#define LUA_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "csm_server.h"
#include "csm_transport.h"

/**
 * \brief Lua bridge context — holds Lua state and COSEM session
 */
typedef struct
{
    lua_State *L;
    csm_client *client;
    csm_transport *transport;
    char host[256];
    uint16_t port;
    int connected;
    char last_error[512];
} lua_bridge_t;

/**
 * \brief Initialize the Lua bridge
 */
int lua_bridge_init(lua_bridge_t *bridge);

/**
 * \brief Destroy the Lua bridge
 */
void lua_bridge_destroy(lua_bridge_t *bridge);

/**
 * \brief Execute a Lua script string
 *
 * \param bridge    Bridge context
 * \param script    Lua script to execute
 * \param result    Buffer for result/error message
 * \param result_size  Size of result buffer
 * \return 0 on success, -1 on error
 */
int lua_bridge_exec(lua_bridge_t *bridge, const char *script,
                    char *result, uint32_t result_size);

/**
 * \brief Execute a Lua script file
 */
int lua_bridge_exec_file(lua_bridge_t *bridge, const char *filename,
                         char *result, uint32_t result_size);

/**
 * \brief Execute a Lua expression and return its result as a string
 *
 *  The script should evaluate to a single value (string, number, boolean, nil).
 *  For table results, use hex() or serialize manually in the script.
 *
 * \param bridge    Bridge context
 * \param script    Lua expression to evaluate
 * \param result    Buffer for the returned value as string
 * \param result_size  Size of result buffer
 * \return 0 on success, -1 on error
 */
int lua_bridge_exec_return(lua_bridge_t *bridge, const char *script,
                           char *result, uint32_t result_size);

/**
 * \brief Get the last error message
 */
const char *lua_bridge_get_error(lua_bridge_t *bridge);

#ifdef __cplusplus
}
#endif

#endif /* LUA_BRIDGE_H */
