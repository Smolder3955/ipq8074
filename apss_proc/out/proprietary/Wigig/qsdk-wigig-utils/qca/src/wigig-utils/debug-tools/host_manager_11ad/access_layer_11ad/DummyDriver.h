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

#ifndef _11AD_DUMMY_DRIVER_H_
#define _11AD_DUMMY_DRIVER_H_

#include <map>
#include <set>
#include <string>

#include "DriverAPI.h"

class DummyDriver : public DriverAPI
{
public:
    explicit DummyDriver(std::string interfaceName);
    ~DummyDriver() override;

    // Device Management
    bool Open();
    void Close();

    // Virtual access functions for device
    bool Read(DWORD address, DWORD& value) override;
    bool ReadBlock(DWORD address, DWORD blockSizeInDwords, std::vector<DWORD>& values) override;
    bool Write(DWORD address, DWORD value) override;
    bool WriteBlock(DWORD address, std::vector<DWORD> values) override;

    OperationStatus InterfaceReset() override;

    static std::set<std::string> Enumerate();

private:
    std::map<DWORD, DWORD> m_registersAddressToValue;
};
#endif //_11AD_DUMMY_DRIVER_H_
