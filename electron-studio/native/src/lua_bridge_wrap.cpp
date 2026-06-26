/**
 * \file lua_bridge_wrap.cpp
 * \brief Node.js wrapper for Lua bridge
 */

#include "lua_bridge_wrap.h"
#include <cstring>

Napi::Function LuaBridgeWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "LuaBridge", {
        InstanceMethod("exec", &LuaBridgeWrap::Exec),
        InstanceMethod("execFile", &LuaBridgeWrap::ExecFile),
        InstanceMethod("execReturn", &LuaBridgeWrap::ExecReturn),
        InstanceMethod("getOutput", &LuaBridgeWrap::GetOutput),
        InstanceMethod("clearOutput", &LuaBridgeWrap::ClearOutput),
        InstanceMethod("getError", &LuaBridgeWrap::GetError),
        InstanceMethod("isConnected", &LuaBridgeWrap::IsConnected),
    });

    exports.Set("LuaBridge", func);
    return func;
}

LuaBridgeWrap::LuaBridgeWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<LuaBridgeWrap>(info)
{
    memset(&bridge_, 0, sizeof(bridge_));
    lua_bridge_init(&bridge_);
}

LuaBridgeWrap::~LuaBridgeWrap()
{
    lua_bridge_destroy(&bridge_);
}

Napi::Value LuaBridgeWrap::Exec(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString())
    {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string script = info[0].As<Napi::String>().Utf8Value();

    char result[4096];
    int rc = lua_bridge_exec(&bridge_, script.c_str(), result, sizeof(result));

    if (rc != 0)
    {
        return Napi::String::New(env, result);
    }

    return env.Null();
}

Napi::Value LuaBridgeWrap::ExecFile(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString())
    {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string filename = info[0].As<Napi::String>().Utf8Value();

    char result[4096];
    int rc = lua_bridge_exec_file(&bridge_, filename.c_str(), result, sizeof(result));

    if (rc != 0)
    {
        return Napi::String::New(env, result);
    }

    return env.Null();
}

Napi::Value LuaBridgeWrap::GetError(const Napi::CallbackInfo &info)
{
    const char *err = lua_bridge_get_error(&bridge_);
    return Napi::String::New(info.Env(), err ? err : "");
}

Napi::Value LuaBridgeWrap::IsConnected(const Napi::CallbackInfo &info)
{
    return Napi::Boolean::New(info.Env(), bridge_.connected != 0);
}

Napi::Value LuaBridgeWrap::ExecReturn(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString())
    {
        Napi::TypeError::New(env, "String expected").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string script = info[0].As<Napi::String>().Utf8Value();

    char result[4096];
    int rc = lua_bridge_exec_return(&bridge_, script.c_str(), result, sizeof(result));

    if (rc != 0)
    {
        /* Return error as string */
        return Napi::String::New(env, result);
    }

    /* Return result — may be empty string for nil */
    return Napi::String::New(env, result);
}

Napi::Value LuaBridgeWrap::GetOutput(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    char output[16384];
    uint32_t len = lua_bridge_get_output(&bridge_, output, sizeof(output));

    return Napi::String::New(env, output, len);
}

Napi::Value LuaBridgeWrap::ClearOutput(const Napi::CallbackInfo &info)
{
    lua_bridge_clear_output(&bridge_);
    return info.Env().Undefined();
}
