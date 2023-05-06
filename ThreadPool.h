#pragma once
#include "EventLoop.h"
#include <vector>
#include "WorkerThread.h"

using namespace std;

// 定义线程池类
class ThreadPool
{
public:
    ThreadPool(struct EventLoop *mainLoop, int count); // count是子线程个数
    ~ThreadPool();
    void run();                       // 启动线程池
    EventLoop *takeWorkerEventLoop(); // 取出线程池中的某个子线程的反应堆实例

private:
    EventLoop *m_mainLoop; // 线程池所属的主线程的反应堆模型
    bool m_isStart;
    int m_threadNum;
    vector<WorkerThread *> m_workerThreads; // 线程池里的子线程们
    int m_index;                            // 线程池里取出的子线程的index
};
