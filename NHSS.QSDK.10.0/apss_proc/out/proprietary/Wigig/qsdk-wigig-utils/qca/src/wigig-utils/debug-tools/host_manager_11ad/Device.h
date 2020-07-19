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

#ifndef _11AD_DEVICE_H_
#define _11AD_DEVICE_H_

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <map>

#include "DriverAPI.h"
#include "HostDefinitions.h"
#include "LogCollector/LogCollector.h"
#include "HostManagerDefinitions.h"

class FieldDescription;

class Device
{
public:
    // Device capabilities enumeration
    // Each value represents a bit in the DWORD (the values would be 0,1,2,3,4...)
    enum CAPABILITIES : DWORD
    {
        DRIVER_CONTROL_EVENTS = 0, // capability of the driver to send and receive driver control commands and events
    };

public:
    explicit Device(const std::string& deviceName);

    virtual ~Device();

    // Functionality common to all devices

    bool Init(bool collectLogs, bool recordLogs, log_collector::RecordingType recordingType);

    bool Fini();

    bool IsValid() const { return m_driver != nullptr; }

    const std::string& GetDeviceName(void) const { return m_deviceName; }

    std::string GetInterfaceName(void) const { return m_driver->GetInterfaceName(); }

    // ************************** [END] Device API **********************//

    BasebandType GetBasebandType();

    BasebandRevision GetBasebandRevision();

    bool GetSilenceMode() const { return m_isSilent; }

    void SetSilenceMode(bool silentMode) { m_isSilent = silentMode;    }

    DriverAPI* GetDriver() const { return m_driver.get(); }

    //TODO: make private after blocking simultaneous driver actions
    mutable std::mutex m_mutex; // threads synchronization

    bool GetIsAlive(void) const { return m_isAlive; }

    const FwVersion& GetFwVersion(void) const { return m_fwVersion; }

    const FwTimestamp& GetFwTimestamp(void) const { return m_fwTimestamp; }

    DWORD GetCapabilities(void) const { return m_capabilitiesMask; }

    bool IsCapabilitySet(CAPABILITIES capability) const;

    // ************************ Log Collector *************************//
    bool StartLogCollection(); // TODO - remove this function after deleting from regular TCP server

    bool StopLogCollection(); // TODO - remove this function after deleting from regular TCP server

    bool StartLogRecording(); // TODO - remove this function after deleting from regular TCP server

    bool StopLogRecording(); // TODO - remove this function after deleting from regular TCP server

    std::shared_ptr<log_collector::LogCollector> m_fwTracer; // TODO : after deleting old log collector commands, change it unique_ptr
    std::shared_ptr<log_collector::LogCollector> m_ucodeTracer; // TODO : after deleting old log collector commands, change it unique_ptr

    // *********************** [END] Log Collector *******************//

    // ************************ Custom Regs *************************//
    bool AddCustomRegister(const std::string& name, uint32_t address, uint32_t firstBit, uint32_t lastBit);

    bool RemoveCustomRegister(const std::string& name);

    // Read values for all defined custom registers
    // Returns true when read operation succeeded for all registers
    bool ReadCustomRegisters(std::vector<StringNameValuePair>& customRegisters) const;
    // *********************** [END] Custom Regs *******************//

private:
    BasebandType m_basebandType;
    BasebandRevision m_basebandRevision;
    std::unique_ptr<DriverAPI> m_driver;
    bool m_isSilent;
    bool m_isAlive;
    std::string m_deviceName;
    FwVersion m_fwVersion;
    FwTimestamp m_fwTimestamp;
    FwUcodeState m_fwUcodeState;

    BasebandType ReadBasebandType() const;
    BasebandRevision ReadBasebandRevision() const;
    // Custom registers
    std::map<std::string, std::unique_ptr<FieldDescription>> m_customRegistersMap;

    bool RegisterDriverControlEvents();

    // Internal service for fetching the FW version and compile time
    bool ReadDeviceFwInfoInternal(FwVersion& fwVersion, FwTimestamp& fwTimestamp) const;
    bool ReadDeviceFwUcodeState(FwUcodeState& fwUcodeState) const;
    void PollFwUcodeState();
    void PollFwVersion();

    enum FwInfoRegisters : DWORD
    {
        FW_VERSION_MAJOR_REGISTER        = 0x880a2c,
        FW_VERSION_MINOR_REGISTER        = 0x880a30,
        FW_VERSION_SUB_MINOR_REGISTER    = 0x880a34,
        FW_VERSION_BUILD_REGISTER        = 0x880a38,
        FW_TIMESTAMP_HOUR_REGISTER        = 0x880a14,
        FW_TIMESTAMP_MINUTE_REGISTER    = 0x880a18,
        FW_TIMESTAMP_SECOND_REGISTER    = 0x880a1c,
        FW_TIMESTAMP_DAY_REGISTER        = 0x880a20,
        FW_TIMESTAMP_MONTH_REGISTER        = 0x880a24,
        FW_TIMESTAMP_YEAR_REGISTER        = 0x880a28,
    };

    enum FwStateRegisters : DWORD
    {
        FW_STATE_REGISTER               = 0x880A44,
        FW_ERROR_REGISTER_SPARROW       = 0x91F020,
        UCODE_ERROR_REGISTER_SPARROW    = 0x91F028,
        FW_ERROR_REGISTER_TALYN         = 0xA37020,
        UCODE_ERROR_REGISTER_TALYN      = 0xA37028,
    };

    DWORD FW_ERROR_REGISTER;
    DWORD UCODE_ERROR_REGISTER;

    // Capabilities region:
    DWORD m_capabilitiesMask;

    void SetCapability(CAPABILITIES capability, bool isTrue);

    unsigned m_fwVersionPollerTaskId;
    unsigned m_fwUcodeStatePollerTaskId;
};

#endif // !_11AD_DEVICE_H_
