/**
 * \file transport_wrap.cpp
 * \brief Node.js wrapper for csm_transport
 */

#include "transport_wrap.h"
#include <cstring>

Napi::Function TransportWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Transport", {
        InstanceMethod("clientInit", &TransportWrap::ClientInit),
        InstanceMethod("serverInit", &TransportWrap::ServerInit),
        InstanceMethod("connect", &TransportWrap::Connect),
        InstanceMethod("accept", &TransportWrap::Accept),
        InstanceMethod("getChannel", &TransportWrap::GetChannel),
        InstanceMethod("isConnected", &TransportWrap::IsConnected),
        InstanceMethod("destroy", &TransportWrap::Destroy),
    });

    exports.Set("Transport", func);
    return func;
}

TransportWrap::TransportWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<TransportWrap>(info), initialized_(false)
{
    memset(&transport_, 0, sizeof(transport_));
}

TransportWrap::~TransportWrap()
{
    if (initialized_)
    {
        csm_transport_tcp_destroy(&transport_);
        initialized_ = false;
    }
}

Napi::Value TransportWrap::ClientInit(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 3 || !info[0].IsString() || !info[1].IsNumber() || !info[2].IsNumber())
    {
        Napi::TypeError::New(env, "Arguments: host(String), port(Number), framing(Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string host = info[0].As<Napi::String>().Utf8Value();
    uint16_t port = info[1].As<Napi::Number>().Uint32Value();
    csm_framing_type framing = static_cast<csm_framing_type>(info[2].As<Napi::Number>().Uint32Value());

    int rc = csm_transport_tcp_client_init(&transport_, host.c_str(), port, framing);
    if (rc == CSM_TRANSPORT_OK)
    {
        initialized_ = true;
    }

    return Napi::Number::New(env, rc);
}

Napi::Value TransportWrap::ServerInit(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber())
    {
        Napi::TypeError::New(env, "Arguments: port(Number), framing(Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint16_t port = info[0].As<Napi::Number>().Uint32Value();
    csm_framing_type framing = static_cast<csm_framing_type>(info[1].As<Napi::Number>().Uint32Value());

    int rc = csm_transport_tcp_server_init(&transport_, port, framing);
    if (rc == CSM_TRANSPORT_OK)
    {
        initialized_ = true;
    }

    return Napi::Number::New(env, rc);
}

Napi::Value TransportWrap::Connect(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!initialized_)
    {
        Napi::Error::New(env, "Transport not initialized").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint32_t timeout_ms = CSM_TRANSPORT_DEFAULT_TIMEOUT;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        timeout_ms = info[0].As<Napi::Number>().Uint32Value();
    }

    int rc = csm_transport_tcp_connect(&transport_, timeout_ms);
    return Napi::Number::New(env, rc);
}

Napi::Value TransportWrap::Accept(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!initialized_)
    {
        Napi::Error::New(env, "Transport not initialized").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint32_t timeout_ms = CSM_TRANSPORT_DEFAULT_TIMEOUT;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        timeout_ms = info[0].As<Napi::Number>().Uint32Value();
    }

    int rc = csm_transport_tcp_accept(&transport_, timeout_ms);
    return Napi::Number::New(env, rc);
}

Napi::Value TransportWrap::GetChannel(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint8_t channel = 0;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        channel = info[0].As<Napi::Number>().Uint32Value();
    }

    bool connected = CSM_TRANSPORT_IS_CONNECTED(&transport_, channel) != 0;
    return Napi::Boolean::New(env, connected);
}

Napi::Value TransportWrap::IsConnected(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint8_t channel = 0;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        channel = info[0].As<Napi::Number>().Uint32Value();
    }

    bool connected = CSM_TRANSPORT_IS_CONNECTED(&transport_, channel) != 0;
    return Napi::Boolean::New(env, connected);
}

Napi::Value TransportWrap::Destroy(const Napi::CallbackInfo &info)
{
    if (initialized_)
    {
        csm_transport_tcp_destroy(&transport_);
        initialized_ = false;
    }

    return info.Env().Undefined();
}
