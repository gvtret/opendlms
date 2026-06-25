/**
 * \file server_wrap.cpp
 * \brief Node.js wrapper for csm_server
 */

#include "server_wrap.h"
#include "transport_wrap.h"
#include <cstring>

static csm_db_code db_handler_wrapper(csm_db_context_t *ctx, csm_array *in, csm_array *out, csm_request *request)
{
    /* TODO: call JavaScript callback */
    (void)ctx;
    (void)in;
    (void)out;
    (void)request;
    return CSM_OK;
}

Napi::Function ServerWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Server", {
        InstanceMethod("init", &ServerWrap::InitServer),
        InstanceMethod("registerDB", &ServerWrap::RegisterDB),
        InstanceMethod("poll", &ServerWrap::Poll),
        InstanceMethod("send", &ServerWrap::SendUnsolicited),
        InstanceMethod("destroy", &ServerWrap::Destroy),
    });

    exports.Set("Server", func);
    return func;
}

ServerWrap::ServerWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<ServerWrap>(info), server_(nullptr)
{
    server_ = new csm_server();
    memset(server_, 0, sizeof(csm_server));
}

ServerWrap::~ServerWrap()
{
    Destroy(info.Env());
}

Napi::Value ServerWrap::InitServer(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1)
    {
        Napi::TypeError::New(env, "TransportWrap expected").ThrowAsJavaScriptException();
        return Napi::Boolean::New(env, false);
    }

    TransportWrap *transport = Napi::ObjectWrap<TransportWrap>::Unwrap(info[0].As<Napi::Object>());

    int result = csm_server_init(server_, nullptr, 0, CSM_FRAMING_WRAPPER);

    return Napi::Boolean::New(env, result == 0);
}

Napi::Value ServerWrap::RegisterDB(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() >= 1 && info[0].IsFunction())
    {
        db_callback_ = Napi::Persistent(info[0].As<Napi::Function>());
    }

    csm_server_register_db(server_, db_handler_wrapper);

    return env.Undefined();
}

Napi::Value ServerWrap::Poll(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint32_t timeout_ms = 100;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        timeout_ms = info[0].As<Napi::Number>().Uint32Value();
    }

    int result = csm_server_poll(server_, timeout_ms);

    return Napi::Number::New(env, result);
}

Napi::Value ServerWrap::SendUnsolicited(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    /* TODO: implement */
    return Napi::Number::New(env, -1);
}

Napi::Value ServerWrap::Destroy(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (server_)
    {
        csm_server_destroy(server_);
        delete server_;
        server_ = nullptr;
    }

    return env.Undefined();
}
