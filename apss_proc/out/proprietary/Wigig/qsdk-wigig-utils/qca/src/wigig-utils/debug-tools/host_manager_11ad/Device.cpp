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

#include <map>

#include "Device.h"
#include "Host.h"
#include "EventsDefinitions.h"
#include "DebugLogger.h"
#include "AccessLayerAPI.h"
#include "DeviceManager.h"
#include "TaskScheduler.h"
#include "FieldDescription.h"

using namespace std;

Device::Device(const string& deviceName) :
    m_basebandType(BASEBAND_TYPE_NONE),
    m_basebandRevision(BB_REV_UNKNOWN),
    m_driver(AccessLayer::OpenDriver(deviceName)),
    m_isSilent(false),
    m_deviceName(deviceName),
    m_capabilitiesMask((DWORD)0)
{
    m_isAlive = ReadDeviceFwInfoInternal(m_fwVersion, m_fwTimestamp);
    if (m_isAlive)
    {
        RegisterDriverControlEvents();
    }
    GetBasebandType();
    if (m_basebandType == BASEBAND_TYPE_TALYN) {
        LOG_INFO <<"On Device "<<deviceName<< ", Baseband is TALYN" << std::endl;
        FW_ERROR_REGISTER = FW_ERROR_REGISTER_TALYN;
        UCODE_ERROR_REGISTER = UCODE_ERROR_REGISTER_TALYN;
    }
    else
    {
        LOG_INFO << "On Device " << deviceName << ", Baseband is SPARROW" << std::endl;
        FW_ERROR_REGISTER = FW_ERROR_REGISTER_SPARROW;
        UCODE_ERROR_REGISTER = UCODE_ERROR_REGISTER_SPARROW;
    }

    m_fwVersionPollerTaskId = Host::GetHost().GetTaskScheduler().RegisterTask(std::bind(&Device::PollFwVersion, this), std::chrono::milliseconds(500), true);
    m_fwUcodeStatePollerTaskId = Host::GetHost().GetTaskScheduler().RegisterTask(std::bind(&Device::PollFwUcodeState, this), std::chrono::milliseconds(500), true);
}

Device::~Device()
{
    Host::GetHost().GetTaskScheduler().UnregisterTask(m_fwVersionPollerTaskId);
    Host::GetHost().GetTaskScheduler().UnregisterTask(m_fwUcodeStatePollerTaskId);
}

bool Device::Init(bool collectLogs, bool recordLogs, log_collector::RecordingType recordingType)
{
    // log collector initialization
    m_fwTracer = unique_ptr<log_collector::LogCollector>(new log_collector::LogCollector(this, log_collector::CPU_TYPE_FW, collectLogs, recordLogs, recordingType));
    m_ucodeTracer = unique_ptr<log_collector::LogCollector>(new log_collector::LogCollector(this, log_collector::CPU_TYPE_UCODE, collectLogs, recordLogs, recordingType));
    return true;
}

bool Device::Fini()
{
    if (Host::GetHost().GetDeviceManager().GetLogRecordingState())
    {
        StopLogRecording();
    }
    m_fwTracer.reset();
    m_ucodeTracer.reset();
    return true;
}

BasebandType Device::GetBasebandType()
{
    if (m_basebandType == BASEBAND_TYPE_NONE)
    {
        m_basebandType = ReadBasebandType();
    }

    return m_basebandType;
}

BasebandRevision Device::GetBasebandRevision()
{
    if (m_basebandRevision == BB_REV_UNKNOWN)
    {
        m_basebandRevision = ReadBasebandRevision();
    }

    return m_basebandRevision;
}

void Device::SetCapability(CAPABILITIES capability, bool isTrue)
{
    const DWORD mask = (DWORD)1 << capability;
    m_capabilitiesMask = isTrue ? m_capabilitiesMask | mask : m_capabilitiesMask & ~mask;
}

bool Device::IsCapabilitySet(CAPABILITIES capability) const
{
    return (m_capabilitiesMask & (DWORD)1 << capability) != (DWORD)0;
}

