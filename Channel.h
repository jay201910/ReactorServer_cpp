#pragma once
#include <functional>
using namespace std;

// 定义函数指针
// typedef int (*handleFunc)(void *arg);
// using handleFunc = int (*)(void *); // 用using关键字定义函数指针

// 定义文件描述符的读写事件
// 用强类型枚举
enum class FDEvent
{
    TimeOut = 0x01,
    ReadEvent = 0x02,
    WriteEvent = 0x04
};

class Channel
{
public:
    // 构造函数
    using handleFunc = function<int(void *)>;
    // channel()的参数：fd，要检测fd的读时间还是写时间，读事件处理函数，写事件处理函数，销毁这个channel，给回调函数使用的参数
    Channel(int fd, FDEvent events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void *arg);

    // 回调函数
    handleFunc readCallback;
    handleFunc writeCallback;
    handleFunc destroyCallback;

    // 修改fd的写事件(检测 or 不检测)
    void writeEventEnable(bool flag);
    // 判断是否需要检测文件描述符的写事件
    bool isWriteEventEnable();

    // 取出私有成员的值
    inline int getEvent()
    {
        return m_events;
    }
    inline int getSocket()
    {
        return m_fd;
    }
    inline const void *getArg()   //加const是因为 返回了指针，外部还是能修改内部成员。注意外部取到这个const 指针后用的时候要去掉这个指针的const属性
    {
        return m_arg;
    }

private:
    // 文件描述符
    int m_fd;
    // 事件
    int m_events;
    // 回调函数的参数
    void *m_arg;
};
