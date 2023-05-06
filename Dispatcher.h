#pragma once
#include "Channel.h"
#include <string>
#include "EventLoop.h"

using namespace std;

class Dispatcher
{
public:
    Dispatcher(EventLoop *evLoop);
    virtual ~Dispatcher(); // 虚析构函数 防止父类指针指向子类对象 对象析构时出错
    // 添加
    virtual int add();
    // 删除
    virtual int remove();
    // 修改
    virtual int modify();
    // 事件监测
    virtual int dispatch(int timeout = 2); // 单位: s
    // 设置channel
    inline void setChannel(Channel *channel) //由于dispatcher要处理channel是一直改变的，所以要提供setChannel方法
    {
        m_channel = channel;
    }

protected:
    // protected 子类可以继承
    Channel *m_channel;
    EventLoop *m_evLoop;
    string m_name = string(); // 实例的名字
};