/**
 * \file transport_wrap.h
 * \brief Node.js wrapper for csm_transport
 */

#ifndef TRANSPORT_WRAP_H
#define TRANSPORT_WRAP_H

#include <napi.h>
#include "csm_transport.h"
#include "csm_transport_tcp.h"

class TransportWrap : public Napi::ObjectWrap<TransportWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);
    TransportWrap(const Napi::CallbackInfo &info);
    ~TransportWrap();

    Napi::Value ClientInit(const Napi::CallbackInfo &info);
    Napi::Value ServerInit(const Napi::CallbackInfo &info);
    Napi::Value Connect(const Napi::CallbackInfo &info);
    Napi::Value Accept(const Napi::CallbackInfo &info);
    Napi::Value GetChannel(const Napi::CallbackInfo &info);
    Napi::Value IsConnected(const Napi::CallbackInfo &info);
    Napi::Value Destroy(const Napi::CallbackInfo &info);

private:
    bool initialized_;

public:
    csm_transport transport_;
};

#endif /* TRANSPORT_WRAP_H */
