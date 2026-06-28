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
#include <chrono>
#include <thread>

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

static void copy_cstr(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0U) return;
    if (!src) src = "";
    size_t len = strlen(src);
    if (len >= dst_size) len = dst_size - 1U;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static int parse_obis_string(const char *s, csm_obis_code *obis)
{
    if (!s || !obis) return 0;

    uint32_t values[6] = {0U, 0U, 0U, 0U, 0U, 0U};
    uint8_t part = 0U;
    int have_digit = 0;

    while (*s != '\0')
    {
        if (*s >= '0' && *s <= '9')
        {
            have_digit = 1;
            values[part] = values[part] * 10U + (uint32_t)(*s - '0');
            if (values[part] > 255U) return 0;
        }
        else if (*s == '.')
        {
            if (!have_digit || part >= 5U) return 0;
            part++;
            have_digit = 0;
        }
        else
        {
            return 0;
        }
        s++;
    }

    if (!have_digit || part != 5U) return 0;

    obis->A = (uint8_t)values[0];
    obis->B = (uint8_t)values[1];
    obis->C = (uint8_t)values[2];
    obis->D = (uint8_t)values[3];
    obis->E = (uint8_t)values[4];
    obis->F = (uint8_t)values[5];
    return 1;
}

static void close_client(lua_bridge_t *b)
{
    if (!b) return;
    if (b->client)
    {
        if (b->connected)
        {
            csm_client_disconnect(b->client);
        }
        csm_client_delete(b->client);
        b->client = nullptr;
    }
    if (b->transport_initialized)
    {
        csm_transport_tcp_destroy(&b->transport);
        b->transport_initialized = 0;
    }
    b->connected = 0;
}

