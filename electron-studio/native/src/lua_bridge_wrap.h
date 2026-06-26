/**
 * \file lua_bridge_wrap.h
 * \brief Node.js wrapper for Lua bridge
 */

#ifndef LUA_BRIDGE_WRAP_H
#define LUA_BRIDGE_WRAP_H

#include <napi.h>
#include "lua_bridge.h"

class LuaBridgeWrap : public Napi::ObjectWrap<LuaBridgeWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);
    LuaBridgeWrap(const Napi::CallbackInfo &info);
    ~LuaBridgeWrap();

    Napi::Value Exec(const Napi::CallbackInfo &info);
    Napi::Value ExecFile(const Napi::CallbackInfo &info);
    Napi::Value ExecReturn(const Napi::CallbackInfo &info);
    Napi::Value GetOutput(const Napi::CallbackInfo &info);
    Napi::Value ClearOutput(const Napi::CallbackInfo &info);
    Napi::Value GetError(const Napi::CallbackInfo &info);
    Napi::Value IsConnected(const Napi::CallbackInfo &info);

private:
    lua_bridge_t bridge_;
};

#endif /* LUA_BRIDGE_WRAP_H */
