/**
 * \file client_wrap.cpp
 * \brief Node.js wrapper for csm_client
 */

#include "client_wrap.h"
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

static bool parse_obis(const std::string &text, csm_obis_code *obis)
{
    if (!obis) return false;

    uint32_t values[6] = {0, 0, 0, 0, 0, 0};
    uint32_t part = 0U;
    bool have_digit = false;

    for (char ch : text)
    {
        if (ch >= '0' && ch <= '9')
        {
            have_digit = true;
            values[part] = values[part] * 10U + (uint32_t)(ch - '0');
            if (values[part] > 255U)
            {
                return false;
            }
        }
        else if (ch == '.')
        {
            if (!have_digit || part >= 5U)
            {
                return false;
            }
            part++;
            have_digit = false;
        }
        else
        {
            return false;
        }
    }

    if (!have_digit || part != 5U)
    {
        return false;
    }

    obis->A = (uint8_t)values[0];
    obis->B = (uint8_t)values[1];
    obis->C = (uint8_t)values[2];
    obis->D = (uint8_t)values[3];
    obis->E = (uint8_t)values[4];
    obis->F = (uint8_t)values[5];
    return true;
}

Napi::Function ClientWrap::Init(Napi::Env env, Napi::Object exports)
{
    Napi::Function func = DefineClass(env, "Client", {
        InstanceMethod("connect", &ClientWrap::Connect),
        InstanceMethod("get", &ClientWrap::Get),
        InstanceMethod("set", &ClientWrap::Set),
        InstanceMethod("action", &ClientWrap::Action),
        InstanceMethod("getBlock", &ClientWrap::GetBlock),
        InstanceMethod("setBlock", &ClientWrap::SetBlock),
        InstanceMethod("disconnect", &ClientWrap::Disconnect),
        InstanceMethod("destroy", &ClientWrap::Destroy),
    });

    exports.Set("Client", func);
    return func;
}

ClientWrap::ClientWrap(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<ClientWrap>(info), client_(nullptr), transport_(nullptr), initialized_(false)
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

    client_ = csm_client_create(&transport_->transport_, 0, CSM_FRAMING_WRAPPER);
    if (!client_)
    {
        Napi::Error::New(env, "Failed to initialize client").ThrowAsJavaScriptException();
    }
}

ClientWrap::~ClientWrap()
{
    if (initialized_)
    {
        csm_client_disconnect(client_);
    }
    csm_client_delete(client_);
    client_ = nullptr;
}

Napi::Value ClientWrap::Connect(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!client_)
    {
        Napi::Error::New(env, "Client not initialized").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint32_t timeout_ms = 5000;
    if (info.Length() >= 1 && info[0].IsNumber())
    {
        timeout_ms = info[0].As<Napi::Number>().Uint32Value();
    }

    int rc = csm_client_connect(client_, timeout_ms);
    if (rc == 0)
    {
        initialized_ = true;
    }

    return Napi::Number::New(env, rc);
}

