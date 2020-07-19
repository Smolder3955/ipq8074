/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <cerrno>
#include <string>
#include <stdlib.h>
#include <algorithm>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <algorithm> // for find
#include <errno.h>

#ifndef _WINDOWS
    #include <netinet/tcp.h>
    #define SOCKET_ERROR            (-1)
#else
    #include <winsock2.h>
#endif
#include "TcpClient.h"

using std::endl;
using std::string;


TcpClient::TcpClient()
{
    InitializeNetwork();
}


TcpClient::~TcpClient()
{
    FinalizeNetwork();
}


bool TcpClient::InitializeNetwork()
{
#ifdef _WINDOWS
    // Calling WSAStartup is necessary to be called once for a process. we Must call this function
    // in order to start using the winsock2 under Windows platforms. When not needed any more,
    // WSACleanup is needed to be called (not calling it wouldn't be so bad, but it is preferred)

    WSADATA wsa;
    int err;
    err = WSAStartup(MAKEWORD(2, 0), &wsa);
    if (0 != err)
    {
        LOG_ERROR << "Cannot initialize network library (WSAStartup) with error: " << err << std::endl;
        return false;
    }
#endif
    return true;
}

void TcpClient::FinalizeNetwork()
{
#ifdef _WINDOWS
    WSACleanup();
#endif
}

bool TcpClient::ConnectSendCommandGetResponse(const string & command, const string & ip, const string &port, string &response)
{
    return (ConnectToHost(ip, port) && SendCommand(command) && GetResponse(response));
}


bool TcpClient::ConnectToHost(const string & hostName, const string & port)
{
    int portNum = atoi(port.c_str());
    if (portNum < 1023 || portNum > 65535) // min port is 1023, max port is 65535
    {
        LOG_ERROR << "Invalid Port was provided" << endl;
        return false;
    }
    memset(&m_hints, 0x00, sizeof(m_hints));
    m_hints.ai_flags = AI_NUMERICSERV;
    m_hints.ai_socktype = SOCK_STREAM;
    int rc;
    if (inet_pton(AF_INET, hostName.c_str(), &m_localAddress) == 1)    /* valid IPv4 text address? */
    {
        m_hints.ai_family = AF_INET;
        m_hints.ai_flags |= AI_NUMERICHOST;
    }
    else if (inet_pton(AF_INET6, hostName.c_str(), &m_localAddress) == 1) /* valid IPv6 text address? */
    {

        m_hints.ai_family = AF_INET6;
        m_hints.ai_flags |= AI_NUMERICHOST;
    }
    else // The provided hostName is a machine name and not an IP
    {
        m_hints.ai_family = AF_UNSPEC;
    }

    // Here m_hints.ai_family is set to right v
    rc = getaddrinfo(hostName.c_str(), port.c_str(), &m_hints, &m_pRes);
    if (rc != 0)
    {
        LOG_ERROR << "Invalid IP was provided" << endl;
        return false;
    }

    m_socketFD = static_cast<unsigned int>(socket(m_pRes->ai_family, m_pRes->ai_socktype, m_pRes->ai_protocol));
    if (m_socketFD < 0)
    {
        LOG_ERROR << "Could not create socket. \nError: " << strerror(errno) << endl;
        return false;
    }
    LOG_INFO << "Socket created" << endl;

    if (connect(m_socketFD, m_pRes->ai_addr, static_cast<int>(m_pRes->ai_addrlen)) < 0)
    {
        LOG_ERROR << "Connect failed. IP: " << hostName << " PORT: " << port
            << "\nMake sure that the provided IP is valid and that host manager is running on target machine" << endl;
        return false;
    }
    LOG_INFO << "Connected\n" << endl;
    return true;
}


bool TcpClient::SendCommand(const std::string& text)
{
    LOG_INFO << "Sending command: " << text << std::endl;
    return SendCommand(text.c_str(), text.size());
}


