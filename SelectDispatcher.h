#pragma once
#include "Channel.h"
#include "Dispatcher.h"
#include "EventLoop.h"
#include <string>
#include <sys/select.h>

using namespace std;

class SelectDispatcher : public Dispatcher
{
public:
    SelectDispatcher(EventLoop *evLoop);
    ~SelectDispatcher();
    // 添加
    int add() override;
    // 删除
    int remove() override;
    // 修改
    int modify() override;
    // 事件监测
    int dispatch(int timeout) override; // 单位: s
private:
    // 给类内成员函数服务的函数
    void setFdSet();
    void clearFdSet();

private:
    // select的数据资源
    fd_set m_readSet;
    fd_set m_writeSet;
    const int m_maxSize = 1024;
};