static int read_obis(lua_State *L, int index, csm_obis_code *obis)
{
    if (!obis) return 0;
    memset(obis, 0, sizeof(*obis));

    if (lua_istable(L, index))
    {
        uint8_t *parts[] = { &obis->A, &obis->B, &obis->C, &obis->D, &obis->E, &obis->F };
        for (int i = 0; i < 6; i++)
        {
            lua_rawgeti(L, index, i + 1);
            if (!lua_isinteger(L, -1))
            {
                lua_pop(L, 1);
                return 0;
            }
            lua_Integer value = lua_tointeger(L, -1);
            if (value < 0 || value > 255)
            {
                lua_pop(L, 1);
                return 0;
            }
            *parts[i] = (uint8_t)value;
            lua_pop(L, 1);
        }
        return 1;
    }

    if (lua_isstring(L, index))
    {
        const char *s = lua_tostring(L, index);
        if (parse_obis_string(s, obis))
        {
            return 1;
        }
    }

    return 0;
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
    if (ms < 0)
    {
        return luaL_error(L, "delay must be non-negative");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
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
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++)
    {
        if (i > 0) *p++ = ' ';
        unsigned char byte = (unsigned char)data[i];
        *p++ = hex[(byte >> 4) & 0x0F];
        *p++ = hex[byte & 0x0F];
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
    csm_obis_code obis;
    if (!parse_obis_string(str, &obis))
    {
        return luaL_error(L, "invalid OBIS code");
    }

    lua_newtable(L);
    uint8_t parts[] = { obis.A, obis.B, obis.C, obis.D, obis.E, obis.F };
    for (int i = 0; i < 6; i++)
    {
        lua_pushinteger(L, parts[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

/* ── Lua: connect(host, port) ───────────────────────────────────────────── */

static int lua_connect(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (!b) { lua_pushboolean(L, 0); lua_pushstring(L, "no bridge"); return 2; }

    const char *host = luaL_checkstring(L, 1);
    lua_Integer port_value = luaL_optinteger(L, 2, 4056);
    if (port_value < 1 || port_value > 65535)
    {
        snprintf(b->last_error, sizeof(b->last_error), "invalid TCP port");
        lua_pushboolean(L, 0);
        lua_pushstring(L, b->last_error);
        return 2;
    }
    int port = (int)port_value;

    copy_cstr(b->host, sizeof(b->host), host);
    b->port = (uint16_t)port;

    close_client(b);

    /* Initialize transport */
    if (csm_transport_tcp_client_init(&b->transport, host, (uint16_t)port, CSM_FRAMING_TCP_WRAPPER) != 0)
    {
        snprintf(b->last_error, sizeof(b->last_error), "Failed to init transport to %s:%d", host, port);
        lua_pushboolean(L, 0);
        lua_pushstring(L, b->last_error);
        return 2;
    }
    b->transport_initialized = 1;

    b->client = csm_client_create(&b->transport, 0, CSM_FRAMING_NONE);
    if (!b->client)
    {
        snprintf(b->last_error, sizeof(b->last_error), "Failed to create client for %s:%d", host, port);
        close_client(b);
        lua_pushboolean(L, 0);
        lua_pushstring(L, b->last_error);
        return 2;
    }

    if (csm_client_connect(b->client, 5000) != 0)
    {
        snprintf(b->last_error, sizeof(b->last_error), "Failed to connect to %s:%d", host, port);
        close_client(b);
        lua_pushboolean(L, 0);
        lua_pushstring(L, b->last_error);
        return 2;
    }

    b->connected = 1;
    b->invoke_id = 1;
    lua_pushboolean(L, 1);
    return 1;
}

/* ── Lua: disconnect() ──────────────────────────────────────────────────── */

static int lua_disconnect(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    close_client(b);
    lua_pushboolean(L, 1);
    return 1;
}

/* ── Lua: getCosem(classId, obis, attrId) ───────────────────────────────── */

static int lua_get_cosem(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (!b || !b->connected || !b->client)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }

    uint16_t class_id = (uint16_t)luaL_checkinteger(L, 1);
    csm_obis_code obis;
    if (!read_obis(L, 2, &obis))
    {
        lua_pushnil(L);
        lua_pushstring(L, "invalid OBIS");
        return 2;
    }
    uint8_t attr_id = (uint8_t)luaL_checkinteger(L, 3);

    uint8_t resp_buf[65536];
    int rc = csm_client_get_block(b->client, b->invoke_id++, class_id, &obis,
                                  attr_id, resp_buf, sizeof(resp_buf));
    if (rc < 0)
    {
        snprintf(b->last_error, sizeof(b->last_error), "GET failed: %d", rc);
        lua_pushnil(L);
        lua_pushstring(L, b->last_error);
        return 2;
    }

    lua_pushlstring(L, (const char *)resp_buf, (size_t)rc);
    return 1;
}

/* ── Lua: getObjectList() ──────────────────────────────────────────────── */

static int lua_get_object_list(lua_State *L)
{
    lua_pushinteger(L, 15);
    lua_pushstring(L, "0.0.40.0.0.255");
    lua_pushinteger(L, 2);
    return lua_get_cosem(L);
}

/* ── Lua: setCosem(classId, obis, attrId, data) ─────────────────────────── */

static int lua_set_cosem(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (!b || !b->connected || !b->client)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }

    uint16_t class_id = (uint16_t)luaL_checkinteger(L, 1);
    csm_obis_code obis;
    if (!read_obis(L, 2, &obis))
    {
        lua_pushnil(L);
        lua_pushstring(L, "invalid OBIS");
        return 2;
    }
    uint8_t attr_id = (uint8_t)luaL_checkinteger(L, 3);
    size_t data_len = 0;
    const char *data = luaL_checklstring(L, 4, &data_len);

    uint8_t resp_buf[2048];
    int rc = csm_client_set_block(b->client, b->invoke_id++, class_id, &obis, attr_id,
                                  (const uint8_t *)data, (uint32_t)data_len,
                                  resp_buf, sizeof(resp_buf));
    if (rc < 0)
    {
        snprintf(b->last_error, sizeof(b->last_error), "SET failed: %d", rc);
        lua_pushnil(L);
        lua_pushstring(L, b->last_error);
        return 2;
    }

    lua_pushlstring(L, (const char *)resp_buf, (size_t)rc);
    return 1;
}

/* ── Lua: action(classId, obis, methodId, data) ─────────────────────────── */

static int lua_action(lua_State *L)
{
    lua_bridge_t *b = get_bridge(L);
    if (!b || !b->connected || !b->client)
    {
        lua_pushnil(L);
        lua_pushstring(L, "not connected");
        return 2;
    }

    uint16_t class_id = (uint16_t)luaL_checkinteger(L, 1);
    csm_obis_code obis;
    if (!read_obis(L, 2, &obis))
    {
        lua_pushnil(L);
        lua_pushstring(L, "invalid OBIS");
        return 2;
    }
    uint8_t method_id = (uint8_t)luaL_checkinteger(L, 3);
    size_t data_len = 0;
    const char *data = NULL;
    if (!lua_isnoneornil(L, 4))
    {
        data = luaL_checklstring(L, 4, &data_len);
    }

    uint8_t resp_buf[2048];
    int rc = csm_client_action(b->client, b->invoke_id++, class_id, &obis, method_id,
                               (const uint8_t *)data, (uint32_t)data_len,
                               resp_buf, sizeof(resp_buf));
    if (rc < 0)
    {
        snprintf(b->last_error, sizeof(b->last_error), "ACTION failed: %d", rc);
        lua_pushnil(L);
        lua_pushstring(L, b->last_error);
        return 2;
    }

    lua_pushlstring(L, (const char *)resp_buf, (size_t)rc);
    return 1;
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
    lua_register(bridge->L, "getObjectList", lua_get_object_list);
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
    close_client(bridge);
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
            copy_cstr(bridge->last_error, sizeof(bridge->last_error), err);
            if (result) copy_cstr(result, result_size, err);
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
            copy_cstr(bridge->last_error, sizeof(bridge->last_error), err);
            if (result) copy_cstr(result, result_size, err);
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
                copy_cstr(bridge->last_error, sizeof(bridge->last_error), err);
                if (result) copy_cstr(result, result_size, err);
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
