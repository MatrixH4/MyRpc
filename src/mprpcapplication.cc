#include "mprpcapplication.h"
#include <iostream>
#include <unistd.h>
#include <string>

MprpcConfig MprpcApplication::m_config;

void ShowArgsHelp(){
    std::cout<< "format: command -i <configfile>"<<std::endl;
}
//RPC框架初始化函数
//输入参数：可能会读取一些配置文件
//provider -i config.conf
void MprpcApplication::Init(int argc, char** argv){
    if(argc < 2){
        //传入参数少
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }
    int c = 0;
    std::string config_file;
    while((c = getopt(argc, argv, "i:")) != -1){
        switch (c){
            case 'i':
                config_file = optarg;
                break;
            case '?':
                ShowArgsHelp();
                exit(EXIT_FAILURE);
                break;
            case ':':
                ShowArgsHelp();
                exit(EXIT_FAILURE);
                break;
            default:
                break;
        }
    }
    //加载配置文件 rpcserver_ip rpcserver_port zookeeper_ip zookeeper_port
    m_config.LoadConfigFile(config_file.c_str());
    std::cout<<"rpcserverip:"<<m_config.Load("rpcserverip")<<std::endl;
    std::cout<<"rpcserverport:"<<m_config.Load("rpcserverport")<<std::endl;
    std::cout<<"zookeeperip:"<<m_config.Load("zookeeperip")<<std::endl;
    std::cout<<"zookeeperport:"<<m_config.Load("zookeeperport")<<std::endl;
}
//获取唯一app对象
MprpcApplication& MprpcApplication::GetInstance(){
    static MprpcApplication app;
    return app;
}

MprpcConfig& MprpcApplication::GetConfig(){
    return m_config;
}