BasebandType Device::ReadBasebandType() const
{
    DWORD jtagVersion = Utils::REGISTER_DEFAULT_VALUE;
    const int rev_id_address = 0x880B34; //USER.JTAG.USER_USER_JTAG_1.dft_idcode_dev_id
    const int device_id_mask = 0x0fffffff; //take the first 28 bits from the Jtag Id
    BasebandType res = BASEBAND_TYPE_NONE;

    if (!m_driver->Read(rev_id_address, jtagVersion))
    {
        LOG_ERROR << "Failed to read baseband type" << std::endl;
    }

    LOG_DEBUG << "ReadBasebandType = " << Hex<>(jtagVersion) << std::endl;

    switch (jtagVersion & device_id_mask)
    {
    case 0x612072F:
        res = BASEBAND_TYPE_MARLON;
        break;
    case 0x632072F:
        res = BASEBAND_TYPE_SPARROW;
        break;
    case 0x642072F:
    case 0x007E0E1:
        res = BASEBAND_TYPE_TALYN;
        break;
    default:
        ////LOG_MESSAGE_WARN("Invalid device type - assuming Sparrow");
        res = BASEBAND_TYPE_SPARROW;
        break;
    }

    return res;
}

BasebandRevision Device::ReadBasebandRevision() const
{
    DWORD basebandRevision = Utils::REGISTER_DEFAULT_VALUE;
    const int rev_address = 0x880A8C; // FW.g_fw_dedicated_registers.fw_dedicated_registers_s.fw_dedicated_registers_s.rev_id.rev_id_s.rev_id_s.baseband_type
    const int device_rev_mask = 0x0000ffff; // takes only the lower 16 bits
    //BasebandRevision res = UNKNOWN;

    if (!m_driver->Read(rev_address, basebandRevision))
    {
        LOG_ERROR << "Failed to read baseband revision" << std::endl;
    }

    LOG_DEBUG << "ReadBasebandRevision = " << Hex<>(basebandRevision) << std::endl;
    int bbRev = basebandRevision & device_rev_mask;

    if (bbRev >= MAX_BB_REVISION)
    {
        return BB_REV_UNKNOWN;
    }

    return static_cast<BasebandRevision>(bbRev);
}

bool Device::RegisterDriverControlEvents()
{
    bool supportedCommand = GetDriver()->RegisterDriverControlEvents();

    LOG_INFO << "Device " << m_deviceName << " is using " << (supportedCommand ? "Operational" : "Debug") << " mailbox" << endl;

    SetCapability(DRIVER_CONTROL_EVENTS, supportedCommand);
    return supportedCommand;
}

void Device::PollFwUcodeState()
{
    lock_guard<mutex> lock(m_mutex);
    if (GetSilenceMode())
    {
        return;
    }

    FwUcodeState fwUcodeState;

    bool readStatus = ReadDeviceFwUcodeState(fwUcodeState);
    if (readStatus && !(m_fwUcodeState == fwUcodeState))
    {
        m_fwUcodeState = fwUcodeState;
        Host::GetHost().PushEvent(FwUcodeStatusChangedEvent(GetDeviceName(), m_fwUcodeState));
    }
}

void Device::PollFwVersion()
{
    lock_guard<mutex> lock(m_mutex);
    if (GetSilenceMode())
    {
        return;
    }

    FwVersion fwVersion;
    FwTimestamp fwTimestamp;

    m_isAlive = ReadDeviceFwInfoInternal(fwVersion, fwTimestamp);
    if (m_isAlive && !(m_fwVersion == fwVersion && m_fwTimestamp == fwTimestamp))
    {
        m_fwVersion = fwVersion;
        m_fwTimestamp = fwTimestamp;

        Host::GetHost().PushEvent(NewDeviceDiscoveredEvent(GetDeviceName(), m_fwVersion, m_fwTimestamp));
    }
}

bool Device::StartLogCollection()
{
    m_fwTracer->SetCollectionMode(true);
    m_ucodeTracer->SetCollectionMode(true);
    return true;
}

