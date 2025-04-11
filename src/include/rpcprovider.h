#pragma once
#include "google/protobuf/service.h"
#include <mymuduo/TcpServer.h>
#include <mymuduo/EventLoop.h>
#include <mymuduo/InetAddress.h>
#include <mymuduo/TcpConnection.h>
#include <functional>
#include <google/protobuf/descriptor.h>
#include <string>
#include <unordered_map>

//框架提供的专门发布rpc服务的网络对象类，需要高并发
class RpcProvider{
public:
    void NotifyService(google::protobuf::Service* service);//输入参数是任意服务的抽象父类

    void Run();//启动RPC服务节点，提供远程网络调用服务
private:
    void OnConnection(const TcpConnectionPtr&);//新socket的连接回调
    void OnMessage(const TcpConnectionPtr&, Buffer*, Timestamp);//已建立连接的读写回调
    void SendRpcResponse(const TcpConnectionPtr&, google::protobuf::Message*);//closure回调，用于序列化response并网络发送
    //服务类型信息
    struct ServiceInfo{
        google::protobuf::Service *m_service;//保存服务对象
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap;//保存服务方法{方法名，方法指针}
    };
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;//存储注册成功的服务对象与方法的所有信息
    std::unique_ptr<TcpServer> m_tcpserverPtr;//组合tcpserver的智能指针
    EventLoop m_eventLoop;
};