#pragma once

#include <zookeeper/zookeeper.h>
#include <semaphore.h>
#include <string>
#include <vector>
#include <functional>

class ZkClient{
public:
    using ChildrenWatchCallback = std::function<void(const std::vector<std::string>&)>;

    ZkClient();
    ~ZkClient();
    
    void Start();//zkclient启动连接zkserver
    void Create(const char *path, const char *data, int datalen, int state=0);//在zkserver上指定的path创建znode节点
    void CreateLoadBalance1(const char *path, const char *data, int datalen, int state=0);
    std::string GetData(const char* path);//从zk路径获取节点值
    std::vector<std::string> GetChildren(const char* path);
    void WatchChildren(const char* path, ChildrenWatchCallback cb);
private:
    zhandle_t* m_zhandle;//zk客户端的句柄
};
