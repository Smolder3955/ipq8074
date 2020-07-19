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

#ifndef _11AD_DRIVER_API_H_
#define _11AD_DRIVER_API_H_

#include <string>
#include <vector>
#include <cstdint>

#include "OperatingSystemConstants.h"
#include "OperationStatus.h"

#define BAUD_RATE_REGISTER 0x880050
#define DEVICE_NAME_DELIMITER '!'

class DriverAPI
{
public:
    DriverAPI(const std::string& interfaceName): m_interfaceName(interfaceName)
    {
    }

    virtual ~DriverAPI() {}

    // Base access functions (to be implemented by specific device)
    virtual bool Read(DWORD /*address*/, DWORD& /*value*/) { return false;  }
    virtual bool ReadBlock(DWORD /*addr*/, DWORD /*blockSizeInDwords*/, std::vector<DWORD>& /*values*/) { return false; }
    virtual bool ReadBlock(DWORD /*addr*/, DWORD /*blockSizeInBytes*/, char * /*arrBlock*/) { return false; }
    virtual bool ReadRadarData(DWORD /*maxBlockSizeInDwords*/, std::vector<DWORD>& /*values*/) { return false; }
    virtual bool Write(DWORD /*address*/, DWORD /*value*/) { return false; }
    virtual bool WriteBlock(DWORD /*addr*/, std::vector<DWORD> /*values*/) { return false; };
    virtual bool WriteBlock(DWORD /*address*/, DWORD /*blockSize*/, const char* /*valuesToWrite*/) { return false; }

    // PMC functions
    virtual bool AllocPmc(unsigned /*descSize*/, unsigned /*descNum*/, std::string& /*outMessage*/) { return false; }
    virtual bool DeallocPmc(bool /*bSuppressError*/, std::string& /*outMessage*/) { return false; }
    virtual bool CreatePmcFiles(unsigned /*refNumber*/, std::string& /*outMessage*/) { return false; }
    virtual bool FindPmcDataFile(unsigned /*refNumber*/, std::string& /*outMessage*/) { return false; }
    virtual bool FindPmcRingFile(unsigned /*refNumber*/, std::string& /*outMessage*/) { return false; }

    virtual bool IsOpen(void) { return false; }
    virtual bool Open() { return false;  }
    virtual bool ReOpen() { return false; };
    virtual bool DriverControl(uint32_t /*id*/, const void* /*inBuf*/, uint32_t /*inBufSize*/,
                               void* /*outBuf*/, uint32_t /*outBufSize*/, DWORD* /*pLastError*/ = nullptr) { return false; }
    virtual void Close() {}

    virtual bool GetDriverMode(int& /*currentState*/) { return false; }
    virtual bool SetDriverMode(int /*newState*/, int& /*oldState*/) { return false; }

    virtual bool RegisterDriverControlEvents() { return false; }

    virtual OperationStatus InterfaceReset() { return OperationStatus(false, "Interface Reset is not supported on the current platform"); }

    const std::string& GetInterfaceName() const
    {
        return m_interfaceName;
    }

    virtual bool IsValid()
    {
        DWORD value;
        if (!Read(BAUD_RATE_REGISTER, value))
        {
            return false;
        }
        return IsValidInternal();
    }

protected:
    const std::string m_interfaceName;

private:
    virtual bool IsValidInternal() { return true; }
};


#endif //_11AD_DRIVER_API_H_
