#include <iostream>
#include "mprpcapplication.h"
#include "mprpcchannel.h"
#include "user.pb.h"
int main(int argc, char** argv){
    //使用rpc服务前，需要先初始化函数
    MprpcApplication::Init(argc, argv);
    
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());
    //rpc方法的请求参数
    fixbug::LoginRequest request;
    request.set_name("zhangsan");
    request.set_pwd("123456");
    //rpc方法的响应
    fixbug::LoginResponse response;
    //发起调用(同步)callmethod
    MprpcController controller;
    stub.Login(&controller, &request, &response, nullptr);
    //一次rpc调用完成,读取调用结果
    if(controller.Failed()){
        std::cout<<controller.ErrorText()<<std::endl;
    }else{
        if(response.result().errcode()==0){
            std::cout<<"rpc login response: "<<response.success()<<std::endl;
        }else{
            std::cout<<"rpc login response error: "<<response.result().errmsg()<<std::endl;
        }
    }
}