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

#include <chrono>
#include <sstream>
#include <array>
#include <mutex>

#include "DeviceManager.h"
#include "Utils.h"
#include "EventsDefinitions.h"
#include "AccessLayerAPI.h"
#include "DebugLogger.h"
#include "Host.h"
#include "HostManagerDefinitions.h"
#include "TaskScheduler.h"
#include "LogCollector/LogCollectorDefinitions.h"
#include "Device.h"

using namespace std;
using namespace log_collector;
namespace
{
#ifdef RTL_SIMULATION
    const int ENUMERATION_INTERVAL_MSEC = 60000;
#elif _WIGIG_ARCH_SPI
    const int ENUMERATION_INTERVAL_MSEC = 5000;
#else
    const int ENUMERATION_INTERVAL_MSEC = 500;
#endif
}

// Initialize translation maps for the front-end data
const static std::string sc_noRfStr("NO_RF");
const std::unordered_map<int, std::string> DeviceManager::m_rfTypeToString = { {0, sc_noRfStr }, {1, "MARLON"}, {2, "SPR-R"}, {3, "TLN-A1"}, { 4, "TLN-A2" } };

const static int sc_TalynMaBasebandType = 8;
const std::unordered_map<int, std::string> DeviceManager::m_basebandTypeToString =
{
    { 0, "UNKNOWN" }, { 1, "MAR-DB1" }, { 2, "MAR-DB2" },
    { 3, "SPR-A0"  }, { 4, "SPR-A1"  }, { 5, "SPR-B0"  },
    { 6, "SPR-C0"  }, { 7, "SPR-D0"  }, { sc_TalynMaBasebandType, "TLN-M-A0"  },
    { 9, "TLN-M-B0" }
};

DeviceManager::DeviceManager() :
    m_EnumarationPollingIntervalMs(ENUMERATION_INTERVAL_MSEC),
    m_terminate(false),
    m_collectLogs(false),
    m_recordLogs(false)
{
    // Force first enumeration to have it completed before host starts responding to requests
    // Make sure it does NOT access methods of the class to prevent unexpected termination
    UpdateConnectedDevices();

    m_enumarationTaskId = Host::GetHost().GetTaskScheduler().RegisterTask(
        std::bind(&DeviceManager::UpdateConnectedDevices, this), std::chrono::milliseconds(m_EnumarationPollingIntervalMs), true);
}

DeviceManager::~DeviceManager()
{
    Host::GetHost().GetTaskScheduler().UnregisterTask(m_enumarationTaskId);
}

string DeviceManager::GetDeviceManagerOperationStatusString(DeviceManagerOperationStatus status)
{
    switch (status)
    {
    case dmosSuccess:
        return "Successful operation";
    case dmosNoSuchConnectedDevice:
        return "Unknown device";
    case dmosFailedToReadFromDevice:
        return "Read failure";
    case dmosFailedToWriteToDevice:
        return "Write failure";
    case dmosFailedToResetInterface:
        return "Reset interface failure";
    case dmosFailedToResetSw:
        return "SW reset failure";
    case dmosFailedToAllocatePmc:
        return "Allocate PMC failure";
    case dmosFailedToDeallocatePmc:
        return "Deallocate PMC failure";
    case dmosFailedToCreatePmcFile:
        return "Create PMC file failure";
    case dmosFailedToSendWmi:
        return "Send WMI failure";
    case dmosFail:
        return "Operation failure";
    case dmosSilentDevice:
        return "Device is in silent mode";
    case dmosDriverCommandNotSupported:
        return "Driver Commands are not supported for Debug Mailbox";
    default:
        return "DeviceManagerOperationStatus is unknown ";
    }
}

DeviceManagerOperationStatus DeviceManager::GetDevices(set<string>& devicesNames)
{
    devicesNames.clear();
    m_connectedDevicesMutex.lock();
    for (const auto& device : m_devices)
    {
        devicesNames.insert(device.first);
    }
    m_connectedDevicesMutex.unlock();
    return dmosSuccess;
}

std::shared_ptr<Device> DeviceManager::GetDeviceByName(const std::string& deviceName)
{
    m_connectedDevicesMutex.lock();
    for (const auto& device : m_devices)
    {
        if (deviceName == device.first)
        {
            m_connectedDevicesMutex.unlock();
            return device.second;
        }
    }
    m_connectedDevicesMutex.unlock();
    return nullptr;
}

