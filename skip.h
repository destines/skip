#ifndef __SKIP_H_
#define __SKIP_H_
#include <stdio.h>
#include <cstring>
#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <unistd.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <string>
#include <map>

namespace skip
{
    // using namespace std;
    typedef const char c_char;
    typedef unsigned short port_t;
    typedef std::string str;
    typedef std::map<str, str> maps;

    struct Data
    {
        char *ptr;
        long long size;
    };

    class Skip
    {
    private:
        skip::str m_host;
        skip::str m_path;
        bool m_https;
        bool m_link;
        SOCKET m_skfd;
        addrinfo *m_info;
        skip::Data m_data;
        long long m_down_msg[3];

    private:
        bool parseUrl(skip::c_char *url);
        int checkReplyCode(c_char *reply);
        long long parseReplyDownloadsMsg(skip::c_char *reply);
        void hideCursor();
        void showCursor();

    public:
        Skip();
        // Skip(const char *url, const char *save_path);
        // Skip(const char *url, const char *save_path, const char * save_name);
        bool tryConnect(skip::c_char *url, skip::port_t port = 80);

        // GET：从服务器获取资源，只能请求数据，不会对服务器上的资源产生任何影响。
        skip::Data *get(skip::c_char *url, skip::maps *headers = NULL, skip::maps *params = NULL);
        // POST：向服务器提交数据，用于向服务器发送数据，可以用来创建新资源或提交数据给服务器处理。
        // void post();
        // HEAD：与 GET 方法类似，但是服务器在响应中只返回首部，不返回实体的主体部分，通常用于获取资源的元数据信息。
        skip::Data *head(skip::c_char *url, skip::maps *headers, skip::maps *params);
        // TRACE：回显服务器收到的请求，主要用于诊断。
        // void trace();
        // OPTIONS：用于获取目标资源所支持的通信选项。
        skip::Data *options(skip::c_char *url, skip::maps *headers, skip::maps *params);

        ~Skip();
    };
}

// namespace skip
// {

// } // namespace skip

#endif