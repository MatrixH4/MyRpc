#pragma once
#include "mprpcconfig.h"
#include "mprpccontroller.h"

//rpc框架基础类：使用单例模式
class MprpcApplication{
public:
    static void Init(int argc, char** argv);//RPC框架初始化函数
    static MprpcApplication& GetInstance();//获取类的唯一对象
    static MprpcConfig& GetConfig();
private:
    MprpcApplication(){}
    MprpcApplication(const MprpcApplication&) = delete;//删除拷贝构造与移动构造
    MprpcApplication(MprpcApplication&&) = delete;

    static MprpcConfig m_config;
};