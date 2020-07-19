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
#include "AccessLayerAPI.h"
#include "DriverFactory.h"
#include "Utils.h"

using namespace std;

set<string> AccessLayer::GetDrivers()
{
    set<string> enumeratedDevices;

    // Enumerate

#ifdef _WINDOWS
    set<string> pciDevices = WindowsDriverAPI::Enumerate();
    set<string> jtagDevices = JTagDriver::Enumerate();
    enumeratedDevices.insert(pciDevices.begin(), pciDevices.end());
    enumeratedDevices.insert(jtagDevices.begin(), jtagDevices.end());
    //set<string> serialDevices = SerialDevice::Enumerate();
    //enumeratedDevices.insert(serialDevices.begin(), serialDevices.end());
#elif __OS3__
    set<string> pciDevices = OS3DriverAPI::Enumerate();
    enumeratedDevices.insert(pciDevices.begin(), pciDevices.end());
#elif RTL_SIMULATION
    set<string> rtlDevices = RtlSimEnumerator::Enumerate();
    enumeratedDevices.insert(rtlDevices.begin(), rtlDevices.end());
#else
    set<string> pciDevices = UnixPciDriver::Enumerate();
    enumeratedDevices.insert(pciDevices.begin(), pciDevices.end());
#endif

#ifdef _UseTestDevice
    set<string> testDevices = TestDevice::Enumerate();
    enumeratedDevices.insert(testDevices.begin(), testDevices.end());
#endif // _UseTestDevice

#ifdef _WIGIG_ARCH_SPI
    set<string> spiDevices = SpiEnumerator::Enumerate();
    enumeratedDevices.insert(spiDevices.begin(), spiDevices.end());
#endif   // _WIGIG_ARCH_SPI

    return enumeratedDevices;
}

unique_ptr<DriverAPI> AccessLayer::OpenDriver(string deviceName)
{
    vector<string> tokens = Utils::Split(deviceName, DEVICE_NAME_DELIMITER);

    // Device name consists of exactly 3 elements:
    // 1. Baseband Type (SPARROW, TALYN...)
    // 2. Transport Type (PCI, JTAG, Serial...)
    // 3. Interface name (wMp, wPci, wlan0, wigig0...)
    if (tokens.size() != 2)
    {
        // LOG_MESSAGE_ERROR
        return NULL;
    }

    // Transport type
    unique_ptr<DriverAPI> pDevice;

    if (Utils::PCI == tokens[0])
    {
        pDevice = DriverFactory::GetDriver(PCI, tokens[1]);
    }

#ifdef __linux__
    if (Utils::RTL == tokens[0])
    {
        pDevice = DriverFactory::GetDriver(RTL, tokens[1]);
    }
#endif

#ifdef _WINDOWS
    if (Utils::JTAG == tokens[0])
    {
        pDevice = DriverFactory::GetDriver(JTAG, tokens[1]);
    }

    if (Utils::SERIAL == tokens[0])
    {
        pDevice = DriverFactory::GetDriver(SERIAL, tokens[1]);
    }
#endif

#ifdef _UseDummyDevice
    if (Utils::DUMMY == tokens[0])
    {
        pDevice = DriverFactory::GetDriver(DUMMY, tokens[1]);
    }
#endif // _UseOnlyTestDevice

#ifdef _WIGIG_ARCH_SPI
    if (Utils::SPI == tokens[0])
    {
        pDevice = DriverFactory::GetDriver(SPI, tokens[1]);
    }
#endif

    if (NULL != pDevice.get())
    {
        if (!pDevice->Open())
        {
            LOG_ERROR << "Failed to open interface " << tokens[0] << endl;
        }
    }
    return pDevice;
}
