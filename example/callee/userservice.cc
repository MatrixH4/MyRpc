#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
#include "logger.h"

//原本是本地服务，提供本地方法
class UserService: public fixbug::UserServiceRpc{
public:
    bool Login(std::string name, std::string pwd){
        std::cout<<"doing local service: Login"<<std::endl;
        std::cout << "name:" << name << " pwd:" << pwd << std::endl;  
        return false;
    }

    //重写父类UserServiceRpc的虚函数，框架会直接调用下面的方法
    //1.caller(请求发起者)->Login(*)->muduo->callee
    //2.callee(RPC提供者)->Login(*)->交给下面的Login方法
    void Login(::google::protobuf::RpcController* controller,
        const ::fixbug::LoginRequest* request,
        ::fixbug::LoginResponse* response,
        ::google::protobuf::Closure* done){
        //接收caller发来的请求：执行本函数所需的参数
        std::string name = request->name();
        std::string pwd = request->pwd();
        //根据参数执行本地业务
        bool login_result = Login(name, pwd);
        //将result写入响应
        fixbug::ResultCode* code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_success(login_result);
        //执行回调done
        done->Run();
    }
};

int main(int argc, char** argv){
    // LOG_INFO("%s:%s:%d", __FILE__, __FUNCTION__, __LINE__);
    //初始化RPC框架
    MprpcApplication::Init(argc, argv);
    //provider将对象发布到RPC节点上
    RpcProvider provider;
    provider.NotifyService(new UserService());

    //启动一个RPC服务发布节点，run之后，进程阻塞，等待远程的RPC调用
    provider.Run();
}