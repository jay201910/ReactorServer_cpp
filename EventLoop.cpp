#include "EventLoop.h"
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SelectDispatcher.h"
#include "PollDispatcher.h"
#include "EpollDispatcher.h"
#include <functional>

EventLoop::EventLoop() : EventLoop(string()) // 用了委托构造函数
{
}

EventLoop::EventLoop(const string threadName)
{
    m_isQuit = true;                                                   // 默认没有启动
    m_threadID = this_thread::get_id();                                // 获取当前线程id
    m_threadName = threadName == string() ? "MainThread" : threadName; // 主线程threadName为空
    m_dispatcher = new EpollDispatcher(this);
    // map
    m_channelMap.clear();
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_socketPair);
    if (ret == -1)
    {
        perror("socketpair");
        exit(0);
    }

    // 指定规则: evLoop->socketPair[0] 发送数据, evLoop->socketPair[1] 接收数据
#if 0
    //channel通过传入静态成员函数来构造
    Channel *channel = new Channel(m_socketPair[1], FDEvent::ReadEvent,
                                   readLocalMessage, NULL, NULL, this);
#else
    // channel通过传入function类型的参数来构造
    auto obj = bind(&EventLoop::readMessage, this);
    Channel *channel = new Channel(m_socketPair[1], FDEvent::ReadEvent,
                                   obj, NULL, NULL, this);
#endif

    // channel 添加到任务队列
    addTask(channel, ElemType::ADD);
}

EventLoop::~EventLoop()
{
}

// 启动反应堆模型
int EventLoop::run()
{
    m_isQuit = false;
    // 比较线程ID是否正常
    if (m_threadID != this_thread::get_id())
    {
        return -1;
    }
    // 循环进行事件处理
    while (!m_isQuit)
    {
        m_dispatcher->dispatch(); // 超时时长 2s
        processTaskQ();
    }
    return 0;
}

// 处理激活的文件fd
int EventLoop::eventActivate(int fd, int event)
{
    if (fd < 0)
    {
        return -1;
    }
    // 取出channel
    Channel *channel = m_channelMap[fd];
    assert(channel->getSocket() == fd);
    if (event & (int)FDEvent::ReadEvent && channel->readCallback)
    {
        channel->readCallback(const_cast<void *>(channel->getArg()));
    }
    if (event & (int)FDEvent::WriteEvent && channel->writeCallback)
    {
        channel->writeCallback(const_cast<void *>(channel->getArg()));
    }
    return 0;
}

// 添加任务到任务队列
int EventLoop::addTask(Channel *channel, ElemType type)
{
    // 加锁, 保护共享资源
    m_mutex.lock();
    // 创建新节点
    ChannelElement *node = new ChannelElement;
    node->channel = channel;
    node->type = type;
    // 添加新节点到任务队列
    m_taskQ.push(node);
    m_mutex.unlock();
    // 处理节点
    /*
     * 细节:
     *   1. 对于链表节点的添加: 可能是当前线程也可能是其他线程(主线程)
     *       1). 修改fd的事件, 当前子线程发起, 当前子线程处理
     *       2). 添加新的fd, 添加任务节点的操作是由主线程发起的
     *   2. 不能让主线程处理任务队列, 需要由当前的子线程取处理
     */
    if (m_threadID == this_thread::get_id())
    {
        // 当前子线程(基于子线程的角度分析)
        processTaskQ();
    }
    else
    {
        // 主线程 -- 告诉子线程处理任务队列中的任务
        // 1. 子线程在工作 2. 子线程被阻塞了:select, poll, epoll
        taskWakeup();
    }
    return 0;
}

int EventLoop::processTaskQ()
{

    while (!m_taskQ.empty())
    {
        // 对taskQ进行取节点的操作可能多个线程会同时进行，故这段操作要加锁
        m_mutex.lock();
        // 取出头结点
        ChannelElement *node = m_taskQ.front();
        m_taskQ.pop();
        m_mutex.unlock();
        Channel *channel = node->channel;
        if (node->type == ElemType::ADD)
        {
            // 添加
            add(channel);
        }
        else if (node->type == ElemType::DELETE)
        {
            // 删除
            remove(channel);
        }
        else if (node->type == ElemType::MODIFY)
        {
            // 修改
            modify(channel);
        }
        delete node;
    }
    return 0;
}

int EventLoop::add(Channel *channel)
{
    int fd = channel->getSocket();
    // 找到fd对应的数组元素位置, 并存储
    if (m_channelMap.find(fd) == m_channelMap.end()) // 如果map内没有该键值对 就存储
    {
        m_channelMap.insert(make_pair(fd, channel));
        // 调用dispatcher实例的add函数，在此之前通过setChannel函数设置一下对应的channel
        m_dispatcher->setChannel(channel);
        int ret = m_dispatcher->add();
        return ret;
    }
    return -1;
}

int EventLoop::remove(Channel *channel)
{
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end()) // 如果map中没有 就不用删除了
    {
        return -1;
    }
    // 调用dispatcher实例的remove函数，在此之前通过setChannel函数设置一下对应的channel
    m_dispatcher->setChannel(channel);
    int ret = m_dispatcher->remove();
    return ret;
}

int EventLoop::modify(Channel *channel)
{
    int fd = channel->getSocket();
    if (m_channelMap.find(fd) == m_channelMap.end()) // 如果map中没有 就不用修改了
    {
        return -1;
    }
    m_dispatcher->setChannel(channel);
    int ret = m_dispatcher->modify();
    return ret;
}

int EventLoop::freeChannel(Channel *channel)
{
    // 删除 channel 和 fd 的对应关系
    int fd = channel->getSocket();
    auto it = m_channelMap.find(fd); // 通过迭代器找到键值对
    if (it != m_channelMap.end())
    {
        m_channelMap.erase(it);
        close(fd); // 销毁channel之前要先关闭channel的fd
        delete channel;
    }
    return 0;
}

int EventLoop::readLocalMessage(void *arg)
{
    EventLoop *evLoop = static_cast<EventLoop *>(arg); // 类的静态成员函数不属于对象，不能直接访问当前对象，即不能使用this，需要通过arg参数把evLoop传递过来
    char buf[256];
    read(evLoop->m_socketPair[1], buf, sizeof(buf));
    return 0;
}

int EventLoop::readMessage()
{
    char buf[256];
    read(m_socketPair[1], buf, sizeof(buf));
    return 0;
}

void EventLoop::taskWakeup()
{
    const char *msg = "我是要成为海贼王的男人!!!";
    write(m_socketPair[0], msg, strlen(msg));
}
