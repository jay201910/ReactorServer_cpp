#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include "EventLoop.h"

using namespace std;

// 定义子线程对应的结构体
class WorkerThread
{
public:
    WorkerThread(int index);
    ~WorkerThread();
    // 启动线程
    void run();
    // 取出evLoop
    inline EventLoop *getEventLoop() // 函数简单 写成内联函数
    {
        return m_evLoop;
    }

private:
    void running(); // 线程的任务函数

private:
    thread *m_thread;      // 线程实例
    thread::id m_threadID; // ID
    string m_name;
    mutex m_mutex;             // 互斥锁
    condition_variable m_cond; // 条件变量
    EventLoop *m_evLoop;       // 反应堆模型
};
