/**
 * \file lua_bridge.cpp
 * \brief Lua scripting bridge implementation
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include "lua_bridge.h"
#include "csm_block_transfer.h"
#include <cstring>
#include <cstdio>

/* ── Helper: get bridge context from Lua state ───────────────────────────── */

static lua_bridge_t *get_bridge(lua_State *L)
{
    lua_bridge_t *bridge = nullptr;
    lua_getglobal(L, "__bridge");
    if (lua_islightuserdata(L, -1))
    {
        bridge = (lua_bridge_t *)lua_touserdata(L, -1);
    }
    lua_pop(L, 1);
    return bridge;
}

/* ── Lua function: print(...) ────────────────────────────────────────────── */

static int lua_print(lua_State *L)
{
    lua_bridge_t *bridge = get_bridge(L);
    int n = lua_gettop(L);

    for (int i = 1; i <= n; i++)
    {
        const char *s = lua_tostring(L, i);
        if (s)
        {
            if (bridge && bridge->print_len < sizeof(bridge->print_buf) - 1)
            {
                if (i > 1 && bridge->print_len < sizeof(bridge->print_buf) - 1)
                {
                    bridge->print_buf[bridge->print_len++] = '\t';
                }
                uint32_t slen = (uint32_t)strlen(s);
                uint32_t space = sizeof(bridge->print_buf) - 1 - bridge->print_len;
                if (slen > space) slen = space;
                memcpy(bridge->print_buf + bridge->print_len, s, slen);
                bridge->print_len += slen;
            }
        }
        else
        {
            if (bridge && bridge->print_len < sizeof(bridge->print_buf) - 4)
            {
                memcpy(bridge->print_buf + bridge->print_len, "[?]", 3);
                bridge->print_len += 3;
            }
        }
    }

    /* Add newline */
    if (bridge && bridge->print_len < sizeof(bridge->print_buf) - 1)
    {
        bridge->print_buf[bridge->print_len++] = '\n';
    }

    /* Also print to stdout for debug */
    for (int i = 1; i <= n; i++)
    {
        const char *s = lua_tostring(L, i);
        if (s)
        {
            if (i > 1) printf("\t");
            printf("%s", s);
        }
        else
        {
            printf("[?]");
        }
    }
    printf("\n");
    fflush(stdout);

    return 0;
}

/* ── Lua function: delay(ms) ─────────────────────────────────────────────── */

static int lua_delay(lua_State *L)
{
    int ms = (int)luaL_checkinteger(L, 1);
    /* Simple busy-wait (good enough for scripting) */
    /* In production, use uv_sleep or similar */
#ifdef _WIN32
    #include <windows.h>
    Sleep(ms);
#else
    #include <unistd.h>
    usleep(ms * 1000);
#endif
    return 0;
}

/* ── Lua function: hex(data) ─────────────────────────────────────────────── */

static int lua_hex(lua_State *L)
{
    size_t len = 0;
    const char *data = luaL_checklstring(L, 1, &len);

    /* Each byte -> 2 hex chars + space */
    char *buf = (char *)malloc(len * 3 + 1);
    if (!buf) return luaL_error(L, "out of memory");

    char *p = buf;
    for (size_t i = 0; i < len; i++)
    {
        if (i > 0) *p++ = ' ';
        sprintf(p, "%02X", (unsigned char)data[i]);
        p += 2;
    }
    *p = '\0';

    lua_pushstring(L, buf);
    free(buf);
    return 1;
}

/* ── Lua function: obis(str) ─────────────────────────────────────────────── */

static int lua_obis(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);

    lua_newtable(L);

    int idx = 0;
    const char *p = str;
    while (*p && idx < 6)
    {
        int val = atoi(p);
        lua_pushinteger(L, val);
        lua_rawseti(L, -2, idx + 1);

        /* Skip to next dot or end */
        while (*p && *p != '.') p++;
        if (*p == '.') p++;
        idx++;
    }

    return 1;
}

/* ── Lua function: connect(host, port) ───────────────────────────────────── */

