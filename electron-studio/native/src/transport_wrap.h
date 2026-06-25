/**
 * \file transport_wrap.h
 * \brief Node.js wrapper for csm_transport
 */

#ifndef TRANSPORT_WRAP_H
#define TRANSPORT_WRAP_H

#include <napi.h>
#include "csm_transport.h"

class TransportWrap : public Napi::ObjectWrap<TransportWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);
    TransportWrap(const Napi::CallbackInfo &info);
    ~TransportWrap();

    /* Methods exposed to JS */
    Napi::Value Connect(const Napi::CallbackInfo &info);
    Napi::Value Send(const Napi::CallbackInfo &info);
    Napi::Value Receive(const Napi::CallbackInfo &info);
    Napi::Value Close(const Napi::CallbackInfo &info);
    Napi::Value IsConnected(const Napi::CallbackInfo &info);

private:
    csm_transport *transport_;
    bool connected_;
    char host_[256];
    uint16_t port_;
};

#endif /* TRANSPORT_WRAP_H */
