/**
 * \file transport_wrap.cpp
 * \brief Node.js wrapper for csm_transport (TCP)
 */

#include "transport_wrap.h"
#include "csm_transport_tcp.h"
#include <cstring>

Napi::FunctionReference TransportWrap::constructor;

Napi::Function TransportWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Transport", {
        InstanceMethod("connect", &TransportWrap::Connect),
        InstanceMethod("send", &TransportWrap::Send),
        InstanceMethod("receive", &TransportWrap::Receive),
        InstanceMethod("close", &TransportWrap::Close),
        InstanceMethod("isConnected", &TransportWrap::IsConnected),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("Transport", func);
    return func;
}

TransportWrap::TransportWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<TransportWrap>(info), transport_(nullptr), connected_(false)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString())
    {
        Napi::TypeError::New(env, "String expected for host").ThrowAsJavaScriptException();
        return;
    }

    std::string host = info[0].As<Napi::String>().Utf8Value();
    strncpy(host_, host.c_str(), sizeof(host_) - 1);
    host_[sizeof(host_) - 1] = '\0';

    port_ = 4056;
    if (info.Length() >= 2 && info[1].IsNumber())
    {
        port_ = (uint16_t)info[1].As<Napi::Number>().Uint32Value();
    }

    transport_ = csm_transport_tcp_create();
}

TransportWrap::~TransportWrap()
{
    if (transport_)
    {
        csm_transport_tcp_destroy(transport_);
        transport_ = nullptr;
    }
}

Napi::Value TransportWrap::Connect(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!transport_)
    {
        Napi::Error::New(env, "Transport not initialized").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    int result = csm_transport_tcp_open(transport_, 0, host_, port_);
    connected_ = (result == 0);

    return Napi::Boolean::New(env, connected_);
}

Napi::Value TransportWrap::Send(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer())
    {
        Napi::TypeError::New(env, "Buffer expected").ThrowAsJavaScriptException();
        return Napi::Number::New(env, -1);
    }

    Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
    int sent = csm_transport_tcp_send(transport_, 0, buf.Data(), (uint32_t)buf.Length());

    return Napi::Number::New(env, sent);
}

Napi::Value TransportWrap::Receive(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint32_t timeout_ms = 1000;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        timeout_ms = info[0].As<Napi::Number>().Uint32Value();
    }

    uint8_t buf[2048];
    int received = csm_transport_tcp_recv(transport_, 0, buf, sizeof(buf), timeout_ms);

    if (received <= 0)
    {
        return env.Null();
    }

    return Napi::Buffer<uint8_t>::Copy(env, buf, (size_t)received);
}

Napi::Value TransportWrap::Close(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (transport_ && connected_)
    {
        csm_transport_tcp_close(transport_, 0);
        connected_ = false;
    }

    return env.Undefined();
}

Napi::Value TransportWrap::IsConnected(const Napi::CallbackInfo &info)
{
    return Napi::Boolean::New(info.Env(), connected_);
}
