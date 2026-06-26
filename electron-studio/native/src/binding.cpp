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

    return exports;
}

NODE_API_MODULE(opendlms_native, InitAll)
