/**
 * \file block_wrap.cpp
 * \brief Node.js wrapper for csm_block_state (block transfer)
 */

#include "block_wrap.h"
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

Napi::Function BlockWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Block", {
        InstanceMethod("startServer", &BlockWrap::StartServer),
        InstanceMethod("encodeFirst", &BlockWrap::EncodeFirst),
        InstanceMethod("encodeNext", &BlockWrap::EncodeNext),
        InstanceMethod("isActive", &BlockWrap::IsActive),
        InstanceMethod("abort", &BlockWrap::Abort),
        InstanceMethod("startClient", &BlockWrap::StartClient),
        InstanceMethod("encodeSetRequest", &BlockWrap::EncodeSetRequest),
        InstanceMethod("encodeSetNext", &BlockWrap::EncodeSetNext),
        InstanceMethod("startGetReceive", &BlockWrap::StartGetReceive),
        InstanceMethod("getReceiveData", &BlockWrap::GetReceiveData),
        InstanceMethod("getReceivedData", &BlockWrap::GetReceivedData),
        InstanceMethod("encodeGetNext", &BlockWrap::EncodeGetNext),
        InstanceMethod("startReceive", &BlockWrap::StartReceive),
        InstanceMethod("receiveData", &BlockWrap::ReceiveData),
        InstanceMethod("getReceived", &BlockWrap::GetReceived),
        InstanceMethod("encodeSetResponse", &BlockWrap::EncodeSetResponse),
        InstanceMethod("canReceive", &BlockWrap::CanReceive),
        InstanceMethod("init", &BlockWrap::Init),
    });

    exports.Set("Block", func);
    return func;
}

BlockWrap::BlockWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<BlockWrap>(info), active_(false)
{
    memset(&state_, 0, sizeof(state_));
    csm_block_init(&state_);
}

BlockWrap::~BlockWrap()
{
    csm_block_abort(&state_);
}

Napi::Value BlockWrap::Init(const Napi::CallbackInfo &info)
{
    csm_block_init(&state_);
    active_ = false;
    return info.Env().Undefined();
}

Napi::Value BlockWrap::StartServer(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsBuffer())
    {
        Napi::TypeError::New(env, "Arguments: invokeId (Number), data (Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();
    Napi::Uint8Array data = info[1].As<Napi::Uint8Array>();
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data.ByteLength(), &data_len)) return env.Null();

    uint32_t block_size = 0;
    if (info.Length() >= 3 && info[2].IsNumber())
    {
        block_size = info[2].As<Napi::Number>().Uint32Value();
    }

    int rc = csm_block_start_server(&state_, invoke_id, data.Data(), data_len, block_size);
    if (rc != 0) active_ = true;
    return Napi::Number::New(env, rc);
}

Napi::Value BlockWrap::EncodeFirst(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint32_t max_size = 200;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        max_size = info[0].As<Napi::Number>().Uint32Value();
    }

    uint8_t buf[2048];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0, 0);

    int rc = csm_block_encode_first(&state_, &array, max_size);
    if (rc == 0)
    {
        return env.Null();
    }

    uint32_t written = csm_array_written(&array);
    return Napi::Buffer<uint8_t>::Copy(env, buf, written);
}

Napi::Value BlockWrap::EncodeNext(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint32_t max_size = 200;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        max_size = info[0].As<Napi::Number>().Uint32Value();
    }

    uint8_t buf[2048];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0, 0);

    int rc = csm_block_encode_next(&state_, &array, max_size);
    if (rc == 0)
    {
        return env.Null();
    }

    uint32_t written = csm_array_written(&array);
    return Napi::Buffer<uint8_t>::Copy(env, buf, written);
}

Napi::Value BlockWrap::IsActive(const Napi::CallbackInfo &info)
{
    return Napi::Boolean::New(info.Env(), csm_block_is_active(&state_) != 0);
}

Napi::Value BlockWrap::Abort(const Napi::CallbackInfo &info)
{
    csm_block_abort(&state_);
    active_ = false;
    return info.Env().Undefined();
}

Napi::Value BlockWrap::StartClient(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsBuffer())
    {
        Napi::TypeError::New(env, "Arguments: invokeId (Number), data (Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();
    Napi::Uint8Array data = info[1].As<Napi::Uint8Array>();
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data.ByteLength(), &data_len)) return env.Null();

    uint32_t block_size = 0;
    if (info.Length() >= 3 && info[2].IsNumber())
    {
        block_size = info[2].As<Napi::Number>().Uint32Value();
    }

    int rc = csm_block_start_client(&state_, invoke_id, data.Data(), data_len, block_size);
    if (rc != 0) active_ = true;
    return Napi::Number::New(env, rc);
}

