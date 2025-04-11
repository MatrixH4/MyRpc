#include "mprpcchannel.h"
#include <string>
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>
#include "mprpcapplication.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include "mprpccontroller.h"
#include "zookeeperutil.h"
#include <atomic>
#include <mutex>
#include <atomic>

/**
 * header_size service_name method_name args_size args
 */
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method, 
                                google::protobuf::RpcController* controller, 
                                const google::protobuf::Message* request, 
                                google::protobuf::Message* response, 
                                google::protobuf::Closure* done){
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name();//service_name,从方法获取所在的服务
    std::string method_name = method->name();//method_name
    //获取参数的序列化字符串长度
    std::string args_str;
    int args_size = 0;
    if(request->SerializeToString(&args_str)){
        args_size = args_str.size();
    }else{
        controller->SetFailed("serialize request error!");
        return;
    }
    //定义请求头
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if(rpcHeader.SerializeToString(&rpc_header_str)){
        header_size = rpc_header_str.size();
    }else{
        controller->SetFailed("serialize rpc header error!");
        return;
    }
    //组织待发送的rpc请求字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4));//header_szie
    send_rpc_str += rpc_header_str; //请求头
    send_rpc_str += args_str;//参数

    //打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl; 
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl; 
    std::cout << "service_name: " << service_name << std::endl; 
    std::cout << "method_name: " << method_name << std::endl; 
    std::cout << "args_str: " << args_str << std::endl; 
    std::cout << "============================================" << std::endl;
    //tcp编程，完成远程调用
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd){
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno: %d", errno);
        controller->SetFailed(errtxt);
        exit(EXIT_FAILURE);
    }
    //读取配置文件rpcserver信息
    // std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    // uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    //调用方查询zk，获取服务与方法
    ZkClient zkCli;
    zkCli.Start();
    //  /UserServiceRpc/Login
    std::string method_path = "/" + service_name + "/" + method_name;
    // 127.0.0.1:8000

    // std::string host_data = zkCli.GetData(method_path.c_str());
    // if (host_data == ""){
    //     controller->SetFailed(method_path + " is not exist!");
    //     return;
    // }

    static std::vector<std::string> cached_instances;
    static std::mutex cache_mutex;
    static std::atomic<size_t> current_index(0);
    
    // 首次调用时初始化缓存和监听
    static std::once_flag init_flag;
    std::call_once(init_flag, [&]() {
        zkCli.WatchChildren(method_path.c_str(), [](const std::vector<std::string>& new_instances) {
            std::lock_guard<std::mutex> lock(cache_mutex);
            cached_instances = new_instances; // 动态更新缓存
            current_index.store(0); // 重置轮询索引
            std::cout<<"Service instances updated, count: "<<cached_instances.size()<<std::endl;
        });
    });

    std::vector<std::string> instances = zkCli.GetChildren(method_path.c_str());
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        instances = cached_instances; // 读取缓存
    }
    if (instances.empty()) {
        controller->SetFailed("No available service instances");
        return;
    }
    // 计算当前应选择的节点索引
    size_t index = current_index.fetch_add(1, std::memory_order_relaxed) % instances.size();
    std::string host_data = instances[index];  // 获取选定节点的数据（ip:port）

    int idx = host_data.find(":");
    if (idx == -1){
        controller->SetFailed(method_path + " address is invalid!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx+1, host_data.size()-idx).c_str());

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    server_addr.sin_port = htons(port);
    //发起连接
    if(-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr))){
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! errno: %d", errno);
        controller->SetFailed(errtxt);
        exit(EXIT_FAILURE);
    }
    //发送rpc请求
    if(-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0)){
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno: %d", errno);
        controller->SetFailed(errtxt);
        return;
    }
    //接收rpc响应
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if(-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0))){
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno: %d", errno);
        controller->SetFailed(errtxt);
        return;
    }
    //反序列化RPC调用的响应数据
    // std::string response_str(recv_buf, 0, recv_size);//问题1：buffer中遇到\0,后续数据存不下来，反序列化出现问腿
    if(!response->ParseFromArray(recv_buf, recv_size)){
        close(clientfd);
        char errtxt[1450] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }
    close(clientfd);
}
