#include "mprpcconfig.h"
#include <iostream>
#include <string>

//去除字符串多余的空格

void MprpcConfig::LoadConfigFile(const char* config_file){
    FILE* pf = fopen(config_file, "r");//打开命令行传入的配置文件
    if(nullptr == pf){
        std::cout<< config_file <<" not exits!"<<std::endl;
        exit(EXIT_FAILURE);
    }
    //处理注释，正确的配置项，去掉开头多余空格
    while(!feof(pf)){
        char buf[512]={0};
        fgets(buf, 512, pf);
        //去掉字符串前多余空格
        std::string read_buf(buf);
        Trim(read_buf);
        //#注释
        if(read_buf[0] == '#' || read_buf.empty()){
            continue;
        }
        //解析配置项
        int idx = read_buf.find('=');
        if(idx == -1){
            continue;//配置项不合法
        }
        std::string key;
        std::string value;
        key = read_buf.substr(0, idx);
        Trim(key);
        int endidx = read_buf.find('\n',idx);
        value = read_buf.substr(idx+1, endidx-idx-1);
        Trim(value);
        m_configMap.insert({key, value});
    }
}
std::string MprpcConfig::Load(const std::string &key){
    auto it = m_configMap.find(key);
    if(it == m_configMap.end()){
        return "";
    }
    return it->second;
}

void MprpcConfig::Trim(std::string &src_buf){
    int idx = src_buf.find_first_not_of(' ');
    if(idx != -1){
        //存在空格，剪切字符串
        src_buf = src_buf.substr(idx, src_buf.size() - idx);
    }
    //去除字符串后多余的空格
    idx = src_buf.find_last_not_of(' ');
    if(idx != -1){
        src_buf = src_buf.substr(0, idx+1);
    }
}