#pragma once
#include "Channel.h"
#include "Dispatcher.h"
#include "EventLoop.h"
#include <string>
#include <poll.h>

using namespace std;

class PollDispatcher : public Dispatcher
{
public:
    PollDispatcher(EventLoop *evLoop);
    ~PollDispatcher();
    // 添加
    int add() override;
    // 删除
    int remove() override;
    // 修改
    int modify() override;
    // 事件监测
    int dispatch(int timeout) override; // 单位: s

private:
    // poll的数据资源
    int m_maxfd;
    struct pollfd *m_fds;
    const int m_maxNode = 1024;
};