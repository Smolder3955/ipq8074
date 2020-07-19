/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _TCP_CLIENT_
#define _TCP_CLIENT_

#ifdef _WINDOWS
    #pragma comment(lib, "Ws2_32.lib")
#endif

#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>
#include <memory>

#ifdef _WINDOWS
    #include <winsock2.h>
    #include <Ws2tcpip.h>/* Added to support IPv6*/
#elif __linux
    #include <unistd.h>
    #include <netdb.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/wait.h>
#else
    #include <sys/socket.h>
    #include <net/route.h>
    #include <net/if.h>
    #include <arpa/inet.h>
#endif

#include "LogHandler.h"

#ifdef _WINDOWS
    typedef int socklen_t;
#endif

class TcpClient
{
public:
    TcpClient();
    ~TcpClient();
    static bool InitializeNetwork();
    static void FinalizeNetwork();
    bool ConnectSendCommandGetResponse(const std::string & command, const std::string &ip, const std::string &port, std::string &response);
private:
    bool ConnectToHost(const std::string &ip, const std::string & port);
    bool SendCommand(const std::string &command);
    bool SendCommand(const char* command, size_t bufSize);
    bool GetResponse(std::string &response);
    struct sockaddr_in6 m_localAddress;
    struct addrinfo m_hints, *m_pRes = nullptr;
    int m_socketFD;
    void closeSocketFd();
};

#endif