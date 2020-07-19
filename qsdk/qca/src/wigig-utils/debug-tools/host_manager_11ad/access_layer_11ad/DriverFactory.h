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

#ifndef _DRIVER_FACTORY_
#define _DRIVER_FACTORY_

#pragma once

#include <memory>

#include "DummyDriver.h"
#include "DebugLogger.h"

#ifdef _WINDOWS
#include "WindowsPciDriver.h"
#include "JTagDriver.h"
#include "SerialDriver.h"
#elif __OS3__
#include "OS3DriverAPI.h"
#else
#include "UnixPciDriver.h"
#include "UnixRtlDriver.h"
#include "RtlSimEnumerator.h"
#endif // _WINDOWS

#ifdef _WIGIG_ARCH_SPI
#include "SpiEnumerator.h"
#include "SpiDriver.h"
#endif   // _WIGIG_ARCH_SPI

class DriverAPI;

enum DeviceType
{
    PCI,
    JTAG,
    SERIAL,
    DUMMY,
    RTL,
    SPI
};

class DriverFactory
{
public:

    static std::unique_ptr<DriverAPI> GetDriver(DeviceType deviceType, const std::string& interfaceName)
    {
        switch (deviceType)
        {
#ifdef _WINDOWS
        case JTAG:
            return std::unique_ptr<JTagDriver>(new JTagDriver(interfaceName));
        case SERIAL:
            return std::unique_ptr<SerialDriver>(new SerialDriver(interfaceName));
#endif // WINDOWS
        case DUMMY:
            return std::unique_ptr<DummyDriver>(new DummyDriver(interfaceName));
        case PCI:
#ifdef _WINDOWS
            return std::unique_ptr<WindowsDriverAPI>(new WindowsDriverAPI(interfaceName));
#elif __OS3__
            return std::unique_ptr<OS3DriverAPI>(new OS3DriverAPI(interfaceName));
#else
            return std::unique_ptr<UnixPciDriver>(new UnixPciDriver(interfaceName));
        case RTL:
            return std::unique_ptr<UnixRtlDriver>(new UnixRtlDriver(interfaceName));
#endif // WINDOWS

#ifdef _WIGIG_ARCH_SPI
        case SPI:
            return std::unique_ptr<SpiDriver>(new SpiDriver(interfaceName));
#endif
        default:
            LOG_ERROR << "Got invalid device type. Return an empty driver" << std::endl;
            return std::unique_ptr<DriverAPI>(new DriverAPI(interfaceName));
        }
    }
};

#endif // !_DRIVER_FACTORY_