static int lua_connect(lua_State *L)
{
    lua_bridge_t *bridge = get_bridge(L);
    if (!bridge)
    {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "bridge not initialized");
        return 2;
    }

    const char *host = luaL_checkstring(L, 1);
    int port = (int)luaL_optinteger(L, 2, 4056);

    /* Store connection params */
    strncpy(bridge->host, host, sizeof(bridge->host) - 1);
    bridge->port = (uint16_t)port;

    /* Create client if not exists */
    if (!bridge->client)
    {
        bridge->client = new csm_client();
        memset(bridge->client, 0, sizeof(csm_client));
    }

    /* Create transport if not exists */
    if (!bridge->transport)
    {
        bridge->transport = csm_transport_tcp_create();
    }

    /* Open connection */
    int result = csm_transport_tcp_open(bridge->transport, 0, bridge->host, bridge->port);
    if (result != 0)
    {
        snprintf(bridge->last_error, sizeof(bridge->last_error),
                 "Failed to connect to %s:%d", host, port);
        lua_pushboolean(L, 0);
        lua_pushstring(L, bridge->last_error);
        return 2;
    }

    /* Initialize client with transport */
    /* TODO: proper init with transport binding */
    bridge->connected = 1;

    lua_pushboolean(L, 1);
    return 1;
}

/* ── Lua function: disconnect() ──────────────────────────────────────────── */

static int lua_disconnect(lua_State *L)
{
    lua_bridge_t *bridge = get_bridge(L);
    if (!bridge)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (bridge->connected && bridge->transport)
    {
        csm_transport_tcp_close(bridge->transport, 0);
        bridge->connected = 0;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* ── Lua function: getCosem(classId, obis, attrId) ───────────────────────── */

static int lua_get_cosem(lua_State *L)
{
    lua_bridge_t *bridge = get_bridge(L);
    if (!bridge || !bridge->connected)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }

    int class_id = (int)luaL_checkinteger(L, 1);

    /* Parse OBIS from table */
    csm_obis_code obis;
    memset(&obis, 0, sizeof(obis));
    if (lua_istable(L, 2))
    {
        for (int i = 0; i < 6; i++)
        {
            lua_rawgeti(L, 2, i + 1);
            ((uint8_t *)&obis.A)[i] = (uint8_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
        }
    }

    int attr_id = (int)luaL_checkinteger(L, 3);

    /* Send GET request */
    uint8_t resp[2048];
    int len = csm_client_get_block(bridge->client, 1, (uint16_t)class_id,
                                    &obis, (uint8_t)attr_id,
                                    resp, sizeof(resp));

    if (len <= 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "GET failed");
        return 2;
    }

    /* Push data as Lua string (binary) */
    lua_pushlstring(L, (const char *)resp, (size_t)len);
    return 1;
}

/* ── Lua function: setCosem(classId, obis, attrId, data) ─────────────────── */

static int lua_set_cosem(lua_State *L)
{
    lua_bridge_t *bridge = get_bridge(L);
    if (!bridge || !bridge->connected)
    {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "not connected");
        return 2;
    }

    int class_id = (int)luaL_checkinteger(L, 1);

    /* Parse OBIS from table */
    csm_obis_code obis;
    memset(&obis, 0, sizeof(obis));
    if (lua_istable(L, 2))
    {
        for (int i = 0; i < 6; i++)
        {
            lua_rawgeti(L, 2, i + 1);
            ((uint8_t *)&obis.A)[i] = (uint8_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
        }
    }

    int attr_id = (int)luaL_checkinteger(L, 3);

    /* Get data as binary string */
    size_t data_len = 0;
    const char *data = luaL_checklstring(L, 4, &data_len);

    /* Send SET request */
    uint8_t resp[512];
    int len = csm_client_set_block(bridge->client, 2, (uint16_t)class_id,
                                    &obis, (uint8_t)attr_id,
                                    (const uint8_t *)data, (uint32_t)data_len,
                                    resp, sizeof(resp));

    if (len <= 0)
    {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "SET failed");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* ── Lua function: action(classId, obis, methodId, data) ─────────────────── */

static int lua_action(lua_State *L)
{
    lua_bridge_t *bridge = get_bridge(L);
    if (!bridge || !bridge->connected)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }

    int class_id = (int)luaL_checkinteger(L, 1);

    csm_obis_code obis;
    memset(&obis, 0, sizeof(obis));
    if (lua_istable(L, 2))
    {
        for (int i = 0; i < 6; i++)
        {
            lua_rawgeti(L, 2, i + 1);
            ((uint8_t *)&obis.A)[i] = (uint8_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
        }
    }

    int method_id = (int)luaL_checkinteger(L, 3);

    size_t data_len = 0;
    const char *data = luaL_optlstring(L, 4, "", &data_len);

    uint8_t resp[512];
    int len = csm_client_action(bridge->client, 3, (uint16_t)class_id,
                                 &obis, (uint8_t)method_id,
                                 (const uint8_t *)data, (uint32_t)data_len,
                                 resp, sizeof(resp));

    if (len <= 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "ACTION failed");
        return 2;
    }

    lua_pushlstring(L, (const char *)resp, (size_t)len);
    return 1;
}

/* ── Lua function: getObjectList() ──────────────────────────────────────── */

static int lua_get_object_list(lua_State *L)
{
    lua_bridge_t *bridge = get_bridge(L);
    if (!bridge || !bridge->connected)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }

    /* GET Attribute 2 (object list) from Association object 0.0.40.0.0.255 */
    csm_obis_code obis;
    memset(&obis, 0, sizeof(obis));
    obis.A = 0; obis.B = 0; obis.C = 40; obis.D = 0; obis.E = 0; obis.F = 255;

    uint8_t resp[4096];
    int len = csm_client_get_block(bridge->client, 1, 1, &obis, 2, resp, sizeof(resp));

    if (len <= 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "failed to get object list");
        return 2;
    }

    /* Return as binary string — caller should parse AXDR */
    lua_pushlstring(L, (const char *)resp, (size_t)len);
    return 1;
}

