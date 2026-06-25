/**
 * \file server_wrap.h
 * \brief Node.js wrapper for csm_server
 */

#ifndef SERVER_WRAP_H
#define SERVER_WRAP_H

#include <napi.h>
#include "csm_server.h"

class ServerWrap : public Napi::ObjectWrap<ServerWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);
    ServerWrap(const Napi::CallbackInfo &info);
    ~ServerWrap();

    Napi::Value InitServer(const Napi::CallbackInfo &info);
    Napi::Value RegisterDB(const Napi::CallbackInfo &info);
    Napi::Value Poll(const Napi::CallbackInfo &info);
    Napi::Value SendUnsolicited(const Napi::CallbackInfo &info);
    Napi::Value Destroy(const Napi::CallbackInfo &info);

private:
    csm_server *server_;
    Napi::FunctionReference db_callback_;
};

#endif /* SERVER_WRAP_H */
