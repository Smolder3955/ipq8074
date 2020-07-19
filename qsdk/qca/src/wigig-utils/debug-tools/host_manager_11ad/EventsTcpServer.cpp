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

#include "EventsTcpServer.h"
#include "Host.h"
#include "DebugLogger.h"
#include "TcpNetworkInterface.h"

using namespace std;

EventsTcpServer::EventsTcpServer(unsigned int eventsTcpPort)
    :m_port(eventsTcpPort), m_pSocket(new TcpNetworkInterfaces::NetworkInterface("Events")), m_running(true)
{
}

void EventsTcpServer::Start()
{
    LOG_INFO << "Starting events TCP server on port " << m_port << endl;

    m_pSocket->Bind(m_port);
    m_pSocket->Listen();

    //Infinite loop because it has to wait for clients to connect to it forever, there's no reason to stop it
    //unless there is a problem.
    while (m_running)
    {
        try
        {
            unique_ptr<TcpNetworkInterfaces::NetworkInterface> newClient = m_pSocket->Accept();
            //LOG_INFO << "Adding a new client to the Events TCP Server: " << newClient.getPeerName() << endl;
            //using unique_lock promises that in case of exception the mutex is unlocked:
            lock_guard<mutex> clientsVectorLock(m_clientsVectorMutex);
            m_clientsVector.push_back(move(newClient));
        }
        catch (exception e)
        {
            LOG_ERROR << "Couldn't start a new connection with a new client on events TCP server" << e.what() << endl;
        }
    }
}

bool EventsTcpServer::SendToAllConnectedClients(const string& message)
{
    LOG_VERBOSE << "Sending to all connected clients: " << message << std::endl;

    try
    {
        //using lock_guard promises that in case of exception the mutex is unlocked:
        lock_guard<mutex> clientsVectorLock(m_clientsVectorMutex); //locks the mutex because the for loop iterates on clientsVector
        for (auto client = m_clientsVector.begin(); client != m_clientsVector.end(); )
        {
            try
            {
                if (!(*client)->SendString(message))
                { //it means the client had disconnected, remove the client from the clients list
                    LOG_WARNING << "Client: " << (*client)->GetPeerName() << " has disconnected, removing from the clients list" << endl;
                    client = m_clientsVector.erase(client);
                }
                else
                {
                    ++client;
                }
            }
            catch (exception e)
            {
                string peerName = "Unknown client";
                if (client != m_clientsVector.end())
                {
                    peerName = (*client)->GetPeerName();
                }
                LOG_WARNING << "Couldn't send the event to the client: " << peerName << " " << e.what() << endl;

            }
        }
    }
    catch (exception e)
    {
        LOG_WARNING << "Couldn't send the event to all the clients" << e.what() << endl;
        return false;
    }

    return true;
}

void EventsTcpServer::Stop()
{
    LOG_INFO << "Stopping the events TCP server" << endl;
    m_pSocket.reset();

    m_running = false;
}
