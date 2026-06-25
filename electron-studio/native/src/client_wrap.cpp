/**
 * \file client_wrap.cpp
 * \brief Node.js wrapper for csm_client
 */

#include "client_wrap.h"
#include "transport_wrap.h"
#include <cstring>

Napi::Function ClientWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Client", {
        InstanceMethod("connect", &ClientWrap::Connect),
        InstanceMethod("disconnect", &ClientWrap::Disconnect),
        InstanceMethod("get", &ClientWrap::Get),
        InstanceMethod("getBlock", &ClientWrap::GetBlock),
        InstanceMethod("set", &ClientWrap::Set),
        InstanceMethod("setBlock", &ClientWrap::SetBlock),
        InstanceMethod("action", &ClientWrap::Action),
    });

    exports.Set("Client", func);
    return func;
}

ClientWrap::ClientWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<ClientWrap>(info), client_(nullptr)
{
    client_ = new csm_client();
    memset(client_, 0, sizeof(csm_client));
}

ClientWrap::~ClientWrap()
{
    if (client_)
    {
        csm_client_destroy(client_);
        delete client_;
        client_ = nullptr;
    }
}

Napi::Value ClientWrap::Connect(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint32_t timeout_ms = 5000;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        timeout_ms = info[0].As<Napi::Number>().Uint32Value();
    }

    int result = csm_client_connect(client_, timeout_ms);

    return Napi::Boolean::New(env, result == 0);
}

Napi::Value ClientWrap::Disconnect(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    int result = csm_client_disconnect(client_);

    return Napi::Boolean::New(env, result == 0);
}

Napi::Value ClientWrap::Get(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 4)
    {
        Napi::TypeError::New(env, "Expected: invokeId, classId, obis, attrId").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = (uint8_t)info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = (uint16_t)info[1].As<Napi::Number>().Uint32Value();

    Napi::Array obis_arr = info[2].As<Napi::Array>();
    csm_obis_code obis;
    for (int i = 0; i < 6; i++)
    {
        ((uint8_t *)&obis.A)[i] = (uint8_t)obis_arr.Get(i).As<Napi::Number>().Uint32Value();
    }

    uint8_t attr_id = (uint8_t)info[3].As<Napi::Number>().Uint32Value();

    uint8_t resp[2048];
    int len = csm_client_get(client_, invoke_id, class_id, &obis, attr_id, resp, sizeof(resp));

    if (len <= 0)
    {
        return env.Null();
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp, (size_t)len);
}

Napi::Value ClientWrap::GetBlock(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 4)
    {
        Napi::TypeError::New(env, "Expected: invokeId, classId, obis, attrId").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = (uint8_t)info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = (uint16_t)info[1].As<Napi::Number>().Uint32Value();

    Napi::Array obis_arr = info[2].As<Napi::Array>();
    csm_obis_code obis;
    for (int i = 0; i < 6; i++)
    {
        ((uint8_t *)&obis.A)[i] = (uint8_t)obis_arr.Get(i).As<Napi::Number>().Uint32Value();
    }

    uint8_t attr_id = (uint8_t)info[3].As<Napi::Number>().Uint32Value();

    uint8_t resp[4096];
    int len = csm_client_get_block(client_, invoke_id, class_id, &obis, attr_id, resp, sizeof(resp));

    if (len <= 0)
    {
        return env.Null();
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp, (size_t)len);
}

Napi::Value ClientWrap::Set(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 5)
    {
        Napi::TypeError::New(env, "Expected: invokeId, classId, obis, attrId, data").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = (uint8_t)info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = (uint16_t)info[1].As<Napi::Number>().Uint32Value();

    Napi::Array obis_arr = info[2].As<Napi::Array>();
    csm_obis_code obis;
    for (int i = 0; i < 6; i++)
    {
        ((uint8_t *)&obis.A)[i] = (uint8_t)obis_arr.Get(i).As<Napi::Number>().Uint32Value();
    }

    uint8_t attr_id = (uint8_t)info[3].As<Napi::Number>().Uint32Value();

    Napi::Buffer<uint8_t> data_buf = info[4].As<Napi::Buffer<uint8_t>>();

    uint8_t resp[512];
    int len = csm_client_set(client_, invoke_id, class_id, &obis, attr_id,
                              data_buf.Data(), (uint32_t)data_buf.Length(),
                              resp, sizeof(resp));

    if (len <= 0)
    {
        return env.Null();
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp, (size_t)len);
}

Napi::Value ClientWrap::SetBlock(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 5)
    {
        Napi::TypeError::New(env, "Expected: invokeId, classId, obis, attrId, data").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = (uint8_t)info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = (uint16_t)info[1].As<Napi::Number>().Uint32Value();

    Napi::Array obis_arr = info[2].As<Napi::Array>();
    csm_obis_code obis;
    for (int i = 0; i < 6; i++)
    {
        ((uint8_t *)&obis.A)[i] = (uint8_t)obis_arr.Get(i).As<Napi::Number>().Uint32Value();
    }

    uint8_t attr_id = (uint8_t)info[3].As<Napi::Number>().Uint32Value();

    Napi::Buffer<uint8_t> data_buf = info[4].As<Napi::Buffer<uint8_t>>();

    uint8_t resp[4096];
    int len = csm_client_set_block(client_, invoke_id, class_id, &obis, attr_id,
                                    data_buf.Data(), (uint32_t)data_buf.Length(),
                                    resp, sizeof(resp));

    if (len <= 0)
    {
        return env.Null();
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp, (size_t)len);
}

Napi::Value ClientWrap::Action(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 5)
    {
        Napi::TypeError::New(env, "Expected: invokeId, classId, obis, methodId, data").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = (uint8_t)info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = (uint16_t)info[1].As<Napi::Number>().Uint32Value();

    Napi::Array obis_arr = info[2].As<Napi::Array>();
    csm_obis_code obis;
    for (int i = 0; i < 6; i++)
    {
        ((uint8_t *)&obis.A)[i] = (uint8_t)obis_arr.Get(i).As<Napi::Number>().Uint32Value();
    }

    uint8_t method_id = (uint8_t)info[3].As<Napi::Number>().Uint32Value();

    Napi::Buffer<uint8_t> data_buf = info[4].As<Napi::Buffer<uint8_t>>();

    uint8_t resp[512];
    int len = csm_client_action(client_, invoke_id, class_id, &obis, method_id,
                                 data_buf.Data(), (uint32_t)data_buf.Length(),
                                 resp, sizeof(resp));

    if (len <= 0)
    {
        return env.Null();
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp, (size_t)len);
}
