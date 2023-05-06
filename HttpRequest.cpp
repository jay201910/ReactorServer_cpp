// #define _GNU_SOURCE
#include "HttpRequest.h"
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "TcpConnection.h"
#include <assert.h>
#include <ctype.h>

HttpRequest::HttpRequest()
{
    reset();
}

HttpRequest::~HttpRequest()
{
}

// 重置
void HttpRequest::reset()
{
    m_curState = ProcessState::ParseReqLine;
    m_method = m_url = m_version = string();
    m_reqHeaders.clear(); // 初始化map
}

// 添加请求头
void HttpRequest::addHeader(const string key, const string value)
{
    if (key.empty() || value.empty())
        return;
    m_reqHeaders.insert(make_pair(key, value));
}

// 根据key得到请求头的value
string HttpRequest::getHeader(const string key)
{
    auto it = m_reqHeaders.find(key);
    if (it == m_reqHeaders.end())
        return string(); // 没查找到
    return it->second;
}

// 解析请求行 得到method，url，version
bool HttpRequest::parseRequestLine(Buffer *readBuf)
{
    // 读出请求行, 保存字符串结束地址
    char *end = readBuf->findCRLF();
    // 保存字符串起始地址
    char *start = readBuf->getReadPos();
    // 请求行总长度
    int lineSize = end - start;

    if (lineSize > 0)
    {
        auto setMethodFunc = bind(&HttpRequest::setMethod, this, placeholders::_1); // placeholders::_1参数占位，因为setMethod有一个参数
        start = splitRequestLine(start, end, " ", setMethodFunc);
        auto setUrlFunc = bind(&HttpRequest::setUrl, this, placeholders::_1);
        start = splitRequestLine(start, end, " ", setUrlFunc);
        auto setVersionFunc = bind(&HttpRequest::setVersion, this, placeholders::_1);
        splitRequestLine(start, end, nullptr, setVersionFunc);

        // 为解析请求头做准备
        readBuf->readPosIncrease(lineSize + 2);
        // 修改状态
        setState(ProcessState::ParseReqHeaders); // 类内可以直接对m_curState进行修改
        return true;
    }
    return false;
}
// 解析请求头 得到键值对
bool HttpRequest::parseRequestHeader(Buffer *readBuf)
{
    char *end = readBuf->findCRLF();
    if (end != nullptr)
    {
        char *start = readBuf->getReadPos();
        int lineSize = end - start;
        // 基于: 搜索字符串
        char *middle = static_cast<char *>(memmem(start, lineSize, ": ", 2)); // 查找冒号的位置
        // 根据start，end，冒号位置middle 分割出键值对
        if (middle != nullptr)
        {
            int keyLen = middle - start;
            int valueLen = end - middle - 2;
            if (keyLen > 0 && valueLen > 0)
            {
                string key(start, keyLen); // 构造string类key
                string value(middle + 2, valueLen);
                addHeader(key, value);
            }
            // 移动读数据的位置
            readBuf->readPosIncrease(lineSize + 2);
        }
        else
        {
            // 请求头被解析完了, 跳过空行
            readBuf->readPosIncrease(2);
            // 修改解析状态
            setState(ProcessState::ParseReqDone); // 类内可以直接对m_curState进行修改
            // 忽略 post 请求, 按照 get 请求处理
        }
        return true;
    }
    return false;
}
// 解析http请求协议
bool HttpRequest::parseHttpRequest(Buffer *readBuf, HttpResponse *response, Buffer *sendBuf, int socket)
{
    bool flag = true;
    while (m_curState != ProcessState::ParseReqDone)
    {
        switch (m_curState)
        {
        case ProcessState::ParseReqLine:
            flag = parseRequestLine(readBuf);
            break;
        case ProcessState::ParseReqHeaders:
            flag = parseRequestHeader(readBuf);
            break;
        case ProcessState::ParseReqBody:
            break;
        default:
            break;
        }
        if (!flag)
        {
            return flag;
        }
        // 判断是否解析完毕了, 如果完毕了, 需要准备回复的数据
        if (m_curState == ProcessState::ParseReqDone)
        {
            // 1. 根据解析出的原始数据, 对客户端的请求做出处理
            processHttpRequest(response);
            // 2. 组织响应数据并发送给客户端
            response->prepareMsg(sendBuf, socket);
        }
    }
    m_curState = ProcessState::ParseReqLine; // 状态还原, 保证还能继续处理第二条及以后的请求
    return flag;
}

