/**
 * \file lua_bridge.cpp
 * \brief Lua scripting bridge implementation
 */

#include "lua_bridge.h"
#include "csm_transport_tcp.h"
#include "csm_framing.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static lua_bridge_t *get_bridge(lua_State *L)
{
    lua_bridge_t *bridge = nullptr;
    lua_getglobal(L, "__bridge");
    if (lua_islightuserdata(L, -1))
        bridge = (lua_bridge_t *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return bridge;
}

static void append_print(lua_bridge_t *b, const char *s, uint32_t len)
{
    if (!b || b->print_len + len >= sizeof(b->print_buf)) return;
    memcpy(b->print_buf + b->print_len, s, len);
    b->print_len += len;
}

/* ── Lua: print(...) ────────────────────────────────────────────────────── */

static int lua_print(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++)
    {
        const char *s = lua_tostring(L, i);
        if (s)
        {
            uint32_t len = (uint32_t)strlen(s);
            if (i > 1 && b) append_print(b, "\t", 1);
            if (b) append_print(b, s, len);
        }
        else
        {
            if (b) append_print(b, "[?]", 3);
        }
    }
    if (b) append_print(b, "\n", 1);
    return 0;
}

/* ── Lua: delay(ms) ─────────────────────────────────────────────────────── */

static int lua_delay(lua_State *L)
{
    int ms = (int)luaL_checkinteger(L, 1);
    (void)ms;
    /* no-op for now */
    return 0;
}

/* ── Lua: hex(data) ─────────────────────────────────────────────────────── */

static int lua_hex(lua_State *L)
{
    size_t len = 0;
    const char *data = luaL_checklstring(L, 1, &len);
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

/* ── Lua: obis(str) ─────────────────────────────────────────────────────── */

static int lua_obis(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    lua_newtable(L);
    int idx = 0;
    const char *p = str;
    while (*p && idx < 6)
    {
        lua_pushinteger(L, atoi(p));
        lua_rawseti(L, -2, idx + 1);
        while (*p && *p != '.') p++;
        if (*p == '.') p++;
        idx++;
    }
    return 1;
}

/* ── Lua: connect(host, port) ───────────────────────────────────────────── */

static int lua_connect(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (!b) { lua_pushboolean(L, 0); lua_pushstring(L, "no bridge"); return 2; }

    const char *host = luaL_checkstring(L, 1);
    int port = (int)luaL_optinteger(L, 2, 4056);

    strncpy(b->host, host, sizeof(b->host) - 1);
    b->port = (uint16_t)port;

    /* Initialize transport */
    csm_transport tcp;
    if (csm_transport_tcp_client_init(&tcp, host, (uint16_t)port, CSM_FRAMING_WRAPPER) != 0)
    {
        snprintf(b->last_error, sizeof(b->last_error), "Failed to init transport to %s:%d", host, port);
        lua_pushboolean(L, 0);
        lua_pushstring(L, b->last_error);
        return 2;
    }

    /* Connect */
    if (CSM_TRANSPORT_OPEN(&tcp, 0) != 0)
    {
        snprintf(b->last_error, sizeof(b->last_error), "Failed to connect to %s:%d", host, port);
        csm_transport_tcp_destroy(&tcp);
        lua_pushboolean(L, 0);
        lua_pushstring(L, b->last_error);
        return 2;
    }

    b->connected = 1;
    lua_pushboolean(L, 1);
    return 1;
}

/* ── Lua: disconnect() ──────────────────────────────────────────────────── */

static int lua_disconnect(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (b) b->connected = 0;
    lua_pushboolean(L, 1);
    return 1;
}

/* ── Lua: getCosem(classId, obis, attrId) ───────────────────────────────── */

static int lua_get_cosem(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (!b || !b->connected)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }
    /* Placeholder — actual COSEM client not wired yet */
    lua_pushnil(L);
    lua_pushstring(L, "getCosem not implemented yet");
    return 2;
}

/* ── Lua: setCosem(classId, obis, attrId, data) ─────────────────────────── */

static int lua_set_cosem(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (!b || !b->connected)
    {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "not connected");
        return 2;
    }
    lua_pushboolean(L, 0);
    lua_pushstring(L, "setCosem not implemented yet");
    return 2;
}

/* ── Lua: action(classId, obis, methodId, data) ─────────────────────────── */

static int lua_action(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (!b || !b->connected)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }
    lua_pushnil(L);
    lua_pushstring(L, "action not implemented yet");
    return 2;
}

/* ── Bridge init/destroy ─────────────────────────────────────────────────── */

int lua_bridge_init(lua_bridge_t *bridge)
{
    if (!bridge) return -1;
    memset(bridge, 0, sizeof(lua_bridge_t));

    bridge->L = luaL_newstate();
    if (!bridge->L) return -1;

    luaL_openlibs(bridge->L);

    /* Remove os, io for security */
    lua_pushnil(bridge->L);
    lua_setglobal(bridge->L, "os");
    lua_pushnil(bridge->L);
    lua_setglobal(bridge->L, "io");

    lua_pushlightuserdata(bridge->L, bridge);
    lua_setglobal(bridge->L, "__bridge");

    lua_register(bridge->L, "connect", lua_connect);
    lua_register(bridge->L, "disconnect", lua_disconnect);
    lua_register(bridge->L, "getCosem", lua_get_cosem);
    lua_register(bridge->L, "setCosem", lua_set_cosem);
    lua_register(bridge->L, "action", lua_action);
    lua_register(bridge->L, "delay", lua_delay);
    lua_register(bridge->L, "hex", lua_hex);
    lua_register(bridge->L, "obis", lua_obis);
    lua_register(bridge->L, "print", lua_print);

    return 0;
}

void lua_bridge_destroy(lua_bridge_t *bridge)
{
    if (!bridge) return;
    if (bridge->L)
    {
        lua_close(bridge->L);
        bridge->L = nullptr;
    }
}

int lua_bridge_exec(lua_bridge_t *bridge, const char *script, char *result, uint32_t result_size)
{
    if (!bridge || !bridge->L || !script)
    {
        if (result) snprintf(result, result_size, "invalid bridge or script");
        return -1;
    }
    bridge->last_error[0] = '\0';
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

int lua_bridge_exec_file(lua_bridge_t *bridge, const char *filename, char *result, uint32_t result_size)
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

int lua_bridge_exec_return(lua_bridge_t *bridge, const char *script, char *result, uint32_t result_size)
{
    if (!bridge || !bridge->L || !script)
    {
        if (result) snprintf(result, result_size, "invalid bridge or script");
        return -1;
    }
    bridge->last_error[0] = '\0';

    /* Wrap in pcall for safety */
    char wrapped[8192];
    snprintf(wrapped, sizeof(wrapped), "local ok, val = pcall(function() return %s end); if ok then return val else error(val) end", script);

    int status = luaL_dostring(bridge->L, wrapped);

    if (status != LUA_OK)
    {
        /* Try as statement without return */
        lua_pop(bridge->L, 1);
        snprintf(wrapped, sizeof(wrapped), "local ok, val = pcall(function() %s end); if not ok then error(val) end", script);
        status = luaL_dostring(bridge->L, wrapped);
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

    /* Read result from stack */
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
    if (len > 0) memcpy(output, bridge->print_buf, len);
    output[len] = '\0';
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
