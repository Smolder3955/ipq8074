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

#ifndef _COMMANDSEXECUTOR_H_
#define _COMMANDSEXECUTOR_H_

#include <string>
#include <vector>

class Host;
struct HostData;

class CommandsExecutor
{
public:
    bool GetHostData(HostData& data, std::string& failureReason);
    bool SetHostAlias(const std::string& name, std::string& failureReason);
    bool AddDeviceRegister(const std::string& deviceName,  const std::string& name, uint32_t address, uint32_t firstBit, uint32_t lastBit, std::string& failureReason);
    bool RemoveDeviceRegister(const std::string& deviceName, const std::string& name, std::string& failureReason);

private:
    //for testing purposes only
    bool GetTestHostData(HostData& data, std::string& failureReason);
    bool SetTestHostAlias(const std::string& name, std::string& failureReason);
    bool AddTestDeviceRegister(const std::string& deviceName, const std::string& name, uint32_t address, std::string& failureReason);
    bool RemoveTestDeviceRegister(const std::string& deviceName, const std::string& name, std::string& failureReason);
};

#endif // _COMMANDSEXECUTOR_H_
