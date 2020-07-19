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

#ifndef _UDPSERVER_H_
#define _UDPSERVER_H_

#include <string>
#include <memory>

#include "HostDefinitions.h"
#include "CommandsHandler.h"

class Host;
class UdpNetworkInterface;

class UdpServer
{
public:
    /*
    UdpServer
    Creates a UDP socket which receives messages from the network and can response using a broadcast option
    */
    UdpServer(unsigned int udpPortIn, unsigned int udpPortOut, Host& host);

    /*
    StartServer
    Starts to receive messages, Handles the message and continue to the receive
    @param: none
    @return: none
    */
    void StartServer();

    /*
    StopServer
    Stops receiving messages
    @param: none
    @return: none
    */
    void Stop();

private:
    /*
    BlockingReceive
    Waits for a UDP message and handles it
    Assumption: m_pSocket is valid
    @param: none
    @return: none
    */
    void BlockingReceive();

    /*
    SendBroadcastMessage
    Sends a message to all hosts in the subnet
    Assumption: m_pSocket is valid
    @param: responseMessage - the message to send
    @return: none
    */
    void SendBroadcastMessage(ResponseMessage responseMessage) const;

    unsigned int m_udpPortIn; // the local host port
    unsigned int m_udpPortOut; // the remote host port
    std::string m_broadcastIp;
    std::unique_ptr<UdpNetworkInterface> m_pSocket;
    CommandsHandler m_CommandHandler;
    bool m_running;
};


#endif // !_UDPSERVER_H_