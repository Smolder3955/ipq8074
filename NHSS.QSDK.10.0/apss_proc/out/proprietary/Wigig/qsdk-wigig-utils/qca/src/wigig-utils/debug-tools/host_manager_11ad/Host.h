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

#ifndef _HOST_H_
#define _HOST_H_

#include <memory>

class HostInfo;
class ArgumentsParser;
class HostManagerEventBase;
class WebHttpServer;
class DeviceManager;
class UdpServer;
class EventsTcpServer;
class CommandsTcpServer;
class JsonCommandsTcpServer;
class PersistentConfiguration;
class TaskScheduler;
struct HostData;

/*
 * Host is a class that holds the TCP and UDP server.
 * It also holds the Device Manager.
 */
class Host
{
public:

    /*
    * Host is s singleton due to HandleOsSignals - signal is a global function that can't get a pointer to the host as an argument
    */
    static Host& GetHost()
    {
        static Host host;
        return host;
    }

    /*
     * StratHost starts each one of the servers it holds
     */
    void StartHost(bool logCollectorTracer3ppLogsFilterOn, ArgumentsParser& argumentParser);

    /*
     * StopHost stops each one of the servers it holds
     */
    void StopHost();

    HostInfo& GetHostInfo() const { return *m_spHostInfo; }
    bool GetHostUpdate(HostData& data);

    DeviceManager& GetDeviceManager() const  { return *m_deviceManager;  }

    // Push the given event through Events TCP Server
    void PushEvent(const HostManagerEventBase& event) const;

    void SetIsEnumerating(bool isEnumerating) { m_isEnumerating = isEnumerating; }
    bool IsEnumerating() const { return m_isEnumerating; }

    // delete copy Cnstr and assignment operator
    // keep public for better error message
    // no move members will be declared implicitly
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    TaskScheduler& GetTaskScheduler() const { return *m_spTaskScheduler; }
    PersistentConfiguration& GetConfiguration() const { return *m_spPersistentConfiguration; }

private:
    std::unique_ptr<JsonCommandsTcpServer> m_pJsonCommandsTcpServer;
    std::unique_ptr<CommandsTcpServer> m_pCommandsTcpServer;
    std::unique_ptr<EventsTcpServer> m_pEventsTcpServer;
    std::unique_ptr<UdpServer> m_pUdpServer;
    std::unique_ptr<WebHttpServer> m_pHttpServer;

    std::unique_ptr<HostInfo> m_spHostInfo;
    std::unique_ptr<TaskScheduler> m_spTaskScheduler;
    std::unique_ptr<DeviceManager> m_deviceManager; // must be initialized after TaskScheduler
    std::unique_ptr<PersistentConfiguration> m_spPersistentConfiguration;

    unsigned m_keepAliveTaskId;
    bool m_isEnumerating; //for enumeration disable, maybe add it to m_hostInfo

    Host();             // define Cstr to be private, part of Singleton pattern
    ~Host();            // To allow fwd declaration for unique_ptr

    void PublishKeepAliveEvent(); // to enable watchdog on the client side
};


#endif // ! _HOST_H_