Napi::Value ClientWrap::Get(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!initialized_)
    {
        Napi::Error::New(env, "Client not connected").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsString() || !info[3].IsNumber())
    {
        Napi::TypeError::New(env, "Arguments: invokeId, classId, obis(String), attrId").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = info[1].As<Napi::Number>().Uint32Value();
    std::string obis_str = info[2].As<Napi::String>().Utf8Value();
    uint8_t attr_id = info[3].As<Napi::Number>().Uint32Value();

    csm_obis_code obis = {0, 0, 0, 0, 0, 0};
    if (!parse_obis(obis_str, &obis))
    {
        Napi::TypeError::New(env, "Invalid OBIS code").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t resp_buf[4096];
    int rc = csm_client_get(client_, invoke_id, class_id, &obis, attr_id, resp_buf, sizeof(resp_buf));

    if (rc < 0)
    {
        return Napi::Number::New(env, rc);
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp_buf, rc);
}

Napi::Value ClientWrap::Set(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!initialized_)
    {
        Napi::Error::New(env, "Client not connected").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (info.Length() < 5 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsString() || !info[3].IsNumber() || !info[4].IsBuffer())
    {
        Napi::TypeError::New(env, "Arguments: invokeId, classId, obis(String), attrId, data(Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = info[1].As<Napi::Number>().Uint32Value();
    std::string obis_str = info[2].As<Napi::String>().Utf8Value();
    uint8_t attr_id = info[3].As<Napi::Number>().Uint32Value();
    Napi::Uint8Array data_arr = info[4].As<Napi::Uint8Array>();
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data_arr.ByteLength(), &data_len)) return env.Null();

    csm_obis_code obis = {0, 0, 0, 0, 0, 0};
    if (!parse_obis(obis_str, &obis))
    {
        Napi::TypeError::New(env, "Invalid OBIS code").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t resp_buf[4096];
    int rc = csm_client_set(client_, invoke_id, class_id, &obis, attr_id,
                            data_arr.Data(), data_len,
                            resp_buf, sizeof(resp_buf));

    if (rc < 0)
    {
        return Napi::Number::New(env, rc);
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp_buf, rc);
}

Napi::Value ClientWrap::Action(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!initialized_)
    {
        Napi::Error::New(env, "Client not connected").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (info.Length() < 5 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsString() || !info[3].IsNumber() || !info[4].IsBuffer())
    {
        Napi::TypeError::New(env, "Arguments: invokeId, classId, obis(String), methodId, data(Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = info[1].As<Napi::Number>().Uint32Value();
    std::string obis_str = info[2].As<Napi::String>().Utf8Value();
    uint8_t method_id = info[3].As<Napi::Number>().Uint32Value();
    Napi::Uint8Array data_arr = info[4].As<Napi::Uint8Array>();
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data_arr.ByteLength(), &data_len)) return env.Null();

    csm_obis_code obis = {0, 0, 0, 0, 0, 0};
    if (!parse_obis(obis_str, &obis))
    {
        Napi::TypeError::New(env, "Invalid OBIS code").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t resp_buf[4096];
    int rc = csm_client_action(client_, invoke_id, class_id, &obis, method_id,
                               data_arr.Data(), data_len,
                               resp_buf, sizeof(resp_buf));

    if (rc < 0)
    {
        return Napi::Number::New(env, rc);
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp_buf, rc);
}

Napi::Value ClientWrap::GetBlock(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!initialized_)
    {
        Napi::Error::New(env, "Client not connected").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (info.Length() < 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsString() || !info[3].IsNumber())
    {
        Napi::TypeError::New(env, "Arguments: invokeId, classId, obis(String), attrId").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = info[1].As<Napi::Number>().Uint32Value();
    std::string obis_str = info[2].As<Napi::String>().Utf8Value();
    uint8_t attr_id = info[3].As<Napi::Number>().Uint32Value();

    csm_obis_code obis = {0, 0, 0, 0, 0, 0};
    if (!parse_obis(obis_str, &obis))
    {
        Napi::TypeError::New(env, "Invalid OBIS code").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t resp_buf[65536];
    int rc = csm_client_get_block(client_, invoke_id, class_id, &obis, attr_id, resp_buf, sizeof(resp_buf));

    if (rc < 0)
    {
        return Napi::Number::New(env, rc);
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp_buf, rc);
}

Napi::Value ClientWrap::SetBlock(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!initialized_)
    {
        Napi::Error::New(env, "Client not connected").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (info.Length() < 5 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsString() || !info[3].IsNumber() || !info[4].IsBuffer())
    {
        Napi::TypeError::New(env, "Arguments: invokeId, classId, obis(String), attrId, data(Buffer)").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t invoke_id = info[0].As<Napi::Number>().Uint32Value();
    uint16_t class_id = info[1].As<Napi::Number>().Uint32Value();
    std::string obis_str = info[2].As<Napi::String>().Utf8Value();
    uint8_t attr_id = info[3].As<Napi::Number>().Uint32Value();
    Napi::Uint8Array data_arr = info[4].As<Napi::Uint8Array>();
    uint32_t data_len = 0U;
    if (!buffer_len_u32(env, data_arr.ByteLength(), &data_len)) return env.Null();

    csm_obis_code obis = {0, 0, 0, 0, 0, 0};
    if (!parse_obis(obis_str, &obis))
    {
        Napi::TypeError::New(env, "Invalid OBIS code").ThrowAsJavaScriptException();
        return env.Null();
    }

    uint8_t resp_buf[4096];
    int rc = csm_client_set_block(client_, invoke_id, class_id, &obis, attr_id,
                                  data_arr.Data(), data_len,
                                  resp_buf, sizeof(resp_buf));

    if (rc < 0)
    {
        return Napi::Number::New(env, rc);
    }

    return Napi::Buffer<uint8_t>::Copy(env, resp_buf, rc);
}

Napi::Value ClientWrap::Disconnect(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!initialized_)
    {
        return Napi::Number::New(env, 0);
    }

    int rc = csm_client_disconnect(client_);
    initialized_ = false;

    return Napi::Number::New(env, rc);
}

Napi::Value ClientWrap::Destroy(const Napi::CallbackInfo &info)
{
    if (initialized_)
    {
        csm_client_disconnect(client_);
        initialized_ = false;
    }

    if (client_)
    {
        csm_client_delete(client_);
        client_ = nullptr;
    }

    return info.Env().Undefined();
}
