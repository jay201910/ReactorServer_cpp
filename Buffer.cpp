//#define _GNU_SOURCE
#include "Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <sys/socket.h>

Buffer::Buffer(int size) : m_capacity(size)
{
    m_data = (char *)malloc(size);
    bzero(m_data, size);
    // 关于这里为什么用malloc不用new，因为m_data指向的内存可能会扩容，所以用malloc、realooc、free这一套函数
}

Buffer::~Buffer()
{
    if (m_data != nullptr)
    {
        free(m_data);
    }
}

// 扩容
void Buffer::extendRoom(int size)
{
    // 1. 内存够用 - 不需要扩容
    if (writeableSize() >= size)
    {
        return;
    }
    // 2. 内存需要合并才够用 - 不需要扩容
    // 剩余的可写的内存 + 已读的内存 > size
    else if (m_readPos + writeableSize() >= size)
    {
        // 得到未读的内存大小
        int readable = readableSize();
        // 移动内存
        memcpy(m_data, m_data + m_readPos, readable);
        // 更新位置
        m_readPos = 0;
        m_writePos = readable;
    }
    // 3. 内存不够用 - 扩容
    else
    {
        void *temp = realloc(m_data, m_capacity + size);
        if (temp == nullptr)
        {
            return; // 失败了
        }
        memset((char *)temp + m_capacity, 0, size);
        // 更新数据
        m_data = static_cast<char *>(temp);
        m_capacity += size;
    }
}

// 得到剩余的可写的内存容量
int Buffer::writeableSize()
{
    return m_capacity - m_writePos;
}

// 得到剩余的可读的内存容量
int Buffer::readableSize()
{
    return m_writePos - m_readPos;
}

// 写内存 1. 直接写 2. 接收套接字数据
int Buffer::appendString(const char *data, int size)
{
    if (data == nullptr || size <= 0)
    {
        return -1;
    }
    // 扩容
    extendRoom(size);
    // 数据拷贝
    memcpy(m_data + m_writePos, data, size);
    m_writePos += size;
    return 0;
}
int Buffer::appendString(const char *data)
{
    int size = strlen(data);
    int ret = appendString(data, size);
    return ret;
}
int Buffer::appendString(const string data)
{
    int ret = appendString(data.data());
    return ret;
}

int Buffer::socketRead(int fd)
{
    // read/recv/readv
    struct iovec vec[2];
    // 初始化数组元素
    int writeable = writeableSize();
    vec[0].iov_base = m_data + m_writePos;
    vec[0].iov_len = writeable;
    char *tmpbuf = (char *)malloc(40960);
    vec[1].iov_base = m_data + m_writePos;
    vec[1].iov_len = 40960;
    int result = readv(fd, vec, 2);
    if (result == -1)
    {
        return -1;
    }
    else if (result <= writeable)
    {
        m_writePos += result;
    }
    else
    {
        m_writePos = m_capacity;
        appendString(tmpbuf, result - writeable);
    }
    free(tmpbuf);
    return result;
}

// 根据\r\n取出一行, 找到其在数据块中的位置, 返回该位置
char *Buffer::findCRLF()
{
    // strstr --> 大字符串中匹配子字符串(遇到\0结束) char *strstr(const char *haystack, const char *needle);
    // memmem --> 大数据块中匹配子数据块(需要指定数据块大小)
    // void *memmem(const void *haystack, size_t haystacklen,
    //      const void* needle, size_t needlelen);
    char *ptr = (char *)memmem(m_data + m_readPos, readableSize(), "\r\n", 2);
    return ptr;
}

// 发送数据
int Buffer::sendData(int socket)
{
    // 判断有无数据
    int readable = readableSize();
    if (readable > 0)
    {
        int count = send(socket, m_data + m_readPos, readable, MSG_NOSIGNAL);
        if (count > 0)
        {
            m_readPos += count;
            usleep(1);
        }
        return count;
    }
    return 0;
}
