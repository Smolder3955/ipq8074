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

#include "Host.h"
#include "SharedVersionInfo.h"
#include "DebugLogger.h"
#include "PersistentConfiguration.h"
#include "LogCollector/LogCollector.h"
#include "ArgumentsParser.h"
#include "EventsDefinitions.h"
#include "JsonCommandsTcpServer.h"
#include "CommandsTcpServer.h"
#include "EventsTcpServer.h"
#include "UdpServer.h"
#include "DeviceManager.h"
#include "WebHttpServer.h"
#include "TaskScheduler.h"
#include "HostInfo.h"
#include "HostDefinitions.h"
#include "UdpNetworkInterface.h"
#include "TcpNetworkInterface.h"

#include <iostream>
#include <thread>
#include <functional> //Included for the "ref" in events TCP server thread

using namespace std;

// The private singleton Cntr
Host::Host()
    : m_spHostInfo(new HostInfo())
    , m_spTaskScheduler(new TaskScheduler())
    , m_keepAliveTaskId(0)
    , m_isEnumerating(true)
{
}

Host::~Host() {}

void Host::StartHost(bool logCollectorTracer3ppLogsFilterOn, ArgumentsParser& argumentParser)
{
    log_collector::LogCollector::Tracer3ppLogsFilterOn = logCollectorTracer3ppLogsFilterOn;

    // Should be initialized first to provide configuration to other modules
    m_spPersistentConfiguration.reset(new PersistentConfiguration(argumentParser.GetCustomConfigFile()));

    m_pEventsTcpServer.reset(new EventsTcpServer(m_spPersistentConfiguration->HostConfiguration.EventPort));
    thread threadEventsTcpServer = thread(&EventsTcpServer::Start, m_pEventsTcpServer.get());

    // Device Manager creation cannot be moved to Host constructor since it assumes Host is already created
    m_deviceManager.reset(new DeviceManager());

    m_keepAliveTaskId = m_spTaskScheduler->RegisterTask(
        std::bind(&Host::PublishKeepAliveEvent, this),
        std::chrono::seconds(static_cast<long long>(m_spPersistentConfiguration->HostConfiguration.KeepAliveIntervalSec)),
        true);

    m_pJsonCommandsTcpServer.reset(new JsonCommandsTcpServer(m_spPersistentConfiguration->HostConfiguration.JsonCommandPort, *this));
    thread threadJsonCommandsTcpServer = thread(&JsonCommandsTcpServer::Start, m_pJsonCommandsTcpServer.get());

    m_pCommandsTcpServer.reset(new CommandsTcpServer(m_spPersistentConfiguration->HostConfiguration.StringCommandPort, *this));
    thread threadCommandsTcpServer = thread(&CommandsTcpServer::Start, m_pCommandsTcpServer.get());

    m_pUdpServer.reset(new UdpServer(m_spPersistentConfiguration->HostConfiguration.UdpDiscoveryInPort, m_spPersistentConfiguration->HostConfiguration.UdpDiscoveryOutPort, *this));
    thread threadUdpServer = thread(&UdpServer::StartServer, m_pUdpServer.get());

    m_pHttpServer.reset(new WebHttpServer(m_spPersistentConfiguration->HostConfiguration.HttpPort));
    thread threadHttpServer = thread(&WebHttpServer::StartServer, m_pHttpServer.get());

    // TODO: consider to start it as early as possible (check if it can be done before Events TCP server because of logs and FW state events)
    m_spTaskScheduler->Start();

    threadEventsTcpServer.join();
    threadJsonCommandsTcpServer.join();
    threadCommandsTcpServer.join();
    threadUdpServer.join();
    threadHttpServer.join();
}

void Host::StopHost()
{
    m_spTaskScheduler->UnregisterTask(m_keepAliveTaskId); // stop sending keep alive, do it before stopping TCP server

    m_pEventsTcpServer->Stop();
    m_pJsonCommandsTcpServer->Stop();
    m_pCommandsTcpServer->Stop();
    m_pUdpServer->Stop();
    m_pHttpServer->Stop();

    LOG_INFO << "Done stopping all servers" << std::endl;
}

// Push the given event through Events TCP Server
void Host::PushEvent(const HostManagerEventBase& event) const
{
    if (!m_pEventsTcpServer)
    {    // shouldn't happen
        LOG_ERROR << "Host::PushEvent: Events TCP Server is not ready!" << endl;
        return;
    }

    m_pEventsTcpServer->SendToAllConnectedClients(event.ToJson());
}

// Retrieve host data
bool Host::GetHostUpdate(HostData& data)
{
    // Extract host IP
    data.m_hostIP = m_spHostInfo->GetIps().m_ip;

    // Extract host_manager version
    data.m_hostManagerVersion = GetToolsVersion();

    // Extract host Alias
    data.m_hostAlias = m_spHostInfo->GetAlias();

    // Update devices status
    if (!GetDeviceManager().GetDeviceStatus(data.m_devices))
    {
        return false;
    }

    return true;
}

void Host::PublishKeepAliveEvent()
{
    PushEvent(KeepAliveEvent());
}
