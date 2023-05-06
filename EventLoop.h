#pragma once
#include "Dispatcher.h"
#include <thread>
#include "Channel.h"
#include <queue>
#include <map>
#include <mutex>
#include <string>

using namespace std;

// 处理该节点中的channel的方式
// 指定类型为char，相对于默认的int类型更节省空间
enum class ElemType : char
{
    ADD,
    DELETE,
    MODIFY
};

// 定义任务队列的节点
struct ChannelElement
{
    ElemType type; // 如何处理该节点中的channel
    Channel *channel;
};

class Dispatcher;

class EventLoop
{
public:
    EventLoop();
    EventLoop(const string threadNmae);
    ~EventLoop();
    // 启动反应堆模型
    int run();
    // 处理激活的文件fd
    int eventActivate(int fd, int event);
    // 添加任务到任务队列
    int addTask(Channel *channel, ElemType type);
    // 处理任务队列中的任务
    int processTaskQ();
    // 处理dispatcher中的节点
    int add(Channel *channel);
    int remove(Channel *channel);
    int modify(Channel *channel);
    // 释放channel
    int freeChannel(Channel *channel);
    static int readLocalMessage(void *arg); // 类的静态成员函数不能直接访问该对象，所以需要将当前对象作为arg参数传入
    int readMessage();                      // 这是readLocalMessage的替换版本，用于传入function类型参数构造channel
    inline thread::id getThreadID()         // 方便从外部获取m_threadID
    {
        return m_threadID;
    }

private:
    void taskWakeup();

private:
    bool m_isQuit;
    Dispatcher *m_dispatcher; // 该指针指向子类dispatcher实例
    // 任务队列
    queue<ChannelElement *> m_taskQ;
    // map
    map<int, Channel *> m_channelMap;
    // 线程id, name, mutex
    thread::id m_threadID;
    string m_threadName;
    mutex m_mutex;
    int m_socketPair[2]; // 存储本地通信的fd 通过socketpair 初始化
};
