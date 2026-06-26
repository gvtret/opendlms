/**
 * \file server_wrap.h
 * \brief Node.js wrapper for csm_server
 */

#ifndef SERVER_WRAP_H
#define SERVER_WRAP_H

#include <napi.h>
#include "csm_server.h"
#include "transport_wrap.h"

class ServerWrap : public Napi::ObjectWrap<ServerWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);
    ServerWrap(const Napi::CallbackInfo &info);
    ~ServerWrap();

    Napi::Value Poll(const Napi::CallbackInfo &info);
    Napi::Value Send(const Napi::CallbackInfo &info);
    Napi::Value Destroy(const Napi::CallbackInfo &info);

private:
    csm_server *server_;
    TransportWrap *transport_;
    bool initialized_;
};

#endif /* SERVER_WRAP_H */
