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

#ifndef _EVENTSTCPSERVER_H_
#define _EVENTSTCPSERVER_H_

#include <memory>
#include <vector>
#include <mutex>

namespace TcpNetworkInterfaces { class NetworkInterface; }

/*
 *TODO - add a function to the events TCP server: when some client connects to the host server, the host server informs every client
 *(include the one that just connected) that another client had connected to the host
 */

/*
* Events TCP server is a asynchronous server that broadcasts events that come from the device or the host to all connected clients.
* If a client wants to get events he has to connect this server (separately from the Commands TCP server).
* This server doesn't get any messages from clients.
*/
class EventsTcpServer
{
public:
    /*
    * Events TCP server constructor gets the port to start in. It also initializes new Socket object.
    */
    EventsTcpServer(unsigned int eventsTcpPort);

    /*
    * Start the events TCP server at the given port (given in the constructor).
    * For each new client that is connecting to the server, it adds it to a clients list.
    */
    void Start();

    /*
    * Send an event to all registered clients (clients that exist in the list).
    * If the connection is lost, remove the client from the list (it is the client's responsibility
    * to renew the connection with this server).
    */
    bool SendToAllConnectedClients(const std::string& message);
    /*
    * Stop the events TCP server by doing some clean ups for the sockets.
    */
    void Stop();

private:
    unsigned int m_port; //The port in which the events TCP server is working on
    std::shared_ptr<TcpNetworkInterfaces::NetworkInterface> m_pSocket;
    // clientsVector is a vector that keeps all the clients that are connected to the server and want to get events from the device or the host
    std::vector<std::unique_ptr<TcpNetworkInterfaces::NetworkInterface>> m_clientsVector;
    std::mutex m_clientsVectorMutex; //Two different (or more) threads can access the clients vector - a mutex is needed

    bool m_running;
};



#endif // !_EVENTSTCPSERVER_H_