DeviceManagerOperationStatus DeviceManager::Read(const string& deviceName, DWORD address, DWORD& value)
{
    if (IsDeviceSilent(deviceName))
    {
        return dmosSilentDevice;
    }

    if ((0 == address) || (0 != address % 4) || (0xFFFFFFFF == address))
    {
        return dmosInvalidAddress;
    }

    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->Read(address, value);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            value = Utils::REGISTER_DEFAULT_VALUE;
            status = dmosFailedToReadFromDevice;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        value = Utils::REGISTER_DEFAULT_VALUE;
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::Write(const string& deviceName, DWORD address, DWORD value)
{
    if (IsDeviceSilent(deviceName))
    {
        return dmosSilentDevice;
    }

    if ((0 == address) || (0 != address % 4) || (0xFFFFFFFF == address))
    {
        return dmosInvalidAddress;
    }

    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->Write(address, value);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            status = dmosFailedToWriteToDevice;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::ReadBlock(const string& deviceName, DWORD address, DWORD blockSize, vector<DWORD>& values)
{
    if (IsDeviceSilent(deviceName))
    {
        return dmosSilentDevice;
    }

    if ((0 == address) || (0 != address % 4) || (0xFFFFFFFF == address))
    {
        return dmosInvalidAddress;
    }

    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->ReadBlock(address, blockSize, values);
        m_devices[deviceName]->m_mutex.unlock();
        return success ? dmosSuccess : dmosFailedToReadFromDevice;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::ReadRadarData(const std::string& deviceName, DWORD maxBlockSize, vector<DWORD>& values)
{
    if (IsDeviceSilent(deviceName))
    {
        return dmosSilentDevice;
    }

    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->ReadRadarData(maxBlockSize, values);
        m_devices[deviceName]->m_mutex.unlock();
        return success ? dmosSuccess : dmosFailedToReadFromDevice;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::WriteBlock(const string& deviceName, DWORD address, const vector<DWORD>& values)
{
    if (IsDeviceSilent(deviceName))
    {
        return dmosSilentDevice;
    }

    if ((0 == address) || (0 != address % 4) || (0xFFFFFFFF == address))
    {
        return dmosInvalidAddress;
    }

    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->WriteBlock(address, values);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            status = dmosFailedToWriteToDevice;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

OperationStatus DeviceManager::InterfaceReset(const string& deviceName)
{
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        std::lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        return m_devices[deviceName]->GetDriver()->InterfaceReset();
    }

    m_connectedDevicesMutex.unlock();
    return OperationStatus(false, "Unknown device");
}

DeviceManagerOperationStatus DeviceManager::SetDriverMode(const string& deviceName, int newMode, int& oldMode)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->SetDriverMode(newMode, oldMode);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            status = dmosFailedToResetSw;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::DriverControl(const string& deviceName, uint32_t Id, const void *inBuf, uint32_t inBufSize, void *outBuf, uint32_t outBufSize)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        std::lock_guard<mutex> lock((m_devices[deviceName]->m_mutex));
        m_connectedDevicesMutex.unlock();

        auto device = m_devices[deviceName];
        if (!device->IsCapabilitySet(Device::DRIVER_CONTROL_EVENTS))
        {
            LOG_ERROR << "Unsupported Driver Command for device " << deviceName << " working with Debug Mailbox" << std::endl;
            return dmosDriverCommandNotSupported;
        }

        bool success = device->GetDriver()->DriverControl(Id, inBuf, inBufSize, outBuf, outBufSize);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            status = dmosFailedToReadFromDevice;
        }
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::AllocPmc(const string& deviceName, unsigned descSize, unsigned descNum, string& errorMsg)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();

    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();

        bool success = m_devices[deviceName]->GetDriver()->AllocPmc(descSize, descNum, errorMsg);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            LOG_ERROR << "Failed to allocate PMC ring: " << errorMsg << std::endl;
            errorMsg = "Failed to allocate PMC ring" + errorMsg;
            status = dmosFailedToAllocatePmc;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        errorMsg = "No device found";
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::DeallocPmc(const string& deviceName, bool bSuppressError, std::string& outMessage)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();

        bool success = m_devices[deviceName]->GetDriver()->DeallocPmc(bSuppressError, outMessage);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            LOG_ERROR << "Failed to de-allocate PMC ring: " << outMessage << std::endl;
            outMessage += " - Failed to de-allocate PMC ring";
            status = dmosFailedToDeallocatePmc;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::CreatePmcFiles(const string& deviceName, unsigned refNumber, std::string& outMessage)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->CreatePmcFiles(refNumber, outMessage);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            status = dmosFailedToCreatePmcFile;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::FindPmcDataFile(const string& deviceName, unsigned refNumber, std::string& outMessage)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->FindPmcDataFile(refNumber, outMessage);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            status = dmosFailedToCreatePmcFile;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

DeviceManagerOperationStatus DeviceManager::FindPmcRingFile(const string& deviceName, unsigned refNumber, std::string& outMessage)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        m_devices[deviceName]->m_mutex.lock();
        m_connectedDevicesMutex.unlock();
        bool success = m_devices[deviceName]->GetDriver()->FindPmcRingFile(refNumber, outMessage);
        if (success)
        {
            status = dmosSuccess;
        }
        else
        {
            status = dmosFailedToCreatePmcFile;
        }
        m_devices[deviceName]->m_mutex.unlock();
        return status;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        return dmosNoSuchConnectedDevice;
    }
}

void DeviceManager::CreateDevice(const string& deviceName)
{
    m_connectedDevicesMutex.lock();
    shared_ptr<Device> device(new Device(deviceName));
    device->Init(m_publishLogs, m_recordLogs, m_recordingType);
    m_devices.insert(make_pair(deviceName, device));
    m_connectedDevicesMutex.unlock();

    if (device->GetIsAlive())
    {
        // Notify that new device discovered, also relevant for case of FW update
        Host::GetHost().PushEvent(NewDeviceDiscoveredEvent(deviceName, device->GetFwVersion(), device->GetFwTimestamp()));
    }
    else
    {
        // Delete unresponsive device, lock is acquired inside
        // Note: Can happen if device becomes unresponsive after enumeration
        DeleteDevice(deviceName);
        LOG_INFO << "Created unresponsive device '" << deviceName << "', removing..." << endl;
    }
}

void DeviceManager::DeleteDevice(const string& deviceName)
{
    m_connectedDevicesMutex.lock();
    // make sure that no client is using this object
    m_devices[deviceName]->m_mutex.lock();
    m_devices[deviceName]->Fini();
    // no need that the mutex will be still locked since new clients have to get m_connectedDevicesMutex before they try to get m_mutex
    m_devices[deviceName]->m_mutex.unlock();
    m_devices.erase(deviceName);
    m_connectedDevicesMutex.unlock();
}

void DeviceManager::UpdateConnectedDevices()
{
    if (!Host::GetHost().IsEnumerating())
    {
        return;
    }

    vector<string> devicesForRemove;
    // Delete unresponsive devices
    m_connectedDevicesMutex.lock();
    for (auto& connectedDevice : m_devices)
    {
        if (connectedDevice.second->GetSilenceMode()) //GetSilenceMode returns true if the device is silent the skip th update
        {
            continue;
        }

        connectedDevice.second->m_mutex.lock();
        if (!connectedDevice.second->GetDriver()->IsValid())
        {
            devicesForRemove.push_back(connectedDevice.first);
        }
        connectedDevice.second->m_mutex.unlock();
    }
    m_connectedDevicesMutex.unlock();

    for (auto& device : devicesForRemove)
    {
        LOG_INFO << __FUNCTION__ << ": deleting invalid device " << device << std::endl;
        DeleteDevice(device);
    }

    devicesForRemove.clear();

    set<string> currentlyConnectedDevices = AccessLayer::GetDrivers();

    // delete devices that aren't connected anymore according to enumeration
    for (auto& connectedDevice : m_devices)
    {
        if (connectedDevice.second->GetSilenceMode()) //GetSilenceMode returns true if the device is silent the skip th update
        {
            continue;
        }

        if (0 == currentlyConnectedDevices.count(connectedDevice.first))
        {
            devicesForRemove.push_back(connectedDevice.first);
        }
    }
    for (auto& device : devicesForRemove)
    {
        LOG_INFO << __FUNCTION__ << ": device " << device << " not found during enumeration, deleting..." << std::endl;
        DeleteDevice(device);
    }

    // add new connected devices
    vector<string> newDevices;
    m_connectedDevicesMutex.lock();
    for (auto& currentlyConnectedDevice : currentlyConnectedDevices)
    {
        if (0 == m_devices.count(currentlyConnectedDevice))
        {
            newDevices.push_back(currentlyConnectedDevice);
        }
    }
    m_connectedDevicesMutex.unlock();

    for (auto& device : newDevices)
    {
        CreateDevice(device);
    }
}

bool DeviceManager::IsDeviceSilent(const string& deviceName)
{
    bool isSilent = false;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) <= 0)
    {
        m_connectedDevicesMutex.unlock();
        return isSilent;
    }

    if (!(m_devices[deviceName] && m_devices[deviceName]->IsValid()))
    {
        LOG_ERROR << "Invalid device pointer in IsDeviceSilent (NULL)" << endl;
        m_connectedDevicesMutex.unlock();
        return isSilent;
    }

    m_devices[deviceName]->m_mutex.lock();
    m_connectedDevicesMutex.unlock();

    isSilent = m_devices[deviceName]->GetSilenceMode();

    m_devices[deviceName]->m_mutex.unlock();

    return isSilent;
}