bool TcpClient::SendCommand(const char* pBuf, size_t bufSize)
{
    LOG_INFO << "Sending command: " << pBuf << std::endl;
    if (bufSize == 0)
    {
        LOG_ERROR << "Cannot send empty command" << endl;
        return false;
    }
    size_t sentSize = 0;
    const char* pCurrent = pBuf;

    while (sentSize < bufSize)
    {
        int chunkSize = send(m_socketFD, pCurrent, static_cast<int>(bufSize - sentSize), 0);
        if (chunkSize < 0)
        {
            LOG_ERROR << "Error sending data."
                << " Error: " << strerror(errno)
                << " Sent till now (B): " << sentSize
                << " To be sent (B): " << bufSize
                << endl;
            return false;
        }

        sentSize += chunkSize;
        pCurrent += chunkSize;

        LOG_INFO << "Sent data chunk."
            << " Chunk Size (B): " << chunkSize
            << " Sent till now (B): " << sentSize
            << " To be sent (B): " << bufSize
            << endl;
    }

    LOG_INFO << "Buffer sent successfully."
        << " Sent (B): " << sentSize
        << " Buffer Size (B): " << bufSize
        << endl;

    if (sentSize != bufSize)
    {
        LOG_ERROR << "Fail to send JSON command, check connection to target" << std::endl;
        return false;
    }
    return true;
}


bool TcpClient::GetResponse(std::string &response)
{
    struct timeval timeout;
    timeout.tv_sec = 5; // set timeout to 5 seconds
    timeout.tv_usec = 0;
    response.clear();
    const size_t BUFFER_SIZE_BYTES = 4096;
    char buffer[BUFFER_SIZE_BYTES];

    fd_set set;
    FD_ZERO(&set); /* clear the set */
    FD_SET(static_cast<unsigned int>(m_socketFD), &set);
    size_t offset = 0;
    do {
        int bytesReceived;

        int rv = select(m_socketFD+1, &set, nullptr, nullptr, &timeout);
        if (rv == SOCKET_ERROR) // No FD available for selection
        {
            LOG_ERROR << "Error while receiving from a TCP socket (select error): " << strerror(errno) << endl;
            return false;
        }
        else if (rv == 0) // Timeout occurred (connection stopped not closed properly by the target)
        {
            LOG_INFO << "timeout, socket does not have anything to read" << endl;
            if (offset < BUFFER_SIZE_BYTES) {
                // override trailing \r with zero - to make sure we have a valid string.
                buffer[offset] = '\0';
                response = buffer;
                LOG_INFO << "Buffer received successfully. Received (B): " << offset << endl;
                closeSocketFd();
                if (offset == 0)
                {
                    LOG_ERROR << "Connection Error occurred. Please check host_manager_11ad status. " << std::endl;
                    return false;
                }
                return true; // return true only if we read something
            }
        }
        else // All Good
        {
            // socket has something to read
            bytesReceived = recv(m_socketFD, buffer + offset /*buffer write ptr*/, static_cast<int>(BUFFER_SIZE_BYTES - offset) /*bytes left in buffer*/, 0 /*flags*/);
            if (bytesReceived == SOCKET_ERROR) //unexpected error
            {
                LOG_ERROR << "Error while receiving from a TCP socket (recv error): " << strerror(errno) << std::endl;
                return false;
            }
            else if (bytesReceived == 0) // nothing to read or buffer is full
            {
                if (offset < BUFFER_SIZE_BYTES)
                {
                    // override trailing \r with zero
                    buffer[offset] = '\0';
                    response = buffer;
                    LOG_INFO << "Buffer received successfully. Received (B): " << offset << std::endl;
                    closeSocketFd();
                    return true;
                }
            }
            else // Had something to read update counters and try to read more
            {
                if (bytesReceived + offset < BUFFER_SIZE_BYTES)
                {
                    offset += bytesReceived;
                    LOG_INFO << "Buffer received successfully. Received (B): " << offset << std::endl;
                }
                else
                {
                    LOG_ERROR << "Error while receiving from a TCP socket: message is longer than the buffer size (" << BUFFER_SIZE_BYTES << "B)" << std::endl;
                    return false;
                }
            }
        }
    } while (true);
}

void TcpClient::closeSocketFd()
{
#ifdef _WINDOWS
    closesocket(m_socketFD);
#else
    close(m_socketFD);
#endif
}
