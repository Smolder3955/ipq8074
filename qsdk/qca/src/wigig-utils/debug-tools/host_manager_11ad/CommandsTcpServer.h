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

#ifndef _COMMANDSTCPSERVER_H_
#define _COMMANDSTCPSERVER_H_

#include <memory>
#include "HostDefinitions.h"

class Host;
namespace TcpNetworkInterfaces { class NetworkInterface; }

/*
* Commands TCP Server is a synchronous server that handles each client separately.
* When a client sends a new command to the Commands TCP Server, the host processes it and sends back to the client an answer.
*/
class CommandsTcpServer
{
public:

    /*
    * Commands TCP server constructor gets the port to start in. It also initializes new Socket object.
    */
    CommandsTcpServer(unsigned int commandsTcpPort, Host& host);
    /*
    * Start the commands TCP server at the given port (given in the constructor).
    * For each new client that is connecting to the server it opens a new thread and
    * continue to listen on the port for more clients.
    */
    void Start();

    /*
    * Stop the commands TCP server by doing some clean ups for the sockets.
    */
    void Stop();

private:
    unsigned int m_port; //The port in which the commands TCP server is working on
    std::shared_ptr<TcpNetworkInterfaces::NetworkInterface> m_pSocket; //an object that holds the connection with each client
    Host& m_host; // reference to the host object (that is passed to commandsHandler each time a new TCP connection is created)

    void ServerThread(std::unique_ptr<TcpNetworkInterfaces::NetworkInterface> client);
    ConnectionStatus Reply(TcpNetworkInterfaces::NetworkInterface *client, ResponseMessage &responseMessage);
    ConnectionStatus ReplyBuffer(TcpNetworkInterfaces::NetworkInterface *client, ResponseMessage &responseMessage);
    ConnectionStatus ReplyFile(TcpNetworkInterfaces::NetworkInterface *client, ResponseMessage &fileName);
    ConnectionStatus ReplyBinary(TcpNetworkInterfaces::NetworkInterface *client, ResponseMessage &responseMessage);

    bool m_running;
};


#endif // !_COMMANDSTCPSERVER_H_

