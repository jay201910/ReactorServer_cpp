#include "TcpServer.h"
#include <arpa/inet.h>
#include "TcpConnection.h"
#include <stdio.h>
#include <stdlib.h>
#include "Log.h"

TcpServer::TcpServer(unsigned short port, int threadNum)
{
    m_port = port;
    m_mainLoop = new EventLoop;
    m_threadNum = threadNum;
    m_threadPool = new ThreadPool(m_mainLoop, threadNum);
    setListen();
}

TcpServer::~TcpServer()
{
    // 可以不提供析构函数，服务器关闭了tcpServer才会关闭
}

// 初始化监听
void TcpServer::setListen()
{
    // 1. 创建监听的fd
    m_lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_lfd == -1)
    {
        perror("socket");
        return;
    }
    // 2. 设置端口复用
    int opt = 1;
    int ret = setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (ret == -1)
    {
        perror("setsockopt");
        return;
    }
    // 3. 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(m_lfd, (struct sockaddr *)&addr, sizeof addr);
    if (ret == -1)
    {
        perror("bind");
        return;
    }
    // 4. 设置监听
    ret = listen(m_lfd, 128);
    if (ret == -1)
    {
        perror("listen");
        return;
    }
}

// 启动服务器
void TcpServer::run()
{
    Debug("服务器程序已经启动了...");
    // 启动线程池
    m_threadPool->run();
    // 添加检测的任务;将监听的fd封装成channnel，然后添加到主线程的任务中
    // 封装用于监听的channel
    Channel *channel = new Channel(m_lfd, FDEvent::ReadEvent, acceptConnection, nullptr, nullptr, this);
    m_mainLoop->addTask(channel, ElemType::ADD);
    // 启动反应堆模型
    m_mainLoop->run();
}

int TcpServer::acceptConnection(void *arg)
{
    // 类的静态成员函数不能直接访问该对象，故需要将TcpServer实例通过arg参数传给函数
    TcpServer *server = static_cast<TcpServer *>(arg);
    // 和客户端建立连接 得到用于通信的cfd
    int cfd = accept(server->m_lfd, NULL, NULL); // c函数
    // 从线程池中取出一个子线程的反应堆实例, 去处理这个cfd
    EventLoop *evLoop = server->m_threadPool->takeWorkerEventLoop();
    // 将cfd放到 TcpConnection中处理
    TcpConnection *conn = new TcpConnection(cfd, evLoop);
    new TcpConnection(cfd, evLoop);
    return 0;
}
