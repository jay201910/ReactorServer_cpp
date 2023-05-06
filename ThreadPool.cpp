#include "ThreadPool.h"
#include <assert.h>
#include <stdlib.h>

ThreadPool::ThreadPool(struct EventLoop *mainLoop, int count)
{
    m_index = 0;
    m_isStart = false;
    m_mainLoop = mainLoop;
    m_threadNum = count;
    m_workerThreads.clear(); // 初始化vector
}

ThreadPool::~ThreadPool()
{
    for (auto item : m_workerThreads)
    {
        delete item;
    }
}

void ThreadPool::run()
{
    assert(!m_isStart);
    // 检查是否是主线程在运行线程池
    if (m_mainLoop->getThreadID() != this_thread::get_id())
    {
        exit(0);
    }
    m_isStart = true;
    if (m_threadNum > 0)
    {
        for (int i = 0; i < m_threadNum; ++i)
        {
            // 在线程池中创建子线程们
            WorkerThread *subThread = new WorkerThread(i);
            subThread->run();
            m_workerThreads.push_back(subThread);
        }
    }
}

EventLoop *ThreadPool::takeWorkerEventLoop()
{
    assert(m_isStart);
    // 检查是否是主线程在运行线程池
    if (m_mainLoop->getThreadID() != this_thread::get_id())
    {
        exit(0);
    }
    // 从线程池中找一个子线程, 然后取出里边的反应堆实例
    EventLoop *evLoop = m_mainLoop; // 初始化evLoop为mainLoop，如果线程池里没有子线程，那么取出的evLoop就是mainLoop
    if (m_threadNum > 0)
    {
        evLoop = m_workerThreads[m_index]->getEventLoop();
        m_index = ++m_index % m_threadNum; // 使m_index这个数字在正确的范围内
    }
    return evLoop;
}
