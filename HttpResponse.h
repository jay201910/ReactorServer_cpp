#pragma once
#include "Buffer.h"
#include <map>
#include <functional>

using namespace std;

// 定义状态码枚举
enum class StatusCode
{
    Unknown,
    OK = 200,
    MovedPermanently = 301,
    MovedTemporarily = 302,
    BadRequest = 400,
    NotFound = 404
};

// 定义结构体
class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();
    // 添加响应头
    void addHeader(const string key, const string value);
    // 组织http响应数据
    void prepareMsg(Buffer *sendBuf, int socket);
    // 打包一个函数指针, 用来组织要回复给客户端的数据块
    function<void(const string, struct Buffer *, int)> sendDataFunc; // 这是类成员变量
    inline void setFileName(string name) { m_fileName = name; }
    inline void setStatusCode(StatusCode code) { m_statusCode = code; }

private:
    StatusCode m_statusCode; // 状态码
    string m_fileName;
    map<string, string> m_headers; // 响应头 - 键值对
    // 状态码：状态描述 键值对
    const map<int, string> m_info{
        {200, "OK"}, {301, "MovedPermanently"}, {302, "MovedTemporarily"}, {400, "BadRequest"}, {404, "NotFound"}};
};