/* ── Lua function: getClockOBIS() ───────────────────────────────────────── */

static int lua_get_clock(lua_State *L)
{
    lua_bridge_t *bridge = get_bridge(L);
    if (!bridge || !bridge->connected)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }

    /* GET Attribute 2 (time) from Clock object 0.0.1.0.0.255 */
    csm_obis_code obis;
    memset(&obis, 0, sizeof(obis));
    obis.A = 0; obis.B = 0; obis.C = 1; obis.D = 0; obis.E = 0; obis.F = 255;

    uint8_t resp[512];
    int len = csm_client_get_block(bridge->client, 1, 8, &obis, 2, resp, sizeof(resp));

    if (len <= 0)
    {
        lua_pushnil(L);
        lua_pushstring(L, "failed to get clock");
        return 2;
    }

    lua_pushlstring(L, (const char *)resp, (size_t)len);
    return 1;
}

/* ── Bridge init/destroy ─────────────────────────────────────────────────── */

int lua_bridge_init(lua_bridge_t *bridge)
{
    if (!bridge) return -1;

    memset(bridge, 0, sizeof(lua_bridge_t));

    bridge->L = luaL_newstate();
    if (!bridge->L) return -1;

    /* Open standard Lua libraries (except os, io for security) */
    luaL_openlibs(bridge->L);

    /* Remove os.execute, io.popen for security */
    lua_pushnil(bridge->L);
    lua_setglobal(bridge->L, "os");
    lua_pushnil(bridge->L);
    lua_setglobal(bridge->L, "io");

    /* Store bridge pointer in Lua */
    lua_pushlightuserdata(bridge->L, bridge);
    lua_setglobal(bridge->L, "__bridge");

    /* Register DLMS/COSEM functions */
    lua_register(bridge->L, "connect", lua_connect);
    lua_register(bridge->L, "disconnect", lua_disconnect);
    lua_register(bridge->L, "getCosem", lua_get_cosem);
    lua_register(bridge->L, "setCosem", lua_set_cosem);
    lua_register(bridge->L, "action", lua_action);
    lua_register(bridge->L, "getObjectList", lua_get_object_list);
    lua_register(bridge->L, "getClock", lua_get_clock);
    lua_register(bridge->L, "delay", lua_delay);
    lua_register(bridge->L, "hex", lua_hex);
    lua_register(bridge->L, "obis", lua_obis);

    /* Override print */
    lua_register(bridge->L, "print", lua_print);

    return 0;
}

void lua_bridge_destroy(lua_bridge_t *bridge)
{
    if (!bridge) return;

    if (bridge->connected && bridge->transport)
    {
        csm_transport_tcp_close(bridge->transport, 0);
    }

    if (bridge->transport)
    {
        csm_transport_tcp_destroy(bridge->transport);
        bridge->transport = nullptr;
    }

    if (bridge->client)
    {
        csm_client_destroy(bridge->client);
        delete bridge->client;
        bridge->client = nullptr;
    }

    if (bridge->L)
    {
        lua_close(bridge->L);
        bridge->L = nullptr;
    }
}