Napi::Value BlockWrap::EncodeSetRequest(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint32_t max_size = 200;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        max_size = info[0].As<Napi::Number>().Uint32Value();
    }

    uint8_t buf[2048];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0, 0);

    csm_request request;
    memset(&request, 0, sizeof(request));

    int rc = csm_block_encode_set_request(&state_, &array, &request, max_size);
    if (rc == 0)
    {
        return env.Null();
    }

    uint32_t written = csm_array_written(&array);
    return Napi::Buffer<uint8_t>::Copy(env, buf, written);
}

Napi::Value BlockWrap::EncodeSetNext(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint32_t max_size = 200;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        max_size = info[0].As<Napi::Number>().Uint32Value();
    }

    uint8_t buf[2048];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0, 0);

    int rc = csm_block_encode_set_next(&state_, &array, max_size);
    if (rc == 0)
    {
        return env.Null();
    }

    uint32_t written = csm_array_written(&array);
    return Napi::Buffer<uint8_t>::Copy(env, buf, written);
}

Napi::Value BlockWrap::StartGetReceive(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber())
    {
        Napi::TypeError::New(env, "Argument: invokeId (Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();

    uint32_t block_size = 0;
    if (info.Length() >= 2 && info[1].IsNumber())
    {
        block_size = info[1].As<Napi::Number>().Uint32Value();
    }

    int rc = csm_block_start_get_receive(&state_, invoke_id, block_size);
    return Napi::Number::New(env, rc);
}

Napi::Value BlockWrap::GetReceiveData(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsBuffer() || !info[1].IsBoolean())
    {
        Napi::TypeError::New(env, "Arguments: data (Buffer), isLast (Boolean)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Uint8Array data = info[0].As<Napi::Uint8Array>();
    uint8_t is_last = info[1].As<Napi::Boolean>().Value() ? 1 : 0;
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data.ByteLength(), &data_len)) return env.Null();

    int rc = csm_block_get_receive_data(&state_, data.Data(), data_len, is_last);
    return Napi::Number::New(env, rc);
}

Napi::Value BlockWrap::GetReceivedData(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    const uint8_t *data = nullptr;
    uint32_t data_size = 0;

    int rc = csm_block_get_received_data(&state_, &data, &data_size);
    if (rc == 0 || !data)
    {
        return env.Null();
    }

    return Napi::Buffer<uint8_t>::Copy(env, data, data_size);
}

Napi::Value BlockWrap::EncodeGetNext(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber())
    {
        Napi::TypeError::New(env, "Arguments: invokeId (Number), blockNumber (Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();
    uint32_t block_number = info[1].As<Napi::Number>().Uint32Value();

    uint8_t buf[256];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0, 0);

    int rc = csm_block_encode_get_next(&state_, &array, invoke_id, block_number);
    if (rc == 0)
    {
        return env.Null();
    }

    uint32_t written = csm_array_written(&array);
    return Napi::Buffer<uint8_t>::Copy(env, buf, written);
}

Napi::Value BlockWrap::StartReceive(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber())
    {
        Napi::TypeError::New(env, "Argument: invokeId (Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();

    uint32_t block_size = 0;
    if (info.Length() >= 2 && info[1].IsNumber())
    {
        block_size = info[1].As<Napi::Number>().Uint32Value();
    }

    int rc = csm_block_start_receive(&state_, invoke_id, block_size);
    return Napi::Number::New(env, rc);
}

Napi::Value BlockWrap::ReceiveData(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsBuffer() || !info[1].IsBoolean())
    {
        Napi::TypeError::New(env, "Arguments: data (Buffer), isLast (Boolean)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Uint8Array data = info[0].As<Napi::Uint8Array>();
    uint8_t is_last = info[1].As<Napi::Boolean>().Value() ? 1 : 0;
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data.ByteLength(), &data_len)) return env.Null();

    int rc = csm_block_receive_data(&state_, data.Data(), data_len, is_last);
    return Napi::Number::New(env, rc);
}

Napi::Value BlockWrap::GetReceived(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    const uint8_t *data = nullptr;
    uint32_t data_size = 0;

    int rc = csm_block_get_received(&state_, &data, &data_size);
    if (rc == 0 || !data)
    {
        return env.Null();
    }

    return Napi::Buffer<uint8_t>::Copy(env, data, data_size);
}

Napi::Value BlockWrap::EncodeSetResponse(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint8_t buf[256];
    csm_array array;
    csm_array_init(&array, buf, sizeof(buf), 0, 0);

    int rc = csm_block_encode_set_response(&state_, &array);
    if (rc == 0)
    {
        return env.Null();
    }

    uint32_t written = csm_array_written(&array);
    return Napi::Buffer<uint8_t>::Copy(env, buf, written);
}

Napi::Value BlockWrap::CanReceive(const Napi::CallbackInfo &info)
{
    return Napi::Boolean::New(info.Env(), csm_block_can_receive(&state_) != 0);
}