// 处理http请求协议
bool HttpRequest::processHttpRequest(HttpResponse *response)
{
    if (strcasecmp(m_method.data(), "get") != 0) // string类的c_str()或data()方法生成一个const char*指针 指向一个临时数组。
    {
        return -1;
    }
    m_url = decodeMsg(m_url); // 浏览器会将url编码后发送给服务器 服务器端需要解码
    // 处理客户端请求的静态资源(目录或者文件)
    const char *file = NULL;
    if (strcmp(m_url.data(), "/") == 0) // 是根目录
    {
        file = "./";
    }
    else
    {
        file = m_url.data() + 1;
    }
    // 获取文件属性
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1)
    {
        // 文件不存在 -- 回复404
        // sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
        // sendFile("404.html", cfd);
        response->setFileName("404.html");
        response->setStatusCode(StatusCode::NotFound);
        // 响应头
        response->addHeader("Content-type", getFileType(".html"));
        response->sendDataFunc = sendFile; // function不能直接打包类非静态成员函数 需要先用bind绑定函数 或者把函数改为类静态函数
        return 0;
    }

    response->setFileName(file);
    response->setStatusCode(StatusCode::OK);
    // 判断文件类型
    if (S_ISDIR(st.st_mode))
    {
        // 把这个目录中的内容发送给客户端
        // sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
        // sendDir(file, cfd);
        // 响应头
        response->addHeader("Content-type", getFileType(".html"));
        response->sendDataFunc = sendDir;
    }
    else
    {
        // 把文件的内容发送给客户端
        // sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
        // sendFile(file, cfd);
        // 响应头
        char tmp[12] = {0};
        sprintf(tmp, "%ld", st.st_size);
        response->addHeader("Content-type", getFileType(file));
        response->addHeader("Content-length", to_string(st.st_size));
        response->sendDataFunc = sendFile;
    }

    return false;
}

// 解码字符串
string HttpRequest::decodeMsg(string msg)
{
    string str = string();
    const char *from = msg.data();
    for (; *from != '\0'; ++from)
    {
        // isxdigit -> 判断字符是不是16进制格式, 取值在 0-f
        // Linux%E5%86%85%E6%A0%B8.jpg
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) // 形如 %E5 这样的
        {
            // 将16进制的数 -> 十进制 将这个数值赋值给了字符 int -> char
            // B2 == 178
            // 将3个字符, 变成了一个字符, 这个字符就是原始数据
            str.append(1, hexToDec(from[1]) * 16 + hexToDec(from[2]));
            // 跳过 from[1] 和 from[2] 找下一个 %
            from += 2;
        }
        else
        {
            // 直接拷贝 不用转换
            str.append(1, *from);
        }
    }
    str.append(1, '\0');
    return str;
}

const string HttpRequest::getFileType(const string name)
{
    // a.jpg a.mp4 a.html
    // 自右向左查找‘.’字符, 如不存在返回NULL
    const char *dot = strrchr(name.data(), '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8"; // 纯文本
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

void HttpRequest::sendDir(const string dirName, Buffer *sendBuf, int cfd)
{
    char buf[4096] = {0};
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName.data());
    struct dirent **namelist;
    int num = scandir(dirName.data(), &namelist, NULL, alphasort);
    for (int i = 0; i < num; ++i)
    {
        // 取出文件名 namelist 指向的是一个指针数组 struct dirent* tmp[]
        char *name = namelist[i]->d_name;
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath, "%s/%s", dirName.data(), name);
        stat(subPath, &st);
        if (S_ISDIR(st.st_mode))
        {
            // a标签 <a href="">name</a>
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
                    name, name, st.st_size);
        }
        else
        {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    name, name, st.st_size);
        }
        // send(cfd, buf, strlen(buf), 0);
        sendBuf->appendString(buf);
#ifndef MSG_SEND_AUTO
        sendBuf->sendData(cfd);
#endif
        memset(buf, 0, sizeof(buf));
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    // send(cfd, buf, strlen(buf), 0);
    sendBuf->appendString(buf);
#ifndef MSG_SEND_AUTO
    sendBuf->sendData(cfd);
#endif
    free(namelist);
}

void HttpRequest::sendFile(const string fileName, Buffer *sendBuf, int cfd)
{
    // 1. 打开文件
    int fd = open(fileName.data(), O_RDONLY);
    assert(fd > 0);
#if 1
    while (1)
    {
        char buf[1024];
        int len = read(fd, buf, sizeof buf);
        if (len > 0)
        {
            // send(cfd, buf, len, 0);
            sendBuf->appendString(buf, len);
#ifndef MSG_SEND_AUTO
            sendBuf->sendData(cfd);
#endif
        }
        else if (len == 0)
        {
            break;
        }
        else
        {
            close(fd);
            perror("read");
        }
    }
#else
    off_t offset = 0;
    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    while (offset < size)
    {
        int ret = sendfile(cfd, fd, &offset, size - offset);
        printf("ret value: %d\n", ret);
        if (ret == -1 && errno == EAGAIN)
        {
            printf("没数据...\n");
        }
    }
#endif
    close(fd);
}

char *HttpRequest::splitRequestLine(const char *start, const char *end, const char *sub, function<void(string)> callback)
{
    char *space = const_cast<char *>(end); // 空格
    if (sub != nullptr)
    {
        space = static_cast<char *>(memmem(start, end - start, sub, strlen(sub)));
        assert(space != nullptr);
    }
    int length = space - start;
    // 设置method或url或version
    callback(string(start, length)); // callback的返回值为void，参数为string
    return space + 1;
}

// 将字符转换为整形数
int HttpRequest::hexToDec(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}
