#include "TcpConnection.h"
#include "HttpRequest.h"
#include <stdlib.h>
#include <stdio.h>
#include "Log.h"

TcpConnection::TcpConnection(int fd, EventLoop *evLoop) // 用于通信的fd，所属的eventLoop
{
    m_evLoop = evLoop;
    m_readBuf = new Buffer(10240);
    m_writeBuf = new Buffer(10240);
    m_request = new HttpRequest();
    m_response = new HttpResponse();
    m_name = "Connection-" + to_string(fd);
    m_channel = new Channel(fd, FDEvent::ReadEvent, processRead, processWrite, destroy, this);
    evLoop->addTask(m_channel, ElemType::ADD);
}

TcpConnection::~TcpConnection()
{
    if (m_readBuf && m_readBuf->readableSize() == 0 && m_writeBuf && m_writeBuf->readableSize() == 0) // 如果buf不为空且数据读完了
    {
        delete m_readBuf;
        delete m_writeBuf;
        delete m_request;
        delete m_response;
        m_evLoop->freeChannel(m_channel);
    }
    Debug("连接断开, 释放资源, gameover, connName: %s", m_name.data());
}

int TcpConnection::processRead(void *arg)
{
    TcpConnection *conn = static_cast<TcpConnection *>(arg); // 类静态函数不能直接访问类成员
    // 接收数据
    int socket = conn->m_channel->getSocket();
    int count = conn->m_readBuf->socketRead(socket);
    Debug("接收到的http请求数据: %s", conn->m_readBuf->getReadPos());
    if (count > 0)
    {
        // 接收到了 http 请求, 解析http请求
#ifdef MSG_SEND_AUTO
        // 通过channel的写回调函数 来发送数据
        conn->m_channel->writeEventEnable(true);                    // 要往fd写数据
        conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY); // 本来是只检测fd的读事件，现在改为检测读写事件
#endif
        bool flag = conn->m_request->parseHttpRequest(conn->m_readBuf, conn->m_response, conn->m_writeBuf, socket);
        if (!flag)
        {
            // 解析失败, 回复一个简单的html
            const string errMsg = "Http/1.1 400 Bad Request\r\n\r\n";
            conn->m_writeBuf->appendString(errMsg);
        }
    }
    else
    {
#ifdef MSG_SEND_AUTO
        // 断开连接
        conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
#endif
    }
#ifndef MSG_SEND_AUTO
    // 断开连接
    conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
#endif
    return 0;
}

int TcpConnection::processWrite(void *arg)
{
    Debug("开始发送数据了(基于写事件发送)....");
    TcpConnection *conn = static_cast<TcpConnection *>(arg); // 类静态函数不能直接访问类成员
    // 发送数据
    int count = conn->m_writeBuf->sendData(conn->m_channel->getSocket());
    if (count > 0)
    {
        // 判断数据是否被全部发送出去了
        if (conn->m_writeBuf->readableSize() == 0)
        {
            // 1. 不再检测写事件 -- 修改channel中保存的事件
            conn->m_channel->writeEventEnable(false);
            // 2. 修改了m_channel,修改dispatcher检测的集合 -- 添加任务节点
            conn->m_evLoop->addTask(conn->m_channel, ElemType::MODIFY);
            // 3. 删除这个节点
            conn->m_evLoop->addTask(conn->m_channel, ElemType::DELETE);
        }
    }
    return 0;
}

int TcpConnection::destroy(void *arg)
{
    TcpConnection *conn = static_cast<TcpConnection *>(arg); // 类静态函数不能直接访问类成员
    if (conn != nullptr)
    {
        delete conn;
    }
    return 0;
}