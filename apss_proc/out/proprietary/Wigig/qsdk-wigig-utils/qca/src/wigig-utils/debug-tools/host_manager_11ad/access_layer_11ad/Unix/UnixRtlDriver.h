/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
#ifndef _11AD_UNIX_RTL_DRIVER_H_
#define _11AD_UNIX_RTL_DRIVER_H_

#include "DriverAPI.h"
#include "RtlSimCommand.h"

#include <string>
#include <set>
#include <vector>

// *************************************************************************************************

class UnixRtlDriver : public DriverAPI
{
public:

    explicit UnixRtlDriver(const std::string& interfaceName);

    bool Read(DWORD address, DWORD& value) override;
    bool ReadBlock(DWORD address, DWORD blockSize, std::vector<DWORD>& values) override;
    bool Write(DWORD address, DWORD value) override;
    bool WriteBlock(DWORD addr, std::vector<DWORD> values) override;

    bool Open() override { return true; }
    bool ReOpen() override { return true; }
    bool IsValid() override { return true; }

private:

    static const int SIMULATOR_REPLY_TIMEOUT_MSEC = 500000;

    const std::string m_RtlDirectory;
    const std::string m_InterfacePath;
    const std::string m_ToRtlCmdFileName;
    const std::string m_ToRtlDoneFileName;
    const std::string m_FromRtlDoneFileName;
    const std::string m_FromRtlReplyFileName;

    bool ExecuteCommand(const IRtlSimCommand& cmd, std::vector<DWORD>& refReply) const;
    bool WriteCommandFile(const IRtlSimCommand& cmd) const;
    bool WriteDoneFile() const;
    bool WaitForSimulatorDoneFile(const IRtlSimCommand& cmd) const;
    bool SimulatorDoneFileExists() const;
    bool ReadSimulatorReply(const IRtlSimCommand& cmd, std::vector<DWORD>& reply) const;
};

#endif //_11AD_UNIX_PCI_DRIVER_H_