int lua_bridge_exec(lua_bridge_t *bridge, const char *script,
                    char *result, uint32_t result_size)
{
    if (!bridge || !bridge->L || !script)
    {
        if (result) snprintf(result, result_size, "invalid bridge or script");
        return -1;
    }

    /* Clear error */
    bridge->last_error[0] = '\0';

    /* Execute script */
    int status = luaL_dostring(bridge->L, script);

    if (status != LUA_OK)
    {
        const char *err = lua_tostring(bridge->L, -1);
        if (err)
        {
            strncpy(bridge->last_error, err, sizeof(bridge->last_error) - 1);
            if (result) strncpy(result, err, result_size - 1);
        }
        lua_pop(bridge->L, 1);
        return -1;
    }

    if (result) result[0] = '\0';
    return 0;
}

int lua_bridge_exec_file(lua_bridge_t *bridge, const char *filename,
                         char *result, uint32_t result_size)
{
    if (!bridge || !bridge->L || !filename)
    {
        if (result) snprintf(result, result_size, "invalid bridge or filename");
        return -1;
    }

    bridge->last_error[0] = '\0';

    int status = luaL_dofile(bridge->L, filename);

    if (status != LUA_OK)
    {
        const char *err = lua_tostring(bridge->L, -1);
        if (err)
        {
            strncpy(bridge->last_error, err, sizeof(bridge->last_error) - 1);
            if (result) strncpy(result, err, result_size - 1);
        }
        lua_pop(bridge->L, 1);
        return -1;
    }

    if (result) result[0] = '\0';
    return 0;
}

int lua_bridge_exec_return(lua_bridge_t *bridge, const char *script,
                           char *result, uint32_t result_size)
{
    if (!bridge || !bridge->L || !script)
    {
        if (result) snprintf(result, result_size, "invalid bridge or script");
        return -1;
    }

    bridge->last_error[0] = '\0';

    /* Wrap script in a function that returns the value */
    char wrapped[8192];
    snprintf(wrapped, sizeof(wrapped), "return (function() %s end)()", script);

    int status = luaL_dostring(bridge->L, wrapped);

    if (status != LUA_OK)
    {
        const char *err = lua_tostring(bridge->L, -1);
        if (err)
        {
            strncpy(bridge->last_error, err, sizeof(bridge->last_error) - 1);
            if (result) strncpy(result, err, result_size - 1);
        }
        lua_pop(bridge->L, 1);
        return -1;
    }

    /* Read the return value from the stack */
    if (result && result_size > 0)
    {
        int t = lua_type(bridge->L, -1);
        switch (t)
        {
        case LUA_TSTRING:
        {
            size_t len = 0;
            const char *s = lua_tolstring(bridge->L, -1, &len);
            if (len >= result_size) len = result_size - 1;
            memcpy(result, s, len);
            result[len] = '\0';
            break;
        }
        case LUA_TNUMBER:
            snprintf(result, result_size, "%g", lua_tonumber(bridge->L, -1));
            break;
        case LUA_TBOOLEAN:
            snprintf(result, result_size, "%s", lua_toboolean(bridge->L, -1) ? "true" : "false");
            break;
        case LUA_TNIL:
            result[0] = '\0';
            break;
        default:
            snprintf(result, result_size, "[%s]", lua_typename(bridge->L, t));
            break;
        }
    }

    lua_pop(bridge->L, 1);
    return 0;
}

const char *lua_bridge_get_error(lua_bridge_t *bridge)
{
    if (!bridge) return "null bridge";
    return bridge->last_error;
}

uint32_t lua_bridge_get_output(lua_bridge_t *bridge, char *output, uint32_t output_size)
{
    if (!bridge || !output || output_size == 0) return 0;

    uint32_t len = bridge->print_len;
    if (len >= output_size) len = output_size - 1;

    if (len > 0)
    {
        memcpy(output, bridge->print_buf, len);
    }
    output[len] = '\0';

    /* Clear buffer */
    bridge->print_len = 0;
    bridge->print_buf[0] = '\0';

    return len;
}

void lua_bridge_clear_output(lua_bridge_t *bridge)
{
    if (bridge)
    {
        bridge->print_len = 0;
        bridge->print_buf[0] = '\0';
    }
}
