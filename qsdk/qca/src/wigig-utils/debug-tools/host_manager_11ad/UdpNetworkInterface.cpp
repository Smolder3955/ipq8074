/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
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

#ifdef __linux
#include <stdlib.h>
#include <string.h>
#include <cerrno>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>

#elif _WINDOWS
/* Added here so windows.h will not include winsock.h that is interfering with winsock2.h */
#define _WINSOCKAPI_
#include <windows.h>

#else
#include <dirent.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/if.h>
#include <arpa/inet.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include "DebugLogger.h"
#include "UdpNetworkInterface.h"

UdpNetworkInterface::UdpNetworkInterface(const std::string& broadcastIp, int portIn, int portOut) :
    m_portIn(portIn),
    m_portOut(portOut),
    m_broadcastIp(broadcastIp)
{
}

UdpNetworkInterface::~UdpNetworkInterface()
{
    Close();
}

bool UdpNetworkInterface::Initialize(void)
{
#ifndef _WINDOWS
    // create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket < 0)
    {
        LOG_WARNING << "Cannot open UDP socket" << std::endl;
        return false;
    }

    // set broadcast flag on
    int enabled = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (char*)&enabled, sizeof(enabled)) < 0)
    {
        std::string error = "Cannot set broadcast option for UDP socket, error: ";
        error += strerror(errno);

        LOG_WARNING << error << std::endl;
        return false;
    }

    // bind socket to portIn
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_portIn);
    if (::bind(m_socket, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
    {
        std::string error = "Cannot bind UDP socket to the port, error: ";
        error += strerror(errno);

        LOG_WARNING << error << std::endl;
        return false;
    }

    return true;
#else
    return false;
#endif
}

int UdpNetworkInterface::Send(std::string message)
{
#ifndef _WINDOWS
    struct sockaddr_in dstAddress;
    dstAddress.sin_family = AF_INET;
    inet_pton(AF_INET, m_broadcastIp.c_str(), &dstAddress.sin_addr.s_addr);
    dstAddress.sin_port = htons(m_portOut);
    int messageSize = message.length() * sizeof(char);
    int result = sendto(m_socket, message.c_str(), messageSize, 0, (sockaddr*)&dstAddress, sizeof(dstAddress));
    LOG_VERBOSE << "INFO : sendto with sock_out=" << m_socket << ", message=" << message << " messageSize=" << messageSize << " returned with " << result << std::endl;
    if (result < 0)
    {
        LOG_WARNING << "ERROR : Cannot send udp broadcast message, error " << ": " << strerror(errno) << std::endl;
        return 0;
    }
    return messageSize;
#else
    return 0;
#endif
}

size_t UdpNetworkInterface::Receive(char* pBuf, size_t capacity)
{
#ifndef _WINDOWS
    ssize_t msgLength = recvfrom(m_socket, pBuf, capacity, 0, NULL, 0);
    if (msgLength <= 0)
    {
        LOG_WARNING << "Can't receive UDP message from port " << m_portIn << std::endl;
        return 0;
    }

    return msgLength;
#else
    (void)pBuf;
    (void)capacity;
    return 0;
#endif
}

void UdpNetworkInterface::Close()
{
#ifndef _WINDOWS
    close(m_socket);
#endif
}
