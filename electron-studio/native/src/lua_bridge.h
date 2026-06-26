/**
 * \file lua_bridge.h
 * \brief Lua scripting bridge for OpenDLMS Studio
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

typedef struct
{
    lua_State *L;
    char host[256];
    uint16_t port;
    int connected;
    int transport_initialized;
    csm_transport transport;
    csm_client *client;
    uint8_t invoke_id;
    char last_error[512];
    char print_buf[16384];
    uint32_t print_len;
} lua_bridge_t;

int lua_bridge_init(lua_bridge_t *bridge);
void lua_bridge_destroy(lua_bridge_t *bridge);
int lua_bridge_exec(lua_bridge_t *bridge, const char *script, char *result, uint32_t result_size);
int lua_bridge_exec_file(lua_bridge_t *bridge, const char *filename, char *result, uint32_t result_size);
int lua_bridge_exec_return(lua_bridge_t *bridge, const char *script, char *result, uint32_t result_size);
const char *lua_bridge_get_error(lua_bridge_t *bridge);
uint32_t lua_bridge_get_output(lua_bridge_t *bridge, char *output, uint32_t output_size);
void lua_bridge_clear_output(lua_bridge_t *bridge);

#ifdef __cplusplus
}
#endif

#endif /* LUA_BRIDGE_H */