bool DeviceManager::GetDeviceStatus(vector<DeviceData>& devicesData)
{
    // generic board file id and corresponding name, other ids are displayed as is
    const static std::pair<DWORD, std::string> sc_genericBoardFileDescription = std::pair<DWORD, std::string>(65541, "Falcon Free Space"); // 0x10005
    const static std::array<std::string, 5U> sc_fwType = { { "Operational", "WMI Only", "No PCIe", "WMI Only - No PCIe", "IF2IF" } };

    // Lock the devices
    lock_guard<mutex> lock(m_connectedDevicesMutex);

    auto devices = m_devices;

    for (auto& device : devices)
    {
        // Lock the specific device
        lock_guard<mutex> loopLock(device.second->m_mutex);

        // Create device data
        DeviceData deviceData;

        // Extract FW version
        deviceData.m_fwVersion = device.second->GetFwVersion();

        DWORD value = Utils::REGISTER_DEFAULT_VALUE;

        // Read FW assert code
        device.second->GetDriver()->Read(FW_ASSERT_REG, value);
        deviceData.m_fwAssert = value;

        // Read uCode assert code
        device.second->GetDriver()->Read(UCODE_ASSERT_REG, value);
        deviceData.m_uCodeAssert = value;

        // Read FW association state
        device.second->GetDriver()->Read(FW_ASSOCIATION_REG, value);
        deviceData.m_associated = (value == FW_ASSOCIATED_VALUE);

        // Get FW compilation timestamp
        deviceData.m_compilationTime = device.second->GetFwTimestamp();

        // Get Device name
        deviceData.m_deviceName = device.second->GetDeviceName();

        // Get baseband name & RF type
        // BB type is stored in 2 lower bytes of device type register
        // RF type is stored in 2 upper bytes of device type register
        device.second->GetDriver()->Read(DEVICE_TYPE_REG, value);
        const int basebandTypeValue = value & 0xFFFF;
        const auto basebandTypeIter = m_basebandTypeToString.find(basebandTypeValue);
        deviceData.m_hwType = basebandTypeIter != m_basebandTypeToString.cend() ? basebandTypeIter->second : std::string("UNKNOWN");
        const auto rfTypeIter = m_rfTypeToString.find((value & 0xFFFF0000) >> 16);
        deviceData.m_rfType = rfTypeIter != m_rfTypeToString.cend() ? rfTypeIter->second : sc_noRfStr;

        // Read MCS value and set signal strength (maximum is 5 bars)
        const int maxMcsRange = (basebandTypeValue < sc_TalynMaBasebandType) ? MAX_MCS_RANGE_SPARROW : MAX_MCS_RANGE_TALYN;
        device.second->GetDriver()->Read(MCS_REG, value);
        deviceData.m_mcs = value;
        deviceData.m_signalStrength = static_cast<int>(value * 5.0 / maxMcsRange + 0.5);

        // Get FW mode
        device.second->GetDriver()->Read(FW_MODE_REG, value);
        deviceData.m_mode = value < sc_fwType.size() ? sc_fwType[value] : "NA";

        // Get boot loader version
        device.second->GetDriver()->Read(BOOT_LOADER_VERSION_REG, value);
        std::ostringstream oss;
        oss << value;
        deviceData.m_bootloaderVersion = oss.str();

        // Get channel number
        int Channel = 0;
        if (basebandTypeValue < sc_TalynMaBasebandType)
        {
            device.second->GetDriver()->Read(CHANNEL_REG_PRE_TALYN_MA, value);
            switch (value)
            {
            case 0x64FCACE:
                Channel = 1;
                break;
            case 0x68BA2E9:
                Channel = 2;
                break;
            case 0x6C77B03:
                Channel = 3;
                break;
            default:
                Channel = 0;
            }
        }
        else
        {
            device.second->GetDriver()->Read(CHANNEL_REG_POST_TALYN_MA, value);
            value = (value & 0xF0000) >> 16; // channel value is contained in bits [16-19]
            // the following is an essence of translation from channel value to the channel number
            Channel = value <= 5 ? value + 1 : value + 3;
        }
        deviceData.m_channel = Channel;

        // Get board file version
        device.second->GetDriver()->Read(BOARDFILE_REG, value);
        if (((value & 0xFFFFF000) >> 12) == sc_genericBoardFileDescription.first) // bits [12-31] are 0x10005
        {
            deviceData.m_boardFile = sc_genericBoardFileDescription.second;
        }
        else
        {
            oss.str(std::string());
            oss << value;
            deviceData.m_boardFile = oss.str();
        }

        // Lower byte of RF state contains connection bit-mask
        // Second byte of RF state contains enabled bit-mask
        DWORD rfConnected = 0;
        device.second->GetDriver()->Read(RF_STATE_REG, rfConnected);

        // Note: RF state address is not constant.
        //       As workaround for SPR-B0 is to mark its single RF as enabled (when RF present)
        // TODO: Fix after moving the RF state to a constant address
        if (deviceData.m_hwType == "SPR-B0" && deviceData.m_rfType != sc_noRfStr)
        {
            rfConnected = 0x101; // connected & enabled
        }

        DWORD rfEnabled = rfConnected >> 8;

        // Get RF state of each RF
        deviceData.m_rf.reserve(MAX_RF_NUMBER);
        for (int rfIndex = 0; rfIndex < MAX_RF_NUMBER; ++rfIndex)
        {
            int rfState = 0; // disabled

            if (rfConnected & (1 << rfIndex))
            {
                rfState = 1; // connected
                if (rfEnabled & (1 << rfIndex))
                {
                    rfState = 2; // enabled
                }
            }

            deviceData.m_rf.push_back(rfState);
        }

        ////////// Get fixed registers values //////////////////////////
        StringNameValuePair registerData;

        // uCode Rx on fixed reg
        device.second->GetDriver()->Read(UCODE_RX_ON_REG, value);
        DWORD UcRxonhexVal16 = value & 0xFFFF;
        string UcRxon;
        switch (UcRxonhexVal16)
        {
        case 0:
            UcRxon = "RX_OFF";
            break;
        case 1:
            UcRxon = "RX_ONLY";
            break;
        case 2:
            UcRxon = "RX_ON";
            break;
        default:
            UcRxon = "Unrecognized";
        }
        registerData.m_name = "uCodeRxOn";
        registerData.m_value = UcRxon;
        deviceData.m_fixedRegisters.insert(deviceData.m_fixedRegisters.end(), registerData);

        // BF Sequence fixed reg
        device.second->GetDriver()->Read(BF_SEQ_REG, value);
        oss.str(std::string());
        oss << value;
        registerData.m_name = "BF_Seq";
        registerData.m_value = oss.str();
        deviceData.m_fixedRegisters.insert(deviceData.m_fixedRegisters.end(), registerData);

        // BF Trigger fixed reg
        device.second->GetDriver()->Read(BF_TRIG_REG, value);
        string BF_TRIG = "";
        switch (value)
        {
        case 1:
            BF_TRIG = "MCS1_TH_FAILURE";
            break;
        case 2:
            BF_TRIG = "MCS1_NO_BACK";
            break;
        case 4:
            BF_TRIG = "NO_CTS_IN_TXOP";
            break;
        case 8:
            BF_TRIG = "MAX_BCK_FAIL_TXOP";
            break;
        case 16:
            BF_TRIG = "FW_TRIGGER ";
            break;
        case 32:
            BF_TRIG = "MAX_BCK_FAIL_ION_KEEP_ALIVE";
            break;
        default:
            BF_TRIG = "UNDEFINED";
        }
        registerData.m_name = "BF_Trig";
        registerData.m_value = BF_TRIG;
        deviceData.m_fixedRegisters.insert(deviceData.m_fixedRegisters.end(), registerData);

        // Get NAV fixed reg
        device.second->GetDriver()->Read(NAV_REG, value);
        registerData.m_name = "NAV";
        oss.str(std::string());
        oss << value;
        registerData.m_value = oss.str();
        deviceData.m_fixedRegisters.insert(deviceData.m_fixedRegisters.end(), registerData);

        // Get TX Goodput fixed reg
        device.second->GetDriver()->Read(TX_GP_REG, value);
        string TX_GP = "NO_LINK";
        if (value != 0)
        {
            oss.str(std::string());
            oss << value;
            TX_GP = oss.str();
        }
        registerData.m_name = "TX_GP";
        registerData.m_value = TX_GP;
        deviceData.m_fixedRegisters.insert(deviceData.m_fixedRegisters.end(), registerData);

        // Get RX Goodput fixed reg
        device.second->GetDriver()->Read(RX_GP_REG, value);
        string RX_GP = "NO_LINK";
        if (value != 0)
        {
            oss.str(std::string());
            oss << value;
            RX_GP = oss.str();
        }
        registerData.m_name = "RX_GP";
        registerData.m_value = RX_GP;
        deviceData.m_fixedRegisters.insert(deviceData.m_fixedRegisters.end(), registerData);
        ////////////// Fixed registers end /////////////////////////

        ////////////// Custom registers ////////////////////////////
        device.second->ReadCustomRegisters(deviceData.m_customRegisters);
        ////////////// Custom registers end ////////////////////////

        ////////////// Temperatures ////////////////////////////////
        // Baseband
        device.second->GetDriver()->Read(BASEBAND_TEMPERATURE_REG, value);
        float temperature = (float)value / 1000;
        oss.str(std::string());
        oss.precision(2);
        oss << fixed << temperature;
        deviceData.m_hwTemp = oss.str();

        // RF
        if (deviceData.m_rfType != sc_noRfStr)
        {
            device.second->GetDriver()->Read(RF_TEMPERATURE_REG, value);
            temperature = (float)value / 1000;
            oss.str(std::string());
            oss.precision(2);
            oss << fixed << temperature;
            deviceData.m_rfTemp = oss.str();
        }
        else // no RF, temperature value is not relevant
        {
            deviceData.m_rfTemp = "";
        }
        ////////////// Temperatures end ///////////////////////////

        // AddChildNode the device to the devices list
        devicesData.insert(devicesData.end(), deviceData);
    }

    return true;
}

