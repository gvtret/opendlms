/**
 * \file data_wrap.h
 * \brief Node.js wrapper for csm_array (AXDR data buffer)
 */

#ifndef DATA_WRAP_H
#define DATA_WRAP_H

#include <napi.h>
#include "csm_array.h"

class DataWrap : public Napi::ObjectWrap<DataWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);
    DataWrap(const Napi::CallbackInfo &info);
    ~DataWrap();

    Napi::Value WriteU8(const Napi::CallbackInfo &info);
    Napi::Value WriteU16(const Napi::CallbackInfo &info);
    Napi::Value WriteU32(const Napi::CallbackInfo &info);
    Napi::Value WriteBuffer(const Napi::CallbackInfo &info);
    Napi::Value WriteBoolean(const Napi::CallbackInfo &info);
    Napi::Value WriteOctetString(const Napi::CallbackInfo &info);
    Napi::Value ReadU8(const Napi::CallbackInfo &info);
    Napi::Value ReadU16(const Napi::CallbackInfo &info);
    Napi::Value ReadU32(const Napi::CallbackInfo &info);
    Napi::Value ReadBuffer(const Napi::CallbackInfo &info);
    Napi::Value Written(const Napi::CallbackInfo &info);
    Napi::Value Unread(const Napi::CallbackInfo &info);
    Napi::Value FreeSize(const Napi::CallbackInfo &info);
    Napi::Value ToBuffer(const Napi::CallbackInfo &info);
    Napi::Value Reset(const Napi::CallbackInfo &info);

private:
    uint8_t *buf_;
    uint32_t buf_size_;
    csm_array array_;
};

#endif /* DATA_WRAP_H */
