/**
 * \file block_wrap.h
 * \brief Node.js wrapper for csm_block_state (block transfer)
 */

#ifndef BLOCK_WRAP_H
#define BLOCK_WRAP_H

#include <napi.h>
#include "csm_block_transfer.h"
#include "data_wrap.h"

class BlockWrap : public Napi::ObjectWrap<BlockWrap>
{
public:
    static Napi::Function Init(Napi::Env env, Napi::Object exports);
    BlockWrap(const Napi::CallbackInfo &info);
    ~BlockWrap();

    Napi::Value StartServer(const Napi::CallbackInfo &info);
    Napi::Value EncodeFirst(const Napi::CallbackInfo &info);
    Napi::Value EncodeNext(const Napi::CallbackInfo &info);
    Napi::Value IsActive(const Napi::CallbackInfo &info);
    Napi::Value Abort(const Napi::CallbackInfo &info);
    Napi::Value StartClient(const Napi::CallbackInfo &info);
    Napi::Value EncodeSetRequest(const Napi::CallbackInfo &info);
    Napi::Value EncodeSetNext(const Napi::CallbackInfo &info);
    Napi::Value StartGetReceive(const Napi::CallbackInfo &info);
    Napi::Value GetReceiveData(const Napi::CallbackInfo &info);
    Napi::Value GetReceivedData(const Napi::CallbackInfo &info);
    Napi::Value EncodeGetNext(const Napi::CallbackInfo &info);
    Napi::Value StartReceive(const Napi::CallbackInfo &info);
    Napi::Value ReceiveData(const Napi::CallbackInfo &info);
    Napi::Value GetReceived(const Napi::CallbackInfo &info);
    Napi::Value EncodeSetResponse(const Napi::CallbackInfo &info);
    Napi::Value CanReceive(const Napi::CallbackInfo &info);
    Napi::Value Init(const Napi::CallbackInfo &info);

private:
    csm_block_state state_;
    bool active_;
};

#endif /* BLOCK_WRAP_H */