bool DeviceManager::AddRegister(const string& deviceName, const string& registerName, uint32_t address, uint32_t firstBit, uint32_t lastBit, std::string& failureReason)
{
    lock_guard<mutex> lock(m_connectedDevicesMutex);

    if (m_devices.count(deviceName) <= 0)
    {
        return false;
    }

    if (firstBit > 31 || lastBit > 31 || firstBit > lastBit)
    {
        LOG_ERROR << "Trying to add custom register with invalid mask" << endl;
        failureReason = "Invalid mask provided. Bits should be in range [0,31].";
        return false;
    }

    if (!(m_devices[deviceName] && m_devices[deviceName]->AddCustomRegister(registerName, address, firstBit, lastBit)))
    {
        LOG_ERROR << "Trying to add an already existing custom register name" << endl;
        failureReason = "Custom register with this name already exists.";
        return false;
    }

    return true;
}

bool DeviceManager::RemoveRegister(const string& deviceName, const string& registerName, std::string& failureReason)
{
    lock_guard<mutex> lock(m_connectedDevicesMutex);

    if (m_devices.count(deviceName) <= 0)
    {
        return false;
    }

    if (!(m_devices[deviceName] && m_devices[deviceName]->RemoveCustomRegister(registerName)))
    {
        LOG_ERROR << "Trying to remove a non-existing custom register name" << endl;
        failureReason = "Custom register with this name does not exist.";
        return false;
    }

    return true;
}