bool Device::StopLogCollection()
{
    m_fwTracer->SetCollectionMode(false);
    m_ucodeTracer->SetCollectionMode(false);
    return true;
}

bool Device::StartLogRecording()
{
    m_fwTracer->SetRecordingFlag(true);
    m_ucodeTracer->SetRecordingFlag(true);
    return true;
}

bool Device::StopLogRecording()
{
    m_fwTracer->SetRecordingFlag(false);
    m_ucodeTracer->SetRecordingFlag(false);
    return true;
}

// Provided mask is assumed to be in range [0,31]
bool Device::AddCustomRegister(const string& name, uint32_t address, uint32_t firstBit, uint32_t lastBit)
{
    if (m_customRegistersMap.find(name) != m_customRegistersMap.end())
    {
        // Register already exists
        return false;
    }

    m_customRegistersMap[name].reset(new FieldDescription(address, firstBit, lastBit));
    return true;
}

bool Device::RemoveCustomRegister(const string& name)
{
    if (m_customRegistersMap.find(name) == m_customRegistersMap.end())
    {
        // Register already does not exist
        return false;
    }

    m_customRegistersMap.erase(name);

    return true;
}

// Read values for all defined custom registers
// Returns true when read operation succeeded for all registers
bool Device::ReadCustomRegisters(std::vector<StringNameValuePair>& customRegisters) const
{
    customRegisters.clear(); // capacity unchanged
    customRegisters.reserve(m_customRegistersMap.size());

    bool ok = true;
    for (const auto& reg : m_customRegistersMap)
    {
        DWORD value = Utils::REGISTER_DEFAULT_VALUE;
        ok &= m_driver->Read(reg.second->GetAddress(), value);
        std::ostringstream oss;
        oss << reg.second->MaskValue(value);

        customRegisters.push_back({reg.first, oss.str()});
    }

    return ok;
}

// Internal service for fetching the FW version and compile time
// Note: The device lock should be acquired by the caller
bool Device::ReadDeviceFwInfoInternal(FwVersion& fwVersion, FwTimestamp& fwTimestamp) const
{
    // FW version
    bool readOk = m_driver->Read(FW_VERSION_MAJOR_REGISTER, fwVersion.m_major);
    readOk &= m_driver->Read(FW_VERSION_MINOR_REGISTER, fwVersion.m_minor);
    readOk &= m_driver->Read(FW_VERSION_SUB_MINOR_REGISTER, fwVersion.m_subMinor);
    readOk &= m_driver->Read(FW_VERSION_BUILD_REGISTER, fwVersion.m_build);

    // FW compile time
    readOk &= m_driver->Read(FW_TIMESTAMP_HOUR_REGISTER, fwTimestamp.m_hour);
    readOk &= m_driver->Read(FW_TIMESTAMP_MINUTE_REGISTER, fwTimestamp.m_min);
    readOk &= m_driver->Read(FW_TIMESTAMP_SECOND_REGISTER, fwTimestamp.m_sec);
    readOk &= m_driver->Read(FW_TIMESTAMP_DAY_REGISTER, fwTimestamp.m_day);
    readOk &= m_driver->Read(FW_TIMESTAMP_MONTH_REGISTER, fwTimestamp.m_month);
    readOk &= m_driver->Read(FW_TIMESTAMP_YEAR_REGISTER, fwTimestamp.m_year);

    if (!readOk)
    {
        LOG_ERROR << "Failed to read FW info for device " << GetDeviceName() << endl;
    }

    return readOk;
}


bool Device::ReadDeviceFwUcodeState(FwUcodeState& fwUcodeState) const
{
    // FW version
    bool readOk = m_driver->Read(FW_ERROR_REGISTER, fwUcodeState.fwError);
    readOk &= m_driver->Read(UCODE_ERROR_REGISTER, fwUcodeState.uCodeError);
    readOk &= m_driver->Read(FW_STATE_REGISTER, fwUcodeState.fwState);
    if (!readOk)
    {
        LOG_ERROR << "Failed to read FW state for device " << GetDeviceName() << endl;
    }

    return readOk;
}
