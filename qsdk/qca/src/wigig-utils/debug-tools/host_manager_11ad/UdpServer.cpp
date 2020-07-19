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

#include "UdpServer.h"
#include "CommandsHandler.h"
#include "Host.h"
#include "HostInfo.h"
#include "FileSystemOsAbstraction.h"
#include "DebugLogger.h"
#include "UdpNetworkInterface.h"

using namespace std;

UdpServer::UdpServer(unsigned int udpPortIn, unsigned int udpPortOut, Host& host) :
    m_udpPortIn(udpPortIn),
    m_udpPortOut(udpPortOut),
    m_broadcastIp(host.GetHostInfo().GetIps().m_broadcastIp),
    m_CommandHandler(stUdp, host),
    m_running(false)
{
#ifndef _WINDOWS
    m_pSocket.reset(new UdpNetworkInterface(m_broadcastIp, udpPortIn, udpPortOut));
    if (!m_pSocket->Initialize())
    {
        m_pSocket.reset();
    }
#else
    (void)udpPortOut;
    LOG_INFO << "UDP host discovery is supported on Linux OS only" << endl;
#endif
}

void UdpServer::StartServer()
{
    if (FileSystemOsAbstraction::LOCAL_HOST_IP == m_broadcastIp)
    {
        LOG_WARNING << "Can't start UDP server due to invalid host's IP/broadcast IP" << std::endl;
        return;
    }

    if (m_pSocket)
    {
        LOG_DEBUG << "Start UDP server on local port " << m_udpPortIn << std::endl;
        LOG_DEBUG << "Broadcast messages are sent to port " << m_udpPortOut << std::endl;
        m_running = true;
        BlockingReceive();
    }
}

void UdpServer::Stop()
{
    LOG_INFO << "Stopping the UDP server" << endl;
    m_pSocket.reset();
    m_running = false;
}

void UdpServer::BlockingReceive()
{
    static const size_t INCOMING_DATA_BUFFER_LEN = 1024;
    std::unique_ptr<char[]> spDataBuffer(new char[INCOMING_DATA_BUFFER_LEN]);
    if (!spDataBuffer)
    {
        LOG_ERROR << "Cannot allocate UDP data buffer of " << INCOMING_DATA_BUFFER_LEN << " B" << std::endl;
        return;
    }

    do
    {
        size_t msgLength = m_pSocket->Receive(spDataBuffer.get(), INCOMING_DATA_BUFFER_LEN);
        std::string incomingMessage(spDataBuffer.get(), msgLength);
        LOG_VERBOSE << "Got UDP message: " << incomingMessage << endl;

        ResponseMessage referencedResponse;
        m_CommandHandler.ExecuteCommand(incomingMessage, referencedResponse);
        if (referencedResponse.length > 0)
        {
            LOG_VERBOSE << "Send broadcast message" << endl;
            SendBroadcastMessage(referencedResponse);
        }

    } while (m_running);
}

void UdpServer::SendBroadcastMessage(ResponseMessage responseMessage) const
{
    m_pSocket->Send(responseMessage.message);
}