DeviceManagerOperationStatus DeviceManager::SetDeviceSilentMode(const string& deviceName, bool silentMode)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        m_devices[deviceName]->SetSilenceMode(silentMode);
        status = dmosSuccess;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        status = dmosNoSuchConnectedDevice;
    }

    return status;
}


DeviceManagerOperationStatus DeviceManager::GetDeviceSilentMode(const string& deviceName, bool& silentMode)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        silentMode = m_devices[deviceName]->GetSilenceMode();
        status = dmosSuccess;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        status = dmosNoSuchConnectedDevice;
    }

    return status;
}


/*************************** Log functions *********************************/
OperationStatus DeviceManager::SetLogRecordingMode(bool recordLogs, const string& deviceName, CpuType cpuType, log_collector::RecordingType recordingType)
{
    OperationStatus os;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        switch (cpuType)
        {
        case log_collector::CPU_TYPE_FW:
            os = m_devices[deviceName]->m_fwTracer->SetRecordingMode(recordLogs, recordingType);
            break;
        case log_collector::CPU_TYPE_UCODE:
            os = m_devices[deviceName]->m_ucodeTracer->SetRecordingMode(recordLogs, recordingType);
            break;
        default:
            os = OperationStatus(false, "No such a cpu type");
            break;
        }
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        os = OperationStatus(false, "Device does not exist");
    }
    return os;
}

