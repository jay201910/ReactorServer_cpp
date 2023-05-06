#pragma once
#include "Buffer.h"
#include <stdbool.h>
#include "HttpResponse.h"
#include <map>
#include <functional>

using namespace std;

// 当前的解析状态
enum class ProcessState : char // 强类型枚举 指定数据类型为char 节省内存
{
    ParseReqLine,
    ParseReqHeaders,
    ParseReqBody,
    ParseReqDone
};

// 定义http请求结构体
class HttpRequest
{
public:
    HttpRequest();
    ~HttpRequest();
    // 重置
    void reset();

    // 添加请求头
    void addHeader(const string key, const string value);
    // 根据key得到请求头的value
    string getHeader(const string key);
    // 解析请求行
    bool parseRequestLine(Buffer *readBuf);
    // 解析请求头
    bool parseRequestHeader(Buffer *readBuf);
    // 解析http请求协议
    bool parseHttpRequest(Buffer *readBuf, HttpResponse *response, Buffer *sendBuf, int socket);
    // 处理http请求协议
    bool processHttpRequest(HttpResponse *response);
    // 解码字符串
    string decodeMsg(string msg); // 浏览器会将url编码后发送给服务器 服务器端需要解码
    const string getFileType(const string name);
    static void sendDir(const string dirName, Buffer *sendBuf, int cfd);
    static void sendFile(const string fileName, Buffer *sendBuf, int cfd); //写成静态函数 方便给HttpResponse->senDataFunc

    inline void setMethod(string method) { m_method = method; }
    inline void setUrl(string url) { m_url = url; }
    inline void setVersion(string version) { m_version = version; }
    // 获取处理状态
    inline ProcessState getState() { return m_curState; }
    // 修改处理状态
    inline void setState(ProcessState state) { m_curState = state; }

private:
    // 从请求行中分割出method、url、version，并赋给m_method、m_url、m_version
    // 是设置method还是url还是version由传入的回调函数指定，这些回调函数用function打包
    char *splitRequestLine(const char *start, const char *end, const char *sub, function<void(string)> callback);
    int hexToDec(char c);

private:
    string m_method;
    string m_url;
    string m_version;
    map<string, string> m_reqHeaders;
    ProcessState m_curState;
};
