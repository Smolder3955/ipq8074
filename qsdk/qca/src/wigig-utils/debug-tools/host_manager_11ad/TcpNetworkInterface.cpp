/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TcpNetworkInterface.h"
#include "DebugLogger.h"

#include <cerrno>
#include <string.h>
#include <stdlib.h>
#include <algorithm>

#ifndef _WINDOWS
#include <netinet/tcp.h>
#else
#include <winsock2.h>
#endif

using namespace TcpNetworkInterfaces;

// *************************************************************************************************

NetworkInterface::NetworkInterface(const char* szName): m_channelName(szName)
{
    m_fileDescriptor = static_cast<int>(socket(AF_INET6, SOCK_STREAM, 0));
    if (m_fileDescriptor < 0)
    {
        LOG_ERROR << "Cannot create socket file descriptor: " << strerror(errno) << std::endl;
        exit(1);
    }

    // On Linux-based platforms SO_REUSEADDR allows to bind a socket to an address unless an active connection is present.
    // This is a default behavior on Windows. On Windows, SO_REUSEADDR allows you to additionally bind multiple sockets to the same addresses.
#ifdef  _WINDOWS
    DWORD DWv6only = 0;
    char* v6only = (char*)&DWv6only;
#else
    int optval = 1;
    int DWv6only = 0;
    int* v6only = &DWv6only;

    // On Linux-based platforms SO_REUSEADDR allows to bind a socket to an address unless an active connection is present.
    // This is required to restart the application quickly. On Windows this flags allows several processes to bind to the
    // same port, so we cannot set this flag on Windows.
    if (setsockopt(m_fileDescriptor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        LOG_ERROR << "Failed to configure socket parameter [SO_REUSEADDR]: " << strerror(errno) << std::endl;
    }

#endif //  _WINDOWS

    // Set the IPV6_V6ONLY to '0' to enable dual mode socket (support both IPv4 and IPv6)
    if (setsockopt(m_fileDescriptor, IPPROTO_IPV6, IPV6_V6ONLY, v6only, sizeof(DWv6only)) < 0)
    {
        LOG_ERROR << "Failed to configure socket parameter [IPV6_V6ONLY]: " << strerror(errno) << std::endl;
    }

    m_bufferSize = 0;
    m_buffer = nullptr;
}

// *************************************************************************************************

NetworkInterface::NetworkInterface(const char* szName, int fileDescriptor)
    : m_fileDescriptor(fileDescriptor)
    , m_buffer(nullptr)
    , m_bufferSize(0U)
    , m_commandBuffer(MAX_COMMAND_BUFFER_SIZE)
    , m_channelName(szName)
{

    UpdatePeerNameInternal();
}

// *************************************************************************************************

NetworkInterface::~NetworkInterface()
{
    Close();
}

// *************************************************************************************************

// Establish Connections
void NetworkInterface::Bind(int port)
{
    m_localAddress.sin6_family = AF_INET6;
    m_localAddress.sin6_port = htons(static_cast<unsigned short>(port));
    m_localAddress.sin6_addr = in6addr_any;
    memset(&m_localAddress.sin6_flowinfo, 0, sizeof m_localAddress.sin6_flowinfo);
    memset(&m_localAddress.sin6_scope_id, 0, sizeof m_localAddress.sin6_scope_id);

    LOG_INFO << "Binding channel " << m_channelName << " to port " << port << std::endl;
    int res = bind(m_fileDescriptor, reinterpret_cast<struct sockaddr*>(&m_localAddress), sizeof(m_localAddress));

    if (0 != res)
    {
        LOG_ERROR << "Cannot bind listener to port " << port << ": " << strerror(errno) << std::endl;
        LOG_ERROR << "Please verify if another application instance is running" << std::endl;
        exit(1);
    }
}

// *************************************************************************************************

bool NetworkInterface::InitializeNetwork()
{
#ifdef _WINDOWS
    // Calling WSAStartup is necessary to be called once for a process. It a must calling this function
    // in order to start using the winsock2 under Windows platforms. When not needed any more,
    // WSACleanup is needed to be called (not calling it wouldn't be so bad, but it is preferred)

    WSADATA wsa;
    int err = WSAStartup(MAKEWORD(2, 0), &wsa);
    if (0 != err)
    {
        LOG_ERROR << "Cannot initialize network library (WSAStartup) with error: " << err << std::endl;
        return false;
    }
#endif
    return true;
}

// *************************************************************************************************

void NetworkInterface::FinalizeNetwork()
{
#ifdef _WINDOWS
    WSACleanup();
#endif
}

// *************************************************************************************************

void NetworkInterface::Listen(int backlog)
{
    if (-1 == listen(m_fileDescriptor, backlog))
    {
        LOG_ERROR << "listen() call failed. Check if an application is already running."
            << " Error: " << strerror(errno)
            << " Channel: " << m_channelName
            << std::endl;
        exit(1);
    }
}

// *************************************************************************************************

std::unique_ptr<NetworkInterface> NetworkInterface::Accept()
{
    struct sockaddr_in6 remoteAddress;
    socklen_t size = sizeof(remoteAddress);

    int optval = 1;
    int result = setsockopt(m_fileDescriptor, IPPROTO_TCP, TCP_NODELAY, (char*)&optval, sizeof(int));
    if (result < 0)
    {
        LOG_ERROR << "setsockopt() call failed."
            << " Error: " << strerror(errno)
            << " Channel: " << m_channelName
            << std::endl;
        exit(1);
    }
    int fileDescriptor = static_cast<int>(accept(m_fileDescriptor, (struct sockaddr*)&remoteAddress, &size));
    LOG_INFO << "New client connection accepted."
        << " Channel: " << m_channelName
        << " fd: " << fileDescriptor
        << std::endl;
    if (fileDescriptor <= 0)
    {
        LOG_ERROR << "accept() call failed."
            << " Channel: " << m_channelName
            << " Error: " << strerror(errno)
            << std::endl;
        exit(1);
    }

    return std::unique_ptr<NetworkInterface>(new NetworkInterface(m_channelName.c_str(), fileDescriptor));
}

// *************************************************************************************************

bool NetworkInterface::SendString(const std::string& text)
{
    //LOG_VERBOSE << "Sending text: " << PlainStr(text) << std::endl;
    return SendBuffer(text.c_str(), text.size());
}

// *************************************************************************************************

bool NetworkInterface::SendString(const char* szText)
{
    return SendBuffer(szText, strlen(szText));
}

// *************************************************************************************************

bool NetworkInterface::SendBuffer(const char* pBuf, size_t bufSize)
{
    size_t sentSize = 0;
    const char* pCurrent = pBuf;

    while (sentSize < bufSize)
    {
        int chunkSize = send(m_fileDescriptor, pCurrent, static_cast<int>(bufSize - sentSize), 0);
        if (chunkSize < 0)
        {
            LOG_ERROR << "Error sending data."
                << " Channel: " << m_channelName
                << " Error: " << strerror(errno)
                << " Sent till now (B): " << sentSize
                << " To be sent (B): " << bufSize
                << std::endl;

            return false;
        }

        sentSize += chunkSize;
        pCurrent += chunkSize;

        LOG_VERBOSE << "Sent data chunk."
                    << " Channel: " << m_channelName
                    << " Chunk Size (B): " << chunkSize
                    << " Sent till now (B): " << sentSize
                    << " To be sent (B): " << bufSize
                    << std::endl;
    }

    LOG_VERBOSE << "Buffer sent successfully."
                << " Channel: " << m_channelName
                << " Sent (B): " << sentSize
                << " Buffer Size (B): " << bufSize
                << std::endl;
    LOG_ASSERT(sentSize == bufSize);

    return true;
}

// *************************************************************************************************

bool NetworkInterface::Receive(std::string& message)
{
    message.clear();
    const size_t length = m_commandBuffer.size();
    char* commandBuffer = m_commandBuffer.data();

    if (length == 0U) // shouldn't happen
    {
        LOG_ERROR << "Error while receiving from a TCP socket: command buffer is not initialized"
            << " Channel: " << m_channelName << std::endl;
        return false;
    }

    size_t offset = 0U;
    while (offset < length)
    {
        int bytesReceived = recv(m_fileDescriptor, commandBuffer + offset, length - offset, 0 /*flags*/);
        if (-1 == bytesReceived)
        {
            LOG_ERROR << "recv() call failed."
                << " Error: " << strerror(errno)
                << " Channel: " << m_channelName
                << std::endl;
            return false;
        }
        if (0 == bytesReceived)
        {
            LOG_INFO << "Connection closed by peer"
                << " Channel: " << m_channelName
                << " Peer: " << m_peerName
                << std::endl;
            return false;
        }

        offset += bytesReceived;

        LOG_VERBOSE << "Received data chunk."
            << " Channel: " << m_channelName
            << " Chunk Size (B): " << bytesReceived
            << ", received till now (B): " << offset
            << std::endl;

        char* end = commandBuffer + offset;
        char* pSeparator = std::find(commandBuffer, end, '\r');

        // TODO
        // For backward compatibility with Hercules tools that does not add \r in the end
        // we cannot return to wait for the rest of the message.
        // Note: To send \r in Hercules add "#013"
        // Currently, we preserve the (wrong) concept that message comes in one chunk and it doesn't
        // have to be terminated with \r.
        /*if (pSeparator == end)
        {
            LOG_VERBOSE << "No separator found in the message, continue to receive the rest" << std::endl;
            continue;
        }*/
        if (pSeparator == end)
        {
            // Note: this will not work in case where message is exactly in the length of the buffer,
            // but this code will be removed when the above note is fixed
            commandBuffer[offset] = '\0';
            message = commandBuffer;
            LOG_VERBOSE << "Received buffer without trailing carriage return."
                << " Channel: " << m_channelName
                << " Received (B): " << offset
                << std::endl;
            return true;
        }

        if (pSeparator != end - 1)
        {
            offset = std::distance(commandBuffer, pSeparator) + 1; // try to handle the first message, throw the rest
            LOG_WARNING << "Received ill-formed message from a TCP socket, data after message separator is ignored"
                << " Channel: " << m_channelName << std::endl;
        }

        // override trailing \r with zero
        commandBuffer[offset - 1] = '\0';
        message = commandBuffer;
        LOG_VERBOSE << "Buffer received successfully."
            << " Channel: " << m_channelName
            << " Received (B): " << offset
            << std::endl;
        return true;
    }

    LOG_ERROR << "Error while receiving from a TCP socket: message is longer than the buffer size (" << length << "B)" << std::endl;
    return false;
}

// *************************************************************************************************

const char* NetworkInterface::BinaryReceive(int size, int flags)
{
    if (static_cast<int>(m_bufferSize) <= size)
    {
        char* buffer = (char*)(realloc(m_buffer, sizeof(char) * size));
        if (buffer == nullptr)
        {
            return nullptr;
        }
        m_buffer = buffer;
        m_bufferSize = size;
    }

    memset(m_buffer, 0, m_bufferSize);

    int bytesReceived = recv(m_fileDescriptor, m_buffer, size, flags);
    if (-1 == bytesReceived)
    {
        LOG_ERROR << "recv() call failed for binary data "
            << " Channel: " << m_channelName
            << " Error: " << strerror(errno)
            << std::endl;
        return nullptr;
    }
    if (0 == bytesReceived)
    {
        LOG_INFO << "Connection closed by peer "
            << " Channel: " << m_channelName
            << " Peer: " << m_peerName
            << std::endl;
        return nullptr;
    }

    return m_buffer;
}

// *************************************************************************************************

// Socket Closing Functions
void NetworkInterface::Close()
{
    LOG_INFO << "Closing socket."
        << " Channel: " << m_channelName
        << std::endl;

#ifdef _WINDOWS
    closesocket(m_fileDescriptor);
#elif __linux
    close(m_fileDescriptor);
#else
    shutdown(m_fileDescriptor, SHUT_RDWR);
#endif
}

// *************************************************************************************************

void NetworkInterface::UpdatePeerNameInternal()
{
    struct sockaddr_in6 remoteAddress;
    socklen_t size = sizeof(remoteAddress);

    char astring[INET6_ADDRSTRLEN]; //alocating buffer for address conversion to string
    if (-1 == getpeername(m_fileDescriptor, (struct sockaddr*)&remoteAddress, &size))
    {
        LOG_ERROR << "getpeername() call failed."
            << " Error: " << strerror(errno)
            << " Channel: " << m_channelName
            << std::endl;
        exit(1);
    }
    inet_ntop(AF_INET6, &(remoteAddress.sin6_addr), astring, INET6_ADDRSTRLEN);
    m_peerName = std::string(astring);
}

// *************************************************************************************************

const char* NetworkInterface::GetPeerName() const
{
    return m_peerName.c_str();
}
