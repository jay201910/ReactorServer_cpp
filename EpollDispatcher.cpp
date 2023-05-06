#include "EpollDispatcher.h"
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

EpollDispatcher::EpollDispatcher(EventLoop *evLoop) : Dispatcher(evLoop)  //调用了父类的构造函数，因为evLoop是在Dispatcher里构造的
{
    
    m_epfd = epoll_create(10);
    if (m_epfd == -1)
    {
        perror("epoll_create");
        exit(0);
    }
    m_events = new struct epoll_event[m_maxNode];
    m_name = "Epoll";
}

EpollDispatcher::~EpollDispatcher()
{
    // 析构函数的处理：关闭epfd，销毁new出来的m_events
    close(m_epfd);
    delete[] m_events;
}

int EpollDispatcher::epollCtl(int op)
{
    struct epoll_event ev;
    ev.data.fd = m_channel->getSocket();
    int events = 0;
    if (m_channel->getEvent() & (int)FDEvent::ReadEvent)
    {
        events |= EPOLLIN;
    }
    if (m_channel->getEvent() & (int)FDEvent::WriteEvent)
    {
        events |= EPOLLOUT;
    }
    ev.events = events;
    int ret = epoll_ctl(m_epfd, op, m_channel->getSocket(), &ev);
    return ret;
}
// 添加
int EpollDispatcher::add()
{
    int ret = epollCtl(EPOLL_CTL_ADD);
    if (ret == -1)
    {
        perror("epoll_ctl add");
        exit(0);
    }
    return ret;
}

// 删除
int EpollDispatcher::remove()
{
    int ret = epollCtl(EPOLL_CTL_DEL);
    if (ret == -1)
    {
        perror("epoll_ctl delete");
        exit(0);
    }
    // 通过 channel 释放对应的 TcpConnection 资源
    m_channel->destroyCallback(const_cast<void *>(m_channel->getArg())); // 因为m_channel->getArg()返回值是const*类型的 所以用const_cast去掉const属性

    return ret;
}

// 修改
int EpollDispatcher::modify()
{
    int ret = epollCtl(EPOLL_CTL_MOD);
    if (ret == -1)
    {
        perror("epoll_ctl modify");
        exit(0);
    }
    return ret;
}

// 事件监测
int EpollDispatcher::dispatch(int timeout = 2)
{
    int count = epoll_wait(m_epfd, m_events, m_maxNode, timeout * 1000);
    for (int i = 0; i < count; ++i)
    {
        int events = m_events[i].events;
        int fd = m_events[i].data.fd;
        if (events & EPOLLERR || events & EPOLLHUP)
        {
            // 对方断开了连接, 删除 fd
            // epollRemove(Channel, evLoop);
            continue;
        }
        if (events & EPOLLIN)
        {
            m_evLoop->eventActivate(fd, (int)FDEvent::ReadEvent);
        }
        if (events & EPOLLOUT)
        {
            m_evLoop->eventActivate(fd, (int)FDEvent::WriteEvent);
        }
    }
    return 0;
}