OperationStatus DeviceManager::StartStopRecording(const string& deviceName, CpuType cpuType, bool toStart, log_collector::RecordingType recordingType)
{
    OperationStatus os;
    bool isRec = false;
    os = IsRecordingActiveForDevice(deviceName, isRec);
    if (!os)
    {
        return os;
    }
    if (toStart && isRec)
    {
        return OperationStatus(false, "Recording is already active for: " + deviceName + ". Stop recordings and try again.");
    }
    if (log_collector::CPU_TYPE_BOTH == cpuType)
    {
        OperationStatus os1 = SetLogRecordingMode(toStart, deviceName, log_collector::CPU_TYPE_FW, recordingType);
        OperationStatus os2 = SetLogRecordingMode(toStart, deviceName, log_collector::CPU_TYPE_UCODE, recordingType);

        stringstream ssCombinedMessage;
        ssCombinedMessage << "For FW: " << os1.GetStatusMessage() << ". For uCode: " << os2.GetStatusMessage() <<".";
        const bool osRes = os1.IsSuccess() && os2.IsSuccess();
        os = OperationStatus(osRes, ssCombinedMessage.str());
    }
    else
    {
        os = SetLogRecordingMode(toStart, deviceName, cpuType, recordingType);
    }

    return os;
}

OperationStatus DeviceManager::GetDefaultDevice(std::string & outDeviceName)
{
    OperationStatus os;
    lock_guard<mutex> lock(m_connectedDevicesMutex);
    if (m_devices.empty())
    {
        return OperationStatus(false, "No device exist");
    }
    if (m_devices.size() > 1)
    {
        return OperationStatus(false, "No default device. Please provide device name");
    }
    auto defaultDevice = m_devices.begin()->second;
    outDeviceName = defaultDevice->GetDeviceName();
    return OperationStatus(true);
}

OperationStatus DeviceManager::IsRecordingActiveForDevice(const std::string& deviceName, bool& isRecording)
{
    bool isRecordingFw, isRecordingUcode;
    OperationStatus os = GetLogRecordingState(deviceName, CPU_TYPE_FW, isRecordingFw);
    if(!os)
    {
        isRecording = false;
        return os;
    }
    os = GetLogRecordingState(deviceName, CPU_TYPE_UCODE, isRecordingUcode);
    if (!os)
    {
        isRecording = false;
        return os;
    }
    isRecording = isRecordingFw || isRecordingUcode;
    return OperationStatus();
}

OperationStatus DeviceManager::GetLogRecordingState(const std::string& deviceName, CpuType cpuType, bool& isRecording)
{
    isRecording = false;
    OperationStatus os = OperationStatus(true);
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        switch (cpuType)
        {
        case CPU_TYPE_FW:
            isRecording = m_devices[deviceName]->m_fwTracer->IsRecording();
            break;
        case CPU_TYPE_UCODE:
            isRecording = m_devices[deviceName]->m_ucodeTracer->IsRecording();
            break;
        default:
            os = OperationStatus(false, "No such a cpu type");
            break;
        }
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        os = OperationStatus(false, "Device does not exist");
    }
    return os;
}

OperationStatus DeviceManager::SetLogPublishingMode(bool publishLogs, const std::string& deviceName, CpuType cpuType)
{
    OperationStatus os;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        switch (cpuType)
        {
        case CPU_TYPE_FW:
            os = m_devices[deviceName]->m_fwTracer->SetPublishingMode(publishLogs);
            break;
        case CPU_TYPE_UCODE:
            os = m_devices[deviceName]->m_ucodeTracer->SetPublishingMode(publishLogs);
            break;
        default:
            os = OperationStatus(false, "No such a cpu type");
            break;
        }
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        os = OperationStatus(false, "Device does not exist");
    }
    return os;
}

OperationStatus DeviceManager::GetLogPublishingState(const std::string& deviceName, CpuType cpuType, bool& isPublishing)
{
    isPublishing = false;
    OperationStatus os = OperationStatus(true);
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        switch (cpuType)
        {
        case CPU_TYPE_FW:
            isPublishing = m_devices[deviceName]->m_fwTracer->IsPublishing();
            break;
        case CPU_TYPE_UCODE:
            isPublishing = m_devices[deviceName]->m_ucodeTracer->IsPublishing();
            break;
        default:
            os = OperationStatus(false, "No such a cpu type");
            break;
        }
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        os = OperationStatus(false, "Device does not exist");
    }
    return os;
}

