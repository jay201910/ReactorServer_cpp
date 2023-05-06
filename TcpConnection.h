#pragma once
#include "EventLoop.h"
#include "Buffer.h"
#include "Channel.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

// #define MSG_SEND_AUTO

class TcpConnection
{
public:
    TcpConnection(int fd, EventLoop *evLoop); // 用于通信的fd，所属的eventLoop
    ~TcpConnection();
    static int processRead(void *arg);
    static int processWrite(void *arg);
    static int destroy(void *arg);

private:
    string m_name;
    EventLoop *m_evLoop;
    Channel *m_channel;
    Buffer *m_readBuf;
    Buffer *m_writeBuf;
    HttpRequest *m_request;
    HttpResponse *m_response;
};
