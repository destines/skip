#include "skip.h"

namespace skip
{
    Skip::Skip()
        : m_host(""), m_path(""), m_https(false), m_link(false),
          m_skfd(0), m_info(NULL), m_data({NULL, 0})
    {
        m_down_msg[0] = 0;
        m_down_msg[1] = 0;
        m_down_msg[2] = 0;

        // url parse
        struct WSAData wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa))
        {
            std::cerr << "[ERROR]:in windows: WSAStartup faild" << std::endl;
            return;
        }

        m_skfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (INVALID_SOCKET == m_skfd)
        {
            std::cerr << "[ERROR]:create socket faild" << std::endl;
            WSACleanup();
            return;
        }

        int enable = 1;
        if (SOCKET_ERROR == setsockopt(m_skfd, SOL_SOCKET, SO_REUSEADDR,
                                       (const char *)&enable, sizeof(enable)))
        {
            std::cerr << "[ERROR]:set socket option error" << std::endl;
            return;
        }

        return;
    }

    void Skip::hideCursor()
    {
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(consoleHandle, &cursorInfo);
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(consoleHandle, &cursorInfo);
    }

    void Skip::showCursor()
    {
        HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(consoleHandle, &cursorInfo);
        cursorInfo.bVisible = TRUE;
        SetConsoleCursorInfo(consoleHandle, &cursorInfo);
    }

    bool Skip::parseUrl(skip::c_char *url)
    {
        if (url == NULL)
        {
            std::cerr << "[ERROR]:url is void" << std::endl;
            return false;
        }

        const char *path = url;
        const char *host = url;

        if (NULL != strstr(url, "http://"))
        {
            host += strlen("http://");
            m_https = false;
        }
        else if (NULL != strstr(url, "https://"))
        {
            host += strlen("https://");
            m_https = true;
        }
        // "https://cdn.kernel.org"
        if (NULL == (path = strstr(host, "/")))
        {
            m_host.append(host);
            m_path.resize(0);
        }
        else
        {
            path += 1;
            size_t path_len = strlen(path);
            if (path_len == 0) // "https://cdn.kernel.org/"
            {
                m_host.append(host, strlen(host) - 1);
                m_path.resize(0);
            }
            else // "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.5.3.tar.xz"
            {
                size_t host_len = strlen(host) - path_len - 1;
                m_host.append(host, host_len);
                m_path.append(path, path_len);
            }
        }
        return true;
    }

    bool Skip::tryConnect(skip::c_char *url, skip::port_t port)
    {
        if (parseUrl(url) == false)
            return false;
        unsigned long mode = 1; // 1 表示非阻塞模式

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        int status = getaddrinfo(m_host.c_str(), NULL, &hints, &m_info);
        if (0 != status)
        {
            std::cerr << "[ERROR]:get addr info error: " << gai_strerror(status);
            return false;
        }

        if (0 != ioctlsocket(m_skfd, FIONBIO, &mode))
        { // 设置失败，处理错误情况
            std::cerr << "[ERROR]:ioctlsocket set socket FIONBIO error" << std::endl;
            return false;
        }

        struct sockaddr_in seraddrin;
        memset(&seraddrin, 0, sizeof(seraddrin));
        memcpy(&seraddrin, (struct sockaddr_in *)m_info->ai_addr, sizeof(seraddrin));
        seraddrin.sin_family = AF_INET;
        seraddrin.sin_port = htons(port);
        /* // 将 IP 地址转换为网络字节序并赋值给 sockaddr_in 结构体
        addr_list = (struct in_addr **)host->h_addr_list;
        sa.sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[0]));
        */
        // sockaddrin.sin_addr.s_addr =
        //     inet_addr(inet_ntoa(**(struct in_addr **)(hostEntry->h_addr_list)));

        if (SOCKET_ERROR == connect(m_skfd, (struct sockaddr *)&seraddrin,
                                    sizeof(sockaddr_in)))
        {
            if (WSAEWOULDBLOCK != WSAGetLastError())
            { // connect faild
                std::cerr << "[ERROR]:connect host faild" << std::endl;
                return false;
            }
            std::cerr << "[INFO]:connecting ..." << std::endl;
        }

        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(m_skfd, &wset);
        struct timeval tv = {5, 0};
        // select 第一个参数是待监视的文件描述符中最大的那个文件描述符值加1，而不是直接写死为0。
        // 如果确实不需要监视任何读取操作，可以将第一个参数设置为0 只会监视写入操作
        int sel = select(0, NULL, &wset, NULL, &tv);
        if (SOCKET_ERROR == sel)
        {
            std::cerr << "[ERROR]:error in select function." << std::endl;
            return false;
        }
        if (0 == sel)
        {
            std::cerr << "[ERROR]:timerout" << std::endl;
            return false;
        }

        std::cerr << "[INFO]:in time connect success" << std::endl;

        mode = !mode; // 1 表示非阻塞模式
        if (0 != ioctlsocket(m_skfd, FIONBIO, &mode))
        { // 设置失败，处理错误情况
            std::cerr << "[ERROR]:ioctlsocket set socket FIONBIO error" << std::endl;
            return false;
        }

        return m_link = true;
    }

    int Skip::checkReplyCode(c_char *reply)
    {
        /*  HTTP/1.1 206 Partial Content\r
            Server: nginx\r
            Content-Type: application/x-xz\r
            ...
            找到第一个\r, 获得状态码
        */
        char scode[128 + 1];
        memset(scode, 0, sizeof(scode));

        memcpy(scode, reply, (strstr(reply, "\r") - reply));

        const char *code;
        // 状态码206表示支持断点续传
        if (NULL != (code = strstr(scode, "10")))
            return atoi(code);

        if (NULL != (code = strstr(scode, "20")))
            return atoi(code);

        if (NULL != (code = strstr(scode, "30")))
            return atoi(code);

        if (NULL != (code = strstr(scode, "40")))
            return atoi(code);

        if (NULL != (code = strstr(scode, "41")))
            return atoi(code);

        if (NULL != (code = strstr(scode, "50")))
            return atoi(code);

        // if (strstr(reply, "30"))
        //     std::cerr <<"[INFO]:redirect faild"

        // if (strstr(reply, "401"))
        //     std::cerr <<"[INFO]:Unauthorized"

        // if (strstr(reply, "403"))
        //     std::cerr <<"[INFO]:access prohibited"

        // if (strstr(reply, "404"))
        //     std::cerr <<"Unable to find file"

        // if (strstr(reply, "405"))
        //     std::cerr <<"Resource is prohibited"

        // if (strstr(reply, "407"))
        //     std::cerr <<"Proxy authentication required"

        // if (strstr(reply, "410"))
        //     std::cerr <<"Never available"

        // if (strstr(reply, "414"))
        //     std::cerr <<"Request URI too long"

        // if (strstr(reply, "50"))
        //     std::cerr <<"peer server error"

        return 0;
    }

    long long Skip::parseReplyDownloadsMsg(skip::c_char *reply)
    {
        /*
        "HTTP/1.1 206 Partial Content\r\n
        Content-Range: bytes 500-999/10240\r\n
        Content-Length: 500\r\n
        \r\n"
        */
        char *tmp = NULL;
        if (NULL != (tmp = strstr(reply, "Content-Range:")))
        {
            tmp = strstr(tmp, "bytes");
            tmp += strlen("bytes");
            m_down_msg[0] = atoll(tmp);
            tmp = strstr(tmp, "-") + 1;
            m_down_msg[1] = atoll(tmp);
            tmp = strstr(tmp, "/") + 1;
            m_down_msg[2] = atoll(tmp);
        }
        else if (NULL != (tmp = strstr(reply, "Content-Length:")))
        {
            tmp += strlen("Content-Length:");
            m_down_msg[2] = atoll(tmp);
        }
        return m_down_msg[2];
    }

    skip::Data *Skip::get(skip::c_char *url, skip::maps *headers, skip::maps *params)
    {
        if (m_link == false)
        {
            if (tryConnect(url, 80) == false)
            {
                std::cerr << "[WARN]:try connect service host port [80 ] faild" << std::endl;
                if (tryConnect(url, 443) == false)
                {
                    std::cerr << "[WARN]:try connect service host port [443] faild" << std::endl;
                    std::cerr << "[WARN]:auto connect serivce faild; please use "
                                 "try_connect function and point port"
                              << std::endl;
                    return NULL;
                }
            }
        }
        skip::str msg("GET /");
        if (0 != m_path.length())
            msg.append(m_path);
        msg.append(" HTTP/1.1\r\nhost:");
        msg.append(m_host);
        msg.append("\r\n");
        if (headers)
            for (skip::maps::iterator it = headers->begin(); it != headers->end(); ++it)
            {
                msg.append(it->first);
                msg.append(":");
                msg.append(it->second);
                msg.append("\r\n");
            }
        if (params)
            for (skip::maps::iterator it = params->begin(); it != params->end(); ++it)
            {
                msg.append(it->first);
                msg.append(":");
                msg.append(it->second);
                msg.append("\r\n");
            }
        msg.append("\r\n"); // 结束标志

        if (-1 == send(m_skfd, msg.c_str(), msg.length(), 0))
        {
            std::cerr << "[ERROR]:send request massage to host faild" << std::endl;
            return NULL;
        }

        char reply[1024] = {0};
        clock_t clk_start = clock(); //
        for (uint32_t i = 0; NULL == strstr(reply, "\r\n\r\n"); i++)
        {
            recv(m_skfd, reply + i, 1, 0);
            if ((double)(clock() - clk_start) / CLOCKS_PER_SEC > 5)
            {
                std::cerr << "[ERROR]:timeout no get service host reply" << std::endl;
                return NULL;
            }
        }

        if (0 == checkReplyCode(reply))
            return NULL;

        m_data.size = parseReplyDownloadsMsg(reply);
        if (0 == m_data.size)
            return NULL;

        if (m_data.ptr)
            delete[] m_data.ptr;
        m_data.ptr = new char[m_data.size];
        memset(m_data.ptr, 0, m_data.size);
        uint16_t num = 0;
        long long count = 0;
        clk_start = clock();
        hideCursor();
        while (1)
        {
            memset(reply, 0, sizeof(reply));
            num = recv(m_skfd, reply, sizeof(reply), 0);
            if (0 == num)
            {
                std::cerr << "[INFO]:already get all data" << std::endl;
                return &m_data;
            }
            else if (0 > num)
            {
                std::cerr << "[ERROR]:receive file data faild" << std::endl;
                delete[] m_data.ptr;
                m_data.size = 0;
                m_data.ptr = NULL;
                return NULL;
            }

            memcpy(m_data.ptr + count, reply, num);
            count += num;

            if ((double)(clock() - clk_start) / CLOCKS_PER_SEC > 1)
            {
                std::cerr << "[INFO]:Downloading: [ "
                          << std::setw(5) << (count / 1024 / 1024) << "MB / "
                          << std::setw(5) << (m_data.size / 1024 / 1024) << " MB( "
                          << std::fixed << std::setprecision(2)
                          << ((double)((double)count / m_data.size)) * 100 << "%)]\r";

                clk_start = clock();
            }

            if (count >= m_data.size)
            {
                std::cerr << "[INFO]:Downloading: [ "
                          << std::setw(5) << (count / 1024 / 1024) << "MB / "
                          << std::setw(5) << (m_data.size / 1024 / 1024) << " MB( "
                          << std::fixed << std::setprecision(2)
                          << ((double)((double)count / m_data.size)) * 100 << "%)]"
                          << "[Done]" << std::endl;
                showCursor();
                return &m_data;
            }
        }
    }

    // void Skip::post()
    // {
    // }

    skip::Data *Skip::head(skip::c_char *url, skip::maps *headers, skip::maps *params)
    {
        if (m_link == false)
        {
            if (tryConnect(url, 80) == false)
            {
                std::cerr << "[WARN]:try connect service host port [80 ] faild" << std::endl;
                if (tryConnect(url, 443) == false)
                {
                    std::cerr << "[WARN]:try connect service host port [443] faild" << std::endl;
                    std::cerr << "[WARN]:auto connect serivce faild; please use "
                                 "try_connect function and point port"
                              << std::endl;
                    return NULL;
                }
            }
        }

        skip::str msg("HEAD /");
        if (0 != m_path.length())
            msg.append(m_path);
        msg.append(" HTTP/1.1\r\nhost:");
        msg.append(m_host);
        msg.append("\r\n");
        if (headers)
            for (skip::maps::iterator it = headers->begin(); it != headers->end(); ++it)
            {
                msg.append(it->first);
                msg.append(":");
                msg.append(it->second);
                msg.append("\r\n");
            }
        if (params)
            for (skip::maps::iterator it = params->begin(); it != params->end(); ++it)
            {
                msg.append(it->first);
                msg.append(":");
                msg.append(it->second);
                msg.append("\r\n");
            }
        msg.append("\r\n"); // 结束标志

        if (-1 == send(m_skfd, msg.c_str(), msg.length(), 0))
        {
            std::cerr << "[ERROR]:send request massage to host faild" << std::endl;
            return NULL;
        }

        char reply[1024] = {0};
        clock_t clk_start = clock(); //
        for (uint32_t i = 0; NULL == strstr(reply, "\r\n\r\n"); i++)
        {
            recv(m_skfd, reply + i, 1, 0);
            if ((double)(clock() - clk_start) / CLOCKS_PER_SEC > 5)
            {
                std::cerr << "[ERROR]:timeout no get service host reply" << std::endl;
                return NULL;
            }
        }
        m_data.size = strlen(reply) + 1;
        if (m_data.ptr)
            delete[] m_data.ptr;
        m_data.ptr = new char[m_data.size];
        memset(m_data.ptr, 0, m_data.size);
        memcpy(m_data.ptr, reply, m_data.size);
        return &m_data;
    }

    // void Skip::trace()
    // {
    // }

    skip::Data *Skip::options(skip::c_char *url, skip::maps *headers, skip::maps *params)
    {
        if (m_link == false)
        {
            if (tryConnect(url, 80) == false)
            {
                std::cerr << "[WARN]:try connect service host port [80 ] faild" << std::endl;
                if (tryConnect(url, 443) == false)
                {
                    std::cerr << "[WARN]:try connect service host port [443] faild" << std::endl;
                    std::cerr << "[WARN]:auto connect serivce faild; please use "
                                 "try_connect function and point port"
                              << std::endl;
                    return NULL;
                }
            }
        }
        skip::str msg("OPTIONS /");
        if (0 != m_path.length())
            msg.append(m_path);
        msg.append(" HTTP/1.1\r\nhost:");
        msg.append(m_host);
        msg.append("\r\n");

        msg.append("\r\n"); // 结束标志

        if (-1 == send(m_skfd, msg.c_str(), msg.length(), 0))
        {
            std::cerr << "[ERROR]:send request massage to host faild" << std::endl;
            return NULL;
        }

        char reply[1024] = {0};
        clock_t clk_start = clock(); //
        for (uint32_t i = 0; NULL == strstr(reply, "\r\n\r\n"); i++)
        {
            recv(m_skfd, reply + i, 1, 0);
            if ((double)(clock() - clk_start) / CLOCKS_PER_SEC > 5)
            {
                std::cerr << "[ERROR]:timeout no get service host reply" << std::endl;
                return NULL;
            }
        }
        if (0 == checkReplyCode(reply))
            return NULL;

        m_data.size = strlen(reply) + 1;
        if (m_data.ptr)
            delete[] m_data.ptr;
        m_data.ptr = new char[m_data.size];
        memset(m_data.ptr, 0, m_data.size);
        memcpy(m_data.ptr, reply, m_data.size);
        return &m_data;
    }

    Skip::~Skip()
    {
        if (m_data.ptr)
            delete[] m_data.ptr;
        freeaddrinfo(m_info);
        closesocket(m_skfd);
        WSACleanup();
    }

} // namespace skip
