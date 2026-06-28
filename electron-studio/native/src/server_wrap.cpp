/**
 * \file server_wrap.cpp
 * \brief Node.js wrapper for csm_server
 */

#include "server_wrap.h"
#include "csm_transport_tcp.h"
#include <cstring>
#include <limits>

static bool buffer_len_u32(Napi::Env env, size_t len, uint32_t *out)
{
    if (len > (size_t)std::numeric_limits<uint32_t>::max())
    {
        Napi::RangeError::New(env, "Buffer too large").ThrowAsJavaScriptException();
        return false;
    }
    *out = (uint32_t)len;
    return true;
}

Napi::Function ServerWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Server", {
        InstanceMethod("poll", &ServerWrap::Poll),
        InstanceMethod("send", &ServerWrap::Send),
        InstanceMethod("destroy", &ServerWrap::Destroy),
    });

    exports.Set("Server", func);
    return func;
}

ServerWrap::ServerWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<ServerWrap>(info), server_(nullptr), transport_(nullptr), initialized_(false)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsObject())
    {
        Napi::TypeError::New(env, "Argument: transport (TransportWrap)").ThrowAsJavaScriptException();
        return;
    }

    transport_ = Napi::ObjectWrap<TransportWrap>::Unwrap(info[0].As<Napi::Object>());
    if (!transport_)
    {
        Napi::Error::New(env, "Invalid transport").ThrowAsJavaScriptException();
        return;
    }

    server_ = csm_server_create(&transport_->transport_, 0, CSM_FRAMING_WRAPPER);
    if (!server_)
    {
        Napi::Error::New(env, "Failed to initialize server").ThrowAsJavaScriptException();
    }
    else
    {
        initialized_ = true;
    }
}

ServerWrap::~ServerWrap()
{
    if (server_)
    {
        csm_server_delete(server_);
        server_ = nullptr;
    }
}

Napi::Value ServerWrap::Poll(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!server_)
    {
        Napi::Error::New(env, "Server not initialized").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint32_t timeout_ms = 1000;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        timeout_ms = info[0].As<Napi::Number>().Uint32Value();
    }

    int rc = csm_server_poll(server_, timeout_ms);
    return Napi::Number::New(env, rc);
}

Napi::Value ServerWrap::Send(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!server_)
    {
        Napi::Error::New(env, "Server not initialized").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (info.Length() < 1 || !info[0].IsBuffer())
    {
        Napi::TypeError::New(env, "Argument: apdu (Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t channel = 0;
    if (info.Length() >= 2 && info[1].IsNumber())
    {
        channel = info[1].As<Napi::Number>().Uint32Value();
    }

    Napi::Uint8Array data_arr = info[0].As<Napi::Uint8Array>();
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data_arr.ByteLength(), &data_len)) return env.Null();

    int rc = csm_server_send(server_, channel, data_arr.Data(), data_len);

    return Napi::Number::New(env, rc);
}

Napi::Value ServerWrap::Destroy(const Napi::CallbackInfo &info)
{
    if (server_)
    {
        csm_server_delete(server_);
        server_ = nullptr;
        initialized_ = false;
    }

    return info.Env().Undefined();
}