OperationStatus DeviceManager::SetLogRecordingDeferredMode(bool recordLogs, RecordingType recordingType)
{
    if (recordLogs && (recordingType == RECORDING_TYPE_TXT || recordingType == RECORDING_TYPE_BOTH))
    {
        // file validation in default location:
        string conversionFileDirectory = FileSystemOsAbstraction::GetDefaultLogConversionFilesLocation();
        const bool validConversionFile = FileSystemOsAbstraction::DoesFileExist(conversionFileDirectory + "fw_image_trace_string_load.bin") &&
            FileSystemOsAbstraction::DoesFileExist(conversionFileDirectory + "ucode_image_trace_string_load.bin");
        if (!validConversionFile)
        {
            return OperationStatus(false, "conversion file not found at: " + conversionFileDirectory );
        }
    }
    m_recordLogs = recordLogs;
    m_recordingType = recordingType;
    return OperationStatus();

}

void DeviceManager::SetLogPublishingDeferredMode(bool publishLogs)
{
    m_publishLogs = publishLogs;
}

OperationStatus DeviceManager::SetLogCollectionModuleVerbosity(const std::string& deviceName, CpuType cpuType,
                                                               const std::string& module, const std::string& level)
{
    OperationStatus os = OperationStatus(true);
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        switch (cpuType)
        {
        case CPU_TYPE_FW:
            os = m_devices[deviceName]->m_fwTracer->SetModuleVerbosity(module, level);
            break;
        case CPU_TYPE_UCODE:
            os = m_devices[deviceName]->m_ucodeTracer->SetModuleVerbosity(module, level);
            break;
        default:
            os = OperationStatus(false, "No such a cpu type");
            break;
        }
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        os = OperationStatus(false, "Device does not exist");
    }
    return os;
}

OperationStatus DeviceManager::GetLogCollectionModuleVerbosity(const string& deviceName,
                                                               CpuType cpuType, const string& module, string& value)
{
    OperationStatus os = OperationStatus(true);
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        switch (cpuType)
        {
        case CPU_TYPE_FW:
            value = m_devices[deviceName]->m_fwTracer->GetModuleVerbosity(module);
            break;
        case CPU_TYPE_UCODE:
            value = m_devices[deviceName]->m_ucodeTracer->GetModuleVerbosity(module);
            break;
        default:
            os = OperationStatus(false, "No such a cpu type");
            break;
        }
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        os = OperationStatus(false, "Device does not exist");
    }
    return os;
}

