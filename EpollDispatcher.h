#pragma once
#include "Channel.h"
#include "Dispatcher.h"
#include "EventLoop.h"
#include <string>
#include <sys/epoll.h>

using namespace std;

class EpollDispatcher : public Dispatcher
{
public:
    EpollDispatcher(EventLoop *evLoop);
    ~EpollDispatcher();
    // 添加
    int add() override;
    // 删除
    int remove() override;
    // 修改
    int modify() override;
    // 事件监测
    int dispatch(int timeout) override; // 单位: s
private:
    // epollCtl函数是给add、remove、modify函数代码复用的
    int epollCtl(int op);

private:
    // epoll的数据资源
    int m_epfd;
    struct epoll_event *m_events;
    const int m_maxNode = 520; // 整型常量 对应c代码中的宏Max
};