/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _11AD_PCI_UNIX_DRIVER_EVENTS_HANDLER_H_
#define _11AD_PCI_UNIX_DRIVER_EVENTS_HANDLER_H_

#include "OperatingSystemConstants.h"
#include "OperationStatus.h"
#include <array>
#include <string>
#include <thread>

struct nl_state_t; // forward declaration

// Internal class for operations related to send/receive of Driver Events
class UnixDriverEventsTransport final
{
public:
    UnixDriverEventsTransport();

    ~UnixDriverEventsTransport();

    // Registration for Driver Events through the Operational Mailbox
    // \returns false when Driver does not support this mode or in case of failure
    bool RegisterForDriverEvents(const std::string& shortInterfaceName, const std::string& fullInterfaceName);

    // Workaround, explicit request to enable commands and events transport on the Driver side
    // TODO: remove when new Enumeration mechanism is implemented
    bool ReEnableDriverEventsTransport();

    // Send event to the driver
    bool SendDriverCommand(uint32_t Id, const void *inBuf, uint32_t inBufSize, void *outBuf, uint32_t outBufSize, DWORD* pLastError);

    // Reset FW through generic drive command
    // \remark May fail when:
    // - Working with Debug mailbox
    // - Driver in use does not support NL_60G_GEN_FW_RESET generic command
    // - Not WMI_ONLY FW in use
    OperationStatus TryFwReset();

    // make it non-copyable
    UnixDriverEventsTransport(const UnixDriverEventsTransport& rhs) = delete;
    UnixDriverEventsTransport& operator=(const UnixDriverEventsTransport& rhs) = delete;

private:
    void DriverControlEventsThread(const std::string& fullInterfaceName); // main loop of polling for incoming driver events (blocking, no busy wait)
    void StopDriverControlEventsThread();   // cancel blocking polling for incoming events to allow graceful thread shutdown
    void CloseSocketPair();                 // release sockets from the sockets pair used to stop the driver events thread

    nl_state_t* m_nlState;                  // representation of the netlink state, kind of device descriptor
    std::array<int,2U> m_exitSockets;       // sockets pair to allow cancellation of blocking polling for incoming events
    bool m_eventsTransportRequestNeeded;    // older driver with Operational mailbox support enables events transport by default, newer one only upon an explicit request
    bool m_stopRunningDriverControlEventsThread;
    std::thread m_waitOnSignalThread;       // polling thread
};

#endif // _11AD_PCI_UNIX_DRIVER_EVENTS_HANDLER_H_
