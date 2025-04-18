#pragma once
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

class MprpcChannel : public google::protobuf::RpcChannel{
public:
    //所有通过stub代理的对象调用的rpc方法都会走到这里，统一做rpc方法调用的数据序列化与网络发送
    void CallMethod(const google::protobuf::MethodDescriptor* method, 
                    google::protobuf::RpcController* controller, 
                    const google::protobuf::Message* request, 
                    google::protobuf::Message* response, 
                    google::protobuf::Closure* done) override;
    ~MprpcChannel(){}
};