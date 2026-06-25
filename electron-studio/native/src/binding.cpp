/**
 * \file binding.cpp
 * \brief Node.js native addon entry point for OpenDLMS
 *
 *  Wraps cosemlib C API for use from Electron.js
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include <napi.h>

Napi::Object InitAll(Napi::Env env, Napi::Object exports)
{
    /* Register module classes */
    exports.Set("Transport", Napi::Function::New(env, TransportWrap::Init));
    exports.Set("Server", Napi::Function::New(env, ServerWrap::Init));
    exports.Set("Client", Napi::Function::New(env, ClientWrap::Init));

    /* Constants */
    exports.Set("VERSION", Napi::String::New(env, "1.1.0"));
    exports.Set("FRAMING_NONE", Napi::Number::New(env, 0));
    exports.Set("FRAMING_WRAPPER", Napi::Number::New(env, 1));
    exports.Set("FRAMING_HDLC", Napi::Number::New(env, 2));

    return exports;
}

NODE_API_MODULE(opendlms_native, InitAll)
