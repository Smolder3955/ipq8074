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

#include <sstream>
#include <set>

#include "DummyDriver.h"
#include "Utils.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////
// Test Device interface

DummyDriver::DummyDriver(string interfaceName) : DriverAPI(interfaceName)
{
    m_registersAddressToValue.insert(make_pair(BAUD_RATE_REGISTER, 0x12345678));

    unsigned baseAddress = 0x880100;
    unsigned numOfAddresses = 40;
    unsigned numOfAddressesInRegister = 4;
    unsigned value = 0x0;
    for (unsigned i = 0; i < numOfAddresses * numOfAddressesInRegister; i += numOfAddressesInRegister)
    {
        m_registersAddressToValue.insert(make_pair(baseAddress + i, value++));
    }
}

DummyDriver::~DummyDriver()
{
    Close();
}

// Virtual access functions for device
bool DummyDriver::Read(DWORD address, DWORD& value)
{
    auto registerElement = m_registersAddressToValue.find(address);
    if (registerElement != m_registersAddressToValue.end())
    {
        value = registerElement->second;
        return true;

    }
    value = Utils::REGISTER_DEFAULT_VALUE;
    return false;
}

bool DummyDriver::ReadBlock(DWORD address, DWORD blockSizeInDwords, vector<DWORD>& values)
{
    auto registerElement = m_registersAddressToValue.find(address);
    if (registerElement != m_registersAddressToValue.end())
    {
        for (DWORD v = 0x0; v < blockSizeInDwords; ++v)
        {
            values.push_back(registerElement->second + v);
        }
        return true;
    }
    return false;
}

bool DummyDriver::Write(DWORD address, DWORD value)
{
    auto registerElement = m_registersAddressToValue.find(address);
    if (registerElement != m_registersAddressToValue.end())
    {
        registerElement->second = value;
        return true;
    }
    return false;
}

bool DummyDriver::WriteBlock(DWORD address, vector<DWORD> values)
{
    auto registerElement = m_registersAddressToValue.find(address);
    if (registerElement != m_registersAddressToValue.end())
    {
        unsigned i = 0;
        for (auto v = values.begin(); v != values.end(); ++v)
        {
            m_registersAddressToValue[address + i] = *v;
            ++i;
        }
        return true;
    }
    return false;
}

bool DummyDriver::Open()
{
    return true;
}

void DummyDriver::Close()
{
    return;
}

OperationStatus DummyDriver::InterfaceReset()
{
    // does nothing, but doesn't fail
    return OperationStatus();
}

set<string> DummyDriver::Enumerate()
{
    stringstream deviceNameDelimiter;
    deviceNameDelimiter << DEVICE_NAME_DELIMITER;
    set<string> discoveredDevices;

    // add first device
    string deviceName = std::string("TEST") + deviceNameDelimiter.str() + "wTest0";
    discoveredDevices.insert(deviceName);

    // add second device
    deviceName = std::string("TEST") + deviceNameDelimiter.str() + "wTest1";
    discoveredDevices.insert(deviceName);

    return discoveredDevices;
}
