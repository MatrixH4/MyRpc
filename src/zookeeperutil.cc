#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include <semaphore.h>
#include <iostream>


ZkClient::ZkClient():m_zhandle(nullptr){

}
ZkClient::~ZkClient(){
    if(m_zhandle != nullptr){
        zookeeper_close(m_zhandle);//关闭句柄，释放资源
    }
}
// 全局的watcher观察器   zkserver给zkclient的通知
void global_watcher(zhandle_t *zh, int type,
    int state, const char *path, void *watcherCtx){
    // 回调的消息类型是和会话相关的消息类型
    if (type == ZOO_SESSION_EVENT){
        // zkclient和zkserver连接成功
        if (state == ZOO_CONNECTED_STATE){
            sem_t *sem = (sem_t*)zoo_get_context(zh);
            sem_post(sem);
        }
    }else if(type == ZOO_CHILD_EVENT){
        if (watcherCtx != nullptr) {
            ZkClient::ChildrenWatchCallback* cb = static_cast<ZkClient::ChildrenWatchCallback*>(watcherCtx);
            // 重新获取子节点列表并触发回调
            struct String_vector strings;
            int flag = zoo_wget_children(zh, path, global_watcher, watcherCtx, &strings);
            if (flag == ZOK) {
                std::vector<std::string> children;
                for (int i = 0; i < strings.count; ++i) {
                    children.push_back(std::string(strings.data[i]));
                }
                deallocate_String_vector(&strings);
                (*cb)(children); // 触发回调更新缓存
            }
        }
    }
}
//zkclient启动连接zkserver
void ZkClient::Start(){
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port;
    
	/*
	zookeeper_mt：多线程版本
	zookeeper的API客户端程序提供了三个线程
	API调用线程 
	网络I/O线程  pthread_create  poll
	watcher回调线程 pthread_create
	*/
    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, nullptr, 0);
    if (nullptr == m_zhandle){
        std::cout << "zookeeper_init error!" << std::endl;//获取句柄失败
        exit(EXIT_FAILURE);
    }

    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(m_zhandle, &sem);//设置上下文

    sem_wait(&sem);
    std::cout << "zookeeper_init success!" << std::endl;
}
//在zkserver上指定的path创建znode节点
void ZkClient::Create(const char *path, const char *data, int datalen, int state){
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;
	// 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
	flag = zoo_exists(m_zhandle, path, 0, nullptr);
    // path的znode节点不存在
	if (ZNONODE == flag){
		// 创建指定path的znode节点了
		flag = zoo_create(m_zhandle, path, data, datalen,
			&ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
		if (flag == ZOK){
			std::cout << "znode create success... path:" << path << std::endl;
		}else{
			std::cout << "flag:" << flag << std::endl;
			std::cout << "znode create error... path:" << path << std::endl;
			exit(EXIT_FAILURE);
		}
	}
}
void ZkClient::CreateLoadBalance1(const char *path, const char *data, int datalen, int state){
	if (zoo_exists(m_zhandle, path, 0, nullptr) == ZNONODE) {
        zoo_create(m_zhandle, path, nullptr, 0, &ZOO_OPEN_ACL_UNSAFE, 0, nullptr, 0);
    }
    
    // 2. 创建临时节点（实例节点）
    char instance_path[128];
    sprintf(instance_path, "%s/%s", path, data); // /UserService/Login/127.0.0.1:8000
    int flag = zoo_create(m_zhandle, instance_path, data, datalen,
                         &ZOO_OPEN_ACL_UNSAFE, state, nullptr, 0);
    if (flag == ZOK) {
		std::cout<<"Service instance registered: "<<instance_path<<std::endl;
    }
}
std::vector<std::string> ZkClient::GetChildren(const char* path){
	std::vector<std::string> children;
    struct String_vector strings; // ZooKeeper C API返回的子节点名称结构体

    // 调用ZooKeeper API获取子节点列表
    int flag = zoo_get_children(m_zhandle, path, 0, &strings);

    if (flag != ZOK) {
        std::cout<<"Failed to get children of path: "<< path << " error :" << flag<<std::endl;
        return children; // 返回空列表
    }

    // 将C风格的字符串数组转换为C++ vector<string>
    for (int i = 0; i < strings.count; ++i) {
        children.push_back(std::string(strings.data[i]));
    }

    // 释放ZooKeeper API分配的内存
    deallocate_String_vector(&strings);

    return children;
}
//从zk路径获取节点值
std::string ZkClient::GetData(const char* path){
    char buffer[64];
	int bufferlen = sizeof(buffer);
	int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
	if (flag != ZOK){
		std::cout << "get znode error... path:" << path << std::endl;
		return "";
	}
	else{
		return buffer;
	}
}

void ZkClient::WatchChildren(const char* path, ChildrenWatchCallback cb) {
    // 持久化回调对象（需动态分配）
    ChildrenWatchCallback* cb_ptr = new ChildrenWatchCallback(std::move(cb));
    
    // 注册Watcher并获取初始列表
    struct String_vector strings;
    int flag = zoo_wget_children(m_zhandle, path, global_watcher, cb_ptr, &strings);
    
    if (flag == ZOK) {
        std::vector<std::string> children;
        for (int i = 0; i < strings.count; ++i) {
            children.push_back(std::string(strings.data[i]));
        }
        deallocate_String_vector(&strings);
        (*cb_ptr)(children); // 初始化缓存
    } else {
        std::cout << "WatchChildren failed for path:" << path << std::endl;
        delete cb_ptr;
    }
}
