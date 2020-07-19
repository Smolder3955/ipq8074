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

#include <array>
#include <sstream>
#include <algorithm>

#include "CommandsExecutor.h"
#include "Host.h"
#include "DebugLogger.h"
#include "DeviceManager.h"
#include "HostInfo.h"

using namespace std;

// *************************************************************************************************
bool CommandsExecutor::GetHostData(HostData& data, std::string& failureReason)
{
    LOG_VERBOSE << __FUNCTION__ << endl;

    if (Host::GetHost().GetHostUpdate(data))
    {
        return true;
    }

    // failure reason is not in use for now
    // may fill with more info (if available) to show in UI
    (void)failureReason;

    return false;
}

// *************************************************************************************************
bool CommandsExecutor::SetHostAlias(const string& name, std::string& failureReason)
{
    LOG_VERBOSE << __FUNCTION__ << endl;

    if (Host::GetHost().GetHostInfo().SaveAliasToFile(name))
    {
        return true;
    }

    // failure reason is not in use for now
    // may fill with more info (if available) to show in UI
    (void)failureReason;

    return false;
}

bool CommandsExecutor::AddDeviceRegister(const std::string& deviceName, const string& name, uint32_t address, uint32_t firstBit, uint32_t lastBit, std::string& failureReason)
{
    LOG_VERBOSE << __FUNCTION__ << endl;

    return Host::GetHost().GetDeviceManager().AddRegister(deviceName, name, address, firstBit, lastBit, failureReason);
}

bool CommandsExecutor::RemoveDeviceRegister(const std::string& deviceName, const string& name, std::string& failureReason)
{
    LOG_VERBOSE << __FUNCTION__ << endl;

    return Host::GetHost().GetDeviceManager().RemoveRegister(deviceName, name, failureReason);
}

//for testing purposes only
static string hostAlias = "STA-1021";
static vector<StringNameValuePair> customRegs[3] =
{
    std::vector<StringNameValuePair>(),
    std::vector<StringNameValuePair>(),
    std::vector<StringNameValuePair>{ { "Test1", "0x87" },{ "Test2", "OK" },{ "Test3", "0" }}
};
static std::array<string, 3> deviceNames{ {"wlan0", "wlan1", "wlan2"} };

