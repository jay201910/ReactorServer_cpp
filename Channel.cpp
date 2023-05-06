#include "Channel.h"
#include <stdlib.h>

Channel::Channel(int fd, FDEvent events, handleFunc readFunc, handleFunc writeFunc, handleFunc destroyFunc, void *arg)
{
    m_arg = arg;
    m_fd = fd;
    m_events = (int)events;
    readCallback = readFunc;
    writeCallback = writeFunc;
    destroyCallback = destroyFunc;
}

void Channel::writeEventEnable(bool flag)
{
    if (flag)
    {
        m_events |= static_cast<int>(FDEvent::WriteEvent); // 强枚举类型FDEvent中的成员WriteEvent类型是FDEvent类型 需要强转成int类型
    }
    else
    {
        m_events = m_events & ~static_cast<int>(FDEvent::WriteEvent);
    }
}

bool Channel::isWriteEventEnable()
{
    return m_events & static_cast<int>(FDEvent::WriteEvent);
}