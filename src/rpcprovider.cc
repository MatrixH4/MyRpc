#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "zookeeperutil.h"

//输入参数是任意服务的抽象父类
void RpcProvider::NotifyService(google::protobuf::Service* service){
    ServiceInfo service_info;

    const google::protobuf::ServiceDescriptor* pserviceDesc = service->GetDescriptor();//获取服务对象的描述信息
    
    std::string service_name = pserviceDesc->name();//获取服务名
    int methodCnt = pserviceDesc->method_count();//获取方法数量
    std::cout<<"service name: "<<service_name<<std::endl;
    for(int i = 0; i < methodCnt; ++i){
        //获取服务对象指定下标的服务方法的描述（抽象）
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});
        std::cout<<"method name: "<<method_name<<std::endl;
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

//启动RPC服务节点，提供远程网络调用服务
void RpcProvider::Run(){
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    InetAddress address(port, ip);
    //创建TCPServer对象
    TcpServer server(&m_eventLoop, address, "RpcProvider");
    //绑定连接回调与消息读写回调函数
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMesageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    //设置muduo库线程数
    server.setThreadNum(4);
    //将当前rpc节点上发布的数据全注册到zk上，让rpc_client发现服务
    // session timeout   30s     zkclient 网络I/O线程  1/3 * timeout 时间发送ping消息
    ZkClient zkCli;
    zkCli.Start();
    // service_name为永久性节点    method_name为临时性节点
    for (auto &sp : m_serviceMap) 
    {
        //先创建父节点，再创建子节点
        // /service_name   /UserServiceRpc
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);
        for (auto &mp : sp.second.m_methodMap)
        {
            // /service_name/method_name   /UserServiceRpc/Login 存储当前这个rpc服务节点主机的ip和port
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            // ZOO_EPHEMERAL表示znode是一个临时性节点
            // zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
            zkCli.CreateLoadBalance1(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }
    //启动网络服务
    server.start();
    m_eventLoop.loop();
}

void RpcProvider::OnConnection(const TcpConnectionPtr& conn){
    if(!conn->connected()){
        //断开连接
        conn->shutdown();
    }
}

//框架内部RPCprovider与RPCconsumer协商好通信用的protobuf数据类型
//service_name method_name args_size args 定义proto的message类型，进行数据头的序列化与反序列化
//数据头长度4字节
void RpcProvider::OnMessage(const TcpConnectionPtr&conn, Buffer* buffer, Timestamp){
    std::string recv_buf = buffer->retrieveAllAsString();//网络上接收的远程RPC调用请求的字符流
    //从字符流中读取前4字节:数据头长度固定4字节存储
    uint32_t header_size = 0;
    recv_buf.copy((char*)&header_size, 4, 0);//从0开始读4个字节，存储到header_size中
    //根据header_size读取数据头的原始字符流
    std::string rpc_header_str = recv_buf.substr(4, header_size);//读service_name method_name args_size
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if(rpcHeader.ParseFromString(rpc_header_str)){
        //数据头反序列化成功,获取数据头数据
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }else{
        //数据头反序列化失败
        std::cout<<"rpc_header_str: "<<rpc_header_str<<"parse error!"<<std::endl;
        return;
    }
    //获取rpc方法参数
    std::string args_str = recv_buf.substr(4+header_size, args_size);//越过数据头读参数
    //打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl; 
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl; 
    std::cout << "service_name: " << service_name << std::endl; 
    std::cout << "method_name: " << method_name << std::endl; 
    std::cout << "args_str: " << args_str << std::endl; 
    std::cout << "============================================" << std::endl;

    //获取service对象与method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end()){
        std::cout<<service_name<<" not exist!"<<std::endl;
        return;
    }
    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end()){
        std::cout<<service_name<<": "<<method_name<<"not exist!"<<std::endl;
        return;
    }
    google::protobuf::Service* service = it->second.m_service;//获取service对象
    const google::protobuf::MethodDescriptor* method = mit->second;//获取method对象
    //生成rpc方法调用的请求与响应参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(args_str)){
        std::cout<<"request parse error! content: "<< args_str<<std::endl;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();
    //绑定closure回调:注意，如果不指定泛型，自动推导会将message*推导成message*&
    google::protobuf::Closure* done = google::protobuf::NewCallback<RpcProvider, const TcpConnectionPtr&, google::protobuf::Message*>(this, &RpcProvider::SendRpcResponse, conn, response);
    //在框架上根据远端RPC请求，调用当前RPC节点上发布的方法
    service->CallMethod(method, nullptr, request, response, done);//调用
}
//closure回调，用于序列化response并网络发送
void RpcProvider::SendRpcResponse(const TcpConnectionPtr& conn, google::protobuf::Message* response){
    //序列化
    std::string response_str;
    if(response->SerializeToString(&response_str)){ //response序列化
        //成功后，通过网络将rpc方法执行结果发送回调用方
        conn->send(response_str);
        
    }else{
        std::cout<<"serialize response_str error"<<std::endl;
    }
    conn->shutdown();//由rpcprovider主动断开连接
}