bool CommandsExecutor::GetTestHostData(HostData& data, std::string& /*failureReason*/)
{
    static int counter = 0;
    static bool associated = true;
    static int rfStatus = 0;
    if (counter >= 10)
    {
        counter = 0;
        associated = !associated;
        rfStatus++;
        if (rfStatus >= 3)
        {
            rfStatus = 0;
        }
    }
    counter++;

    data.m_hostManagerVersion = "1.1.2";
    data.m_hostAlias = hostAlias;
    data.m_hostIP = "10.18.172.149";

    DeviceData device1;
    device1.m_deviceName = deviceNames[0];
    device1.m_associated = associated;
    device1.m_signalStrength = 5;
    device1.m_fwAssert = 0;
    device1.m_uCodeAssert = 0;
    device1.m_mcs = 7;
    device1.m_channel = 1;
    device1.m_fwVersion.m_major = 5;
    device1.m_fwVersion.m_minor = 1;
    device1.m_fwVersion.m_subMinor = 0;
    device1.m_fwVersion.m_build = 344;
    device1.m_bootloaderVersion = "7253";
    device1.m_mode = "Operational";
    device1.m_compilationTime.m_hour = 14;
    device1.m_compilationTime.m_min = 52;
    device1.m_compilationTime.m_sec = 1;
    device1.m_compilationTime.m_day = 7;
    device1.m_compilationTime.m_month = 8;
    device1.m_compilationTime.m_year = 2017;
    device1.m_hwType = "SPR-D0";
    device1.m_hwTemp = "32.1";
    device1.m_rfType = "SPR-R";
    device1.m_rfTemp = "32.1";
    device1.m_boardFile = "Generic_500mW";
    device1.m_rf.push_back(2);
    device1.m_rf.push_back(1);
    device1.m_rf.push_back(2);
    device1.m_rf.push_back(1);
    device1.m_rf.push_back(rfStatus);
    device1.m_rf.push_back(0);
    device1.m_rf.push_back(0);
    device1.m_rf.push_back(0);
    device1.m_fixedRegisters.push_back({ "uCodeRxOn", "Rx On" });
    device1.m_fixedRegisters.push_back({ "BfSeq", "2" });
    device1.m_fixedRegisters.push_back({ "BfTrigger", "FW_TRIG" });
    device1.m_fixedRegisters.push_back({ "NAV", "5" });
    device1.m_fixedRegisters.push_back({ "TxGP", "1001" });
    device1.m_fixedRegisters.push_back({ "RxGP", "547" });
    device1.m_customRegisters = customRegs[0];
    data.m_devices.push_back(device1);

    DeviceData device2;
    device2.m_deviceName = deviceNames[1];
    device2.m_associated = false;
    device2.m_signalStrength = 0;
    device2.m_fwAssert = 0;
    device2.m_uCodeAssert = 0;
    device2.m_mcs = 0;
    device2.m_channel = 0;
    device2.m_fwVersion.m_major = 4;
    device2.m_fwVersion.m_minor = 1;
    device2.m_fwVersion.m_subMinor = 0;
    device2.m_fwVersion.m_build = 1;
    device2.m_bootloaderVersion = "7253";
    device2.m_mode = "Operational";
    device2.m_compilationTime.m_hour = 11;
    device2.m_compilationTime.m_min = 11;
    device2.m_compilationTime.m_sec = 11;
    device2.m_compilationTime.m_day = 5;
    device2.m_compilationTime.m_month = 8;
    device2.m_compilationTime.m_year = 2017;
    device2.m_hwType = "SPR-D1";
    device2.m_hwTemp = "22.1";
    device2.m_rfType = "SPR-R";
    device2.m_rfTemp = "22.1";
    device2.m_boardFile = "Generic_400mW";
    device2.m_rf.push_back(2);
    device2.m_rf.push_back(1);
    device2.m_rf.push_back(2);
    device2.m_rf.push_back(1);
    device2.m_rf.push_back(0);
    device2.m_rf.push_back(0);
    device2.m_rf.push_back(0);
    device2.m_rf.push_back(0);
    device2.m_fixedRegisters.push_back({ "uCodeRxOn", "Rx On" });
    device2.m_fixedRegisters.push_back({ "BfSeq", "1" });
    device2.m_fixedRegisters.push_back({ "BfTrigger", "FW_TRIG" });
    device2.m_fixedRegisters.push_back({ "NAV", "5" });
    device2.m_fixedRegisters.push_back({ "TxGP", "1" });
    device2.m_fixedRegisters.push_back({ "RxGP", "1" });
    device2.m_customRegisters = customRegs[1];
    data.m_devices.push_back(device2);

    DeviceData device3;
    device3.m_deviceName = deviceNames[2];
    device3.m_associated = false;
    device3.m_signalStrength = 0;
    device3.m_fwAssert = 0x29;
    device3.m_uCodeAssert = 0xff;
    device3.m_mcs = 0;
    device3.m_channel = 2;
    device3.m_fwVersion.m_major = 3;
    device3.m_fwVersion.m_minor = 2;
    device3.m_fwVersion.m_subMinor = 0;
    device3.m_fwVersion.m_build = 5;
    device3.m_bootloaderVersion = "7253";
    device3.m_mode = "Error";
    device3.m_compilationTime.m_hour = 14;
    device3.m_compilationTime.m_min = 52;
    device3.m_compilationTime.m_sec = 1;
    device3.m_compilationTime.m_day = 7;
    device3.m_compilationTime.m_month = 8;
    device3.m_compilationTime.m_year = 2017;
    device3.m_hwType = "SPR-D1";
    device3.m_hwTemp = "22.1";
    device3.m_boardFile = "Generic_400mW";
    device3.m_rf.push_back(0);
    device3.m_rf.push_back(0);
    device3.m_rf.push_back(0);
    device3.m_rf.push_back(0);
    device3.m_rf.push_back(0);
    device3.m_rf.push_back(0);
    device3.m_rf.push_back(0);
    device3.m_rf.push_back(0);
    device3.m_fixedRegisters.push_back({ "uCodeRxOn", "Rx Off" });
    device3.m_fixedRegisters.push_back({ "BfSeq", "1" });
    device3.m_fixedRegisters.push_back({ "NAV", "4" });
    device3.m_fixedRegisters.push_back({ "RxGP", "0" });
    device3.m_customRegisters = customRegs[2];

    data.m_devices.push_back(device3);

    return true;
}

bool CommandsExecutor::SetTestHostAlias(const std::string& name, std::string& /*failureReason*/)
{
    hostAlias = name;
    return true;
}

bool CommandsExecutor::AddTestDeviceRegister(const std::string& deviceName, const std::string& name, uint32_t address, std::string& /*failureReason*/)
{
    for (size_t i = 0; i < deviceNames.size(); i++)
    {
        if (deviceNames[i] == deviceName)
        {
            auto elem = std::find_if(std::begin(customRegs[i]), std::end(customRegs[i]),
                [&name](const StringNameValuePair& item) {return item.m_name == name; });
            if (std::end(customRegs[i]) == elem)
            {
                std::stringstream ss;
                ss << address;
                customRegs[i].push_back({ name, ss.str() });
                return true;
            }
        }
    }
    return false;
}

bool CommandsExecutor::RemoveTestDeviceRegister(const std::string& deviceName, const std::string& name, std::string& /*failureReason*/)
{
    for (size_t i = 0; i < deviceNames.size(); i++)
    {
        if (deviceNames[i] == deviceName)
        {
            auto elem = std::find_if(std::begin(customRegs[i]), std::end(customRegs[i]),
                [&name](const StringNameValuePair& item) {return item.m_name == name; });
            if (std::end(customRegs[i]) != elem)
            {
                customRegs[i].erase(elem);
                return true;
            }
        }
    }
    return false;
}