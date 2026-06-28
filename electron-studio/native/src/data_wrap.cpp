/**
 * \file data_wrap.cpp
 * \brief Node.js wrapper for csm_array (AXDR data buffer)
 */

#include "data_wrap.h"
#include "csm_axdr_codec.h"
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

Napi::Function DataWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Data", {
        InstanceMethod("writeU8", &DataWrap::WriteU8),
        InstanceMethod("writeU16", &DataWrap::WriteU16),
        InstanceMethod("writeU32", &DataWrap::WriteU32),
        InstanceMethod("writeBuffer", &DataWrap::WriteBuffer),
        InstanceMethod("writeBoolean", &DataWrap::WriteBoolean),
        InstanceMethod("writeOctetString", &DataWrap::WriteOctetString),
        InstanceMethod("readU8", &DataWrap::ReadU8),
        InstanceMethod("readU16", &DataWrap::ReadU16),
        InstanceMethod("readU32", &DataWrap::ReadU32),
        InstanceMethod("readBuffer", &DataWrap::ReadBuffer),
        InstanceMethod("written", &DataWrap::Written),
        InstanceMethod("unread", &DataWrap::Unread),
        InstanceMethod("freeSize", &DataWrap::FreeSize),
        InstanceMethod("toBuffer", &DataWrap::ToBuffer),
        InstanceMethod("reset", &DataWrap::Reset),
    });

    exports.Set("Data", func);
    return func;
}

DataWrap::DataWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<DataWrap>(info), buf_(nullptr), buf_size_(0)
{
    Napi::Env env = info.Env();

    buf_size_ = 4096;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        buf_size_ = info[0].As<Napi::Number>().Uint32Value();
    }

    buf_ = new uint8_t[buf_size_];
    memset(buf_, 0, buf_size_);
    csm_array_init(&array_, buf_, buf_size_, 0, 0);
}

DataWrap::~DataWrap()
{
    delete[] buf_;
    buf_ = nullptr;
}

Napi::Value DataWrap::WriteU8(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber())
    {
        Napi::TypeError::New(env, "Argument: value (Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t val = info[0].As<Napi::Number>().Uint32Value();
    int rc = csm_array_write_u8(&array_, val);
    return Napi::Number::New(env, rc);
}

Napi::Value DataWrap::WriteU16(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber())
    {
        Napi::TypeError::New(env, "Argument: value (Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint16_t val = info[0].As<Napi::Number>().Uint32Value();
    int rc = csm_array_write_u16(&array_, val);
    return Napi::Number::New(env, rc);
}

Napi::Value DataWrap::WriteU32(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber())
    {
        Napi::TypeError::New(env, "Argument: value (Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint32_t val = info[0].As<Napi::Number>().Uint32Value();
    int rc = csm_array_write_u32(&array_, val);
    return Napi::Number::New(env, rc);
}

Napi::Value DataWrap::WriteBuffer(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer())
    {
        Napi::TypeError::New(env, "Argument: buffer (Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Uint8Array data = info[0].As<Napi::Uint8Array>();
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data.ByteLength(), &data_len)) return env.Null();

    int rc = csm_array_write_buff(&array_, data.Data(), data_len);
    return Napi::Number::New(env, rc);
}

Napi::Value DataWrap::WriteBoolean(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBoolean())
    {
        Napi::TypeError::New(env, "Argument: value (Boolean)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t val = info[0].As<Napi::Boolean>().Value() ? 1 : 0;
    int rc = csm_axdr_wr_boolean(&array_, val);
    return Napi::Number::New(env, rc);
}

Napi::Value DataWrap::WriteOctetString(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsBuffer())
    {
        Napi::TypeError::New(env, "Argument: buffer (Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Uint8Array data = info[0].As<Napi::Uint8Array>();
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data.ByteLength(), &data_len)) return env.Null();

    int rc = csm_axdr_wr_octetstring(&array_, data.Data(), data_len);
    return Napi::Number::New(env, rc);
}

Napi::Value DataWrap::ReadU8(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint8_t val = 0;
    int rc = csm_array_read_u8(&array_, &val);
    if (rc == 0)
    {
        Napi::Error::New(env, "Read failed").ThrowAsJavaScriptException();
        return env.Null();
    }

    return Napi::Number::New(env, val);
}

Napi::Value DataWrap::ReadU16(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint16_t val = 0;
    int rc = csm_array_read_u16(&array_, &val);
    if (rc == 0)
    {
        Napi::Error::New(env, "Read failed").ThrowAsJavaScriptException();
        return env.Null();
    }

    return Napi::Number::New(env, val);
}

Napi::Value DataWrap::ReadU32(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    uint32_t val = 0;
    int rc = csm_array_read_u32(&array_, &val);
    if (rc == 0)
    {
        Napi::Error::New(env, "Read failed").ThrowAsJavaScriptException();
        return env.Null();
    }

    return Napi::Number::New(env, val);
}

Napi::Value DataWrap::ReadBuffer(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsNumber())
    {
        Napi::TypeError::New(env, "Argument: size (Number)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint32_t size = info[0].As<Napi::Number>().Uint32Value();
    uint8_t *tmp = new uint8_t[size];
    int rc = csm_array_read_buff(&array_, tmp, size);

    Napi::Value result;
    if (rc == 0)
    {
        result = env.Null();
    }
    else
    {
        result = Napi::Buffer<uint8_t>::Copy(env, tmp, size);
    }

    delete[] tmp;
    return result;
}

Napi::Value DataWrap::Written(const Napi::CallbackInfo &info)
{
    return Napi::Number::New(info.Env(), csm_array_written(&array_));
}

Napi::Value DataWrap::Unread(const Napi::CallbackInfo &info)
{
    return Napi::Number::New(info.Env(), csm_array_unread(&array_));
}

Napi::Value DataWrap::FreeSize(const Napi::CallbackInfo &info)
{
    return Napi::Number::New(info.Env(), csm_array_free_size(&array_));
}

Napi::Value DataWrap::ToBuffer(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    uint32_t written = csm_array_written(&array_);

    if (written == 0)
    {
        return Napi::Buffer<uint8_t>::Copy(env, buf_, 0);
    }

    return Napi::Buffer<uint8_t>::Copy(env, buf_, written);
}

Napi::Value DataWrap::Reset(const Napi::CallbackInfo &info)
{
    csm_array_init(&array_, buf_, buf_size_, 0, 0);
    return info.Env().Undefined();
}
