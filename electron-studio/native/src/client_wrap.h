/**
 * \file client_wrap.h
 * \brief Node.js wrapper for csm_client
 */

#ifndef CLIENT_WRAP_H
#define CLIENT_WRAP_H

#include <napi.h>
#include "csm_server.h"

class ClientWrap : public Napi::ObjectWrap<ClientWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);
    ClientWrap(const Napi::CallbackInfo &info);
    ~ClientWrap();

    Napi::Value Connect(const Napi::CallbackInfo &info);
    Napi::Value Disconnect(const Napi::CallbackInfo &info);
    Napi::Value Get(const Napi::CallbackInfo &info);
    Napi::Value GetBlock(const Napi::CallbackInfo &info);
    Napi::Value Set(const Napi::CallbackInfo &info);
    Napi::Value SetBlock(const Napi::CallbackInfo &info);
    Napi::Value Action(const Napi::CallbackInfo &info);

private:
    csm_client *client_;
};

#endif /* CLIENT_WRAP_H */
