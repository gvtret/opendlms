/**
 * \file binding.cpp
 * \brief Node.js native addon entry point for OpenDLMS
 */

#include <napi.h>
#include "lua_bridge_wrap.h"
#include "transport_wrap.h"
#include "client_wrap.h"
#include "server_wrap.h"
#include "data_wrap.h"
#include "block_wrap.h"

bool opendlms_native_set_security_key(uint8_t sap, uint8_t key_id,
                                      const uint8_t *key, size_t key_len);
void opendlms_native_clear_security_keys(void);
uint8_t opendlms_native_get_security_key_len(uint8_t sap, uint8_t key_id);

static Napi::Value SetSecurityKey(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (info.Length() < 3 || !info[0].IsNumber() || !info[1].IsNumber() ||
        !info[2].IsBuffer())
    {
        Napi::TypeError::New(env, "Arguments: sap(Number), keyId(Number), key(Buffer)")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t sap = info[0].As<Napi::Number>().Uint32Value();
    uint8_t key_id = info[1].As<Napi::Number>().Uint32Value();
    Napi::Buffer<uint8_t> key = info[2].As<Napi::Buffer<uint8_t>>();

    bool ok = opendlms_native_set_security_key(sap, key_id, key.Data(), key.Length());
    return Napi::Boolean::New(env, ok);
}

static Napi::Value ClearSecurityKeys(const Napi::CallbackInfo &info)
{
    opendlms_native_clear_security_keys();
    return info.Env().Undefined();
}

static Napi::Value GetSecurityKeyLength(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber())
    {
        Napi::TypeError::New(env, "Arguments: sap(Number), keyId(Number)")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t sap = info[0].As<Napi::Number>().Uint32Value();
    uint8_t key_id = info[1].As<Napi::Number>().Uint32Value();
    return Napi::Number::New(env, opendlms_native_get_security_key_len(sap, key_id));
}

Napi::Object InitAll(Napi::Env env, Napi::Object exports)
{
    exports.Set("LuaBridge", LuaBridgeWrap::Init(env, exports));
    exports.Set("Transport", TransportWrap::Init(env, exports));
    exports.Set("Client", ClientWrap::Init(env, exports));
    exports.Set("Server", ServerWrap::Init(env, exports));
    exports.Set("Data", DataWrap::Init(env, exports));
    exports.Set("Block", BlockWrap::Init(env, exports));
    exports.Set("VERSION", Napi::String::New(env, "1.1.0"));
    exports.Set("FRAMING_NONE", Napi::Number::New(env, 0));
    exports.Set("FRAMING_WRAPPER", Napi::Number::New(env, 1));
    exports.Set("FRAMING_HDLC", Napi::Number::New(env, 2));
    exports.Set("SEC_KEK", Napi::Number::New(env, 0));
    exports.Set("SEC_GUEK", Napi::Number::New(env, 1));
    exports.Set("SEC_GBEK", Napi::Number::New(env, 2));
    exports.Set("SEC_GAK", Napi::Number::New(env, 3));
    exports.Set("setSecurityKey", Napi::Function::New(env, SetSecurityKey));
    exports.Set("clearSecurityKeys", Napi::Function::New(env, ClearSecurityKeys));
    exports.Set("getSecurityKeyLength", Napi::Function::New(env, GetSecurityKeyLength));

    return exports;
}

NODE_API_MODULE(opendlms_native, InitAll)
