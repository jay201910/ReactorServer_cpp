#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"

class TcpServer
{
public:
    TcpServer(unsigned short port, int threadNum);
    ~TcpServer();
    // 初始化监听
    void setListen();
    // 启动服务器
    void run();
    static int acceptConnection(void *arg); // 创建channel时所用的回调函数，这个函数也可以用function包装，这里写成了静态成员函数

private:
    int m_threadNum;
    EventLoop *m_mainLoop;
    ThreadPool *m_threadPool;
    int m_lfd;             // 服务器用于监听的fd
    unsigned short m_port; // 服务器监听的端口
};