OperationStatus DeviceManager::GetLogCollectorConfiguration(
    const std::string& deviceName, CpuType cpuType,
    log_collector::LogCollectorConfiguration& configuration)
{
    OperationStatus os = OperationStatus(true);
    std::string outDeviceName = deviceName;
    if (deviceName.empty())
    {
        os = GetDefaultDevice(outDeviceName);
        if (!os)
        {
            return os;
        }
    }

    m_connectedDevicesMutex.lock();
    if (m_devices.count(outDeviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[outDeviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        switch (cpuType)
        {
        case log_collector::CPU_TYPE_FW:
            configuration = m_devices[outDeviceName]->m_fwTracer->GetConfiguration();
            break;
        case log_collector::CPU_TYPE_UCODE:
            configuration = m_devices[outDeviceName]->m_ucodeTracer->GetConfiguration();
            break;
        default:
            os = OperationStatus(false, "No such a cpu type");
            break;
        }
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        os = OperationStatus(false, "Device does not exist");
    }
    return os;
}

OperationStatus DeviceManager::SubmitLogCollectorConfiguration(
    const std::string& deviceName, CpuType cpuType,
    const log_collector::LogCollectorConfiguration& newConfiguration)
{
    OperationStatus os = OperationStatus(true);
    std::string outDeviceName = deviceName;
    if (deviceName.empty())
    {
    // We try to get the default device if the explicit device name is not provided
        os = GetDefaultDevice(outDeviceName);
        if (!os)
        {
            return os;
        }
    }
    m_connectedDevicesMutex.lock();
    if (m_devices.count(outDeviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[outDeviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        switch (cpuType)
        {
        case CPU_TYPE_FW:
            os = m_devices[outDeviceName]->m_fwTracer->SubmitConfiguration(newConfiguration);
            break;
        case CPU_TYPE_UCODE:
            os = m_devices[outDeviceName]->m_ucodeTracer->SubmitConfiguration(newConfiguration);
            break;
        default:
            os = OperationStatus(false, "No such a cpu type");
            break;
        }
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        os = OperationStatus(false, "Device does not exist");
    }
    return os;
}

//  ***************  TODO : For the regular tcp server. should be removed in the future. **********************
void DeviceManager::SetLogRecording(bool recordLogs)
{
    m_recordLogs = recordLogs;

    // Start/Stop log collector for all devices
    m_connectedDevicesMutex.lock();
    for (const auto& connectedDevice : m_devices)
    {
        connectedDevice.second->m_mutex.lock();

        if (recordLogs == false)
        {
            connectedDevice.second->StopLogRecording();
        }
        else
        {
            connectedDevice.second->StartLogRecording();
        }

        connectedDevice.second->m_mutex.unlock();
    }
    m_connectedDevicesMutex.unlock();
}

//  *************** TODO : For the regular tcp server. should be removed in the future. **********************
void DeviceManager::SetLogCollectionMode(bool collect)
{
    m_collectLogs = collect;

    // Start/Stop log collector for all devices
    m_connectedDevicesMutex.lock();
    for (const auto& connectedDevice : m_devices)
    {
        connectedDevice.second->m_mutex.lock();

        if (collect == false)
        {
            connectedDevice.second->StopLogCollection();
        }
        else
        {
            connectedDevice.second->StartLogCollection();
        }

        connectedDevice.second->m_mutex.unlock();
    }
    m_connectedDevicesMutex.unlock();
}

//  ***************  TODO : For the regular tcp server. should be removed in the future. **********************
bool DeviceManager::GetLogCollectionMode() const
{
    return m_collectLogs;
}

//  ***************  TODO : For the regular tcp server. should be removed in the future. **********************
bool DeviceManager::GetLogRecordingState() const
{
    return m_recordLogs;
}

//  ***************  TODO : For the regular tcp server. should be removed in the future. **********************
std::shared_ptr<log_collector::LogCollector> DeviceManager::GetLogCollector(const string& deviceName, const string& cpuTypeName, string& errorMessage)
{
    static const string fw = "fw";
    static const string ucode = "ucode";

    stringstream errorMessageSs;

    // find device
    shared_ptr<Device> d = GetDeviceByName(deviceName);
    if (nullptr == d)
    {
        errorMessageSs << "device name " << deviceName << " doesn't exist" << endl;
        errorMessage = errorMessageSs.str();
        return nullptr;
    }

    // get tracer
    shared_ptr<log_collector::LogCollector> tracer;
    if (fw == cpuTypeName)
    {
        tracer = d->m_fwTracer;
    }
    else if (ucode == cpuTypeName)
    {
        tracer = d->m_ucodeTracer;
    }
    else
    {
        errorMessageSs << "device " << deviceName << " has no tracer named " << cpuTypeName << "; ";
        errorMessage = errorMessageSs.str();
        return nullptr;
    }

    // check tracer is valid
    if (!tracer)
    {
        errorMessageSs << "device " << deviceName << " has no active tracer for " << cpuTypeName << "; ";
        errorMessage = errorMessageSs.str();
        return nullptr;
    }

    return tracer;
}

//  ***************  TODO : For the regular tcp server. should be removed in the future. **********************
bool DeviceManager::SetLogCollectionParameter(const string& deviceName, const string& cpuTypeName, const string& parameterAssignment, string& errorMessage)
{
    auto tracer = GetLogCollector(deviceName, cpuTypeName, errorMessage);
    if (!tracer)
    {
        return false;
    }

    return tracer->SetParameter(parameterAssignment);
}

//  ***************  TODO : For the regular tcp server. should be removed in the future. **********************
string DeviceManager::GetLogCollectionParameter(const string& deviceName, const string& cpuTypeName, const string& parameter)
{
    string errorMessage;
    auto tracer = GetLogCollector(deviceName, cpuTypeName, errorMessage);
    if (!tracer)
    {
        return errorMessage;
    }

    string value;
    tracer->GetParameter(parameter, value);
    return value;
}

//  ***************  TODO : For the regular tcp server. should be removed in the future. **********************
string DeviceManager::GetLogCollectorConfigurationString(const string& deviceName, const string& cpuTypeName)
{
    stringstream res;
    string errorMessage;

    auto tracer = GetLogCollector(deviceName, cpuTypeName, errorMessage);
    if (!tracer)
    {
        return errorMessage;
    }

    res << "device=" << deviceName << ",cpu=" << cpuTypeName << "," << tracer->GetConfigurationString() << endl;
    return res.str();
}
/*************************** End of log functions *********************************/

DeviceManagerOperationStatus DeviceManager::GetDeviceCapabilities(const string& deviceName, DWORD& capabilities)
{
    DeviceManagerOperationStatus status;
    m_connectedDevicesMutex.lock();
    if (m_devices.count(deviceName) > 0)
    {
        lock_guard<mutex> lock(m_devices[deviceName]->m_mutex);
        m_connectedDevicesMutex.unlock();
        capabilities = m_devices[deviceName]->GetCapabilities();
        status = dmosSuccess;
    }
    else
    {
        m_connectedDevicesMutex.unlock();
        status = dmosNoSuchConnectedDevice;
    }

    return status;
}

bool DeviceManager::GetDeviceInterfaceName(const string& deviceName, string& interfaceName)
{
    auto device = GetDeviceByName(deviceName);
    if (device)
    {
        interfaceName = device->GetInterfaceName();
        return true;
    }
    return false;
}

bool DeviceManager::GetDeviceBasebandType(const string& deviceName, BasebandType& type)
{
    auto device = GetDeviceByName(deviceName);
    if (device)
    {
        type = device->GetBasebandType();
        return true;
    }
    return false;
}

bool DeviceManager::GetDeviceBasebandRevision(const std::string & deviceName, BasebandRevision & revision)
{
    auto device = GetDeviceByName(deviceName);
    if (device)
    {
        revision = device->GetBasebandRevision();
        LOG_DEBUG << "in DeviceManager::GetDeviceBasebandRevision revision is:"<< revision << std::endl;
        return true;
    }
    LOG_WARNING << "in DeviceManager::GetDeviceBasebandRevision device not found" << std::endl;
    return false;
}
