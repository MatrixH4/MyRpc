#pragma once
#include <google/protobuf/service.h>
#include <string>

class MprpcController: public google::protobuf::RpcController{
public:
    MprpcController();
    void Reset();
    bool Failed() const;
    std::string ErrorText() const;
    void SetFailed(const std::string& reason);
    //目前未实现具体功能
    void StartCancel();
    bool IsCanceled() const;
    void NotifyOnCancel(google::protobuf::Closure* callback);
private:
    bool m_failed;//RPC方法执行中的状态
    std::string m_errText;//RPC方法执行中的错误信息
};