/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "UnixRtlDriver.h"
#include "DebugLogger.h"
#include "RtlSimEnumerator.h"
#include "AddressTranslator.h"

#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>

#include <sys/types.h> // for opendir
#include <dirent.h> // for opendir
#include <errno.h>

using namespace std;

// *************************************************************************************************

namespace
{
    // RTL Simulator contract
    const string TO_RTL_FILE_NAME = "DM2Sim.txt";
    const string TO_RTL_DONE_FILE_NAME = "DM2Sim.done";
    const string FROM_RTL_DONE_FILE_NAME = "Sim2DM.done";
    const string FROM_RTL_REPLY_FILE_NAME = "Sim2DM.txt";

    const bool DEBUG_MODE = false;

    // Helper utility to print reply vector to log
    std::ostream& operator<<(std::ostream& os, const std::vector<uint32_t>& hexVector)
    {
        os << '[';
        for(const auto& value: hexVector)
        {
            os << Hex<8>(value) << ", ";
        }

        return os << "\b\b]";
    }

}

using namespace std;

// *************************************************************************************************

UnixRtlDriver::UnixRtlDriver(const std::string& interfaceName)
    : DriverAPI(interfaceName)
    , m_RtlDirectory(RtlSimEnumerator::GetRtlDirectory())
    , m_InterfacePath(m_RtlDirectory + m_interfaceName + "/")
    , m_ToRtlCmdFileName(m_InterfacePath + TO_RTL_FILE_NAME)
    , m_ToRtlDoneFileName(m_InterfacePath + TO_RTL_DONE_FILE_NAME)
    , m_FromRtlDoneFileName(m_InterfacePath + FROM_RTL_DONE_FILE_NAME)
    , m_FromRtlReplyFileName(m_InterfacePath + FROM_RTL_REPLY_FILE_NAME)
{
}

// *************************************************************************************************

bool UnixRtlDriver::ExecuteCommand(
    const IRtlSimCommand& cmd, vector<DWORD>& refReply) const
{
    LOG_VERBOSE << "Start executing RTL Simulator command: " << cmd << std::endl;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    bool bSuccess =
        WriteCommandFile(cmd) &&
        WriteDoneFile() &&
        WaitForSimulatorDoneFile(cmd) &&
        ReadSimulatorReply(cmd, refReply);

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    int executionTimeMsec = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    LOG_DEBUG << "RTL Command: " << cmd
              << " Reply: " << refReply
              << " Time: " << executionTimeMsec << " msec"
              << std::endl;

    return bSuccess;
}

// *************************************************************************************************

bool UnixRtlDriver::WriteCommandFile(const IRtlSimCommand& cmd) const
{
    string dm2SimFilePath(m_ToRtlCmdFileName);
    LOG_VERBOSE << "Creating command file: " << dm2SimFilePath << std::endl;
    ofstream dm2Sim(dm2SimFilePath);
    if (!dm2Sim)
    {
        LOG_ERROR << "Cannot open RTL command file."
                  << " File Name: " << dm2SimFilePath
                  << " Error: " << strerror(errno)
                  << std::endl;
        return false;
    }

    cmd.FlushToFile(dm2Sim);
    dm2Sim.close();
    return true;
}

// *************************************************************************************************

bool UnixRtlDriver::WriteDoneFile() const
{
    string doneFilePath(m_ToRtlDoneFileName);
    LOG_VERBOSE << "Creating done file: " << doneFilePath << std::endl;

    ofstream dm2SimDone (doneFilePath);
    if (!dm2SimDone)
    {
        LOG_ERROR << "Cannot open done file."
                  << " File Name: " << doneFilePath
                  << " Error: " << strerror(errno)
                  << std::endl;
        return false;
    }

    dm2SimDone.close();
    return true;
}

// *************************************************************************************************

bool UnixRtlDriver::WaitForSimulatorDoneFile(const IRtlSimCommand& cmd) const
{
    LOG_VERBOSE << "Waiting for Simulation done file: " << m_FromRtlDoneFileName << std::endl;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    while (!SimulatorDoneFileExists())
    {
        LOG_VERBOSE << "Cannot stat Sim done file: " << strerror(errno) << std::endl;

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        int elapsedMSec = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        LOG_VERBOSE << "Polling for sim done: " << elapsedMSec << " msec" << std::endl;

        if (elapsedMSec >= SIMULATOR_REPLY_TIMEOUT_MSEC)
        {
            LOG_ERROR << "Timeout while waiting for reply from RTL driver."
                      << " Elapsed Time: " << elapsedMSec << " msec"
                      << " Timeout: " << SIMULATOR_REPLY_TIMEOUT_MSEC << " msec"
                      << " File: " << m_FromRtlDoneFileName
                      << " Command: " << cmd
                      << endl;

            if (DEBUG_MODE)
            {
                LOG_ERROR << "=================================" << std::endl;
                LOG_ERROR << "DEBUG MODE - Terminate on timeout" << std::endl;
                LOG_ERROR << "=================================" << std::endl;

                exit(1);
            }
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    int simHandlingMSec = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    LOG_VERBOSE << "Detected simulation done flag"
                << " File: "  << m_FromRtlDoneFileName
                << " Simulation time: " << simHandlingMSec << " msec" << std:: endl;
    remove(m_FromRtlDoneFileName.c_str());

    return true;
}

// *************************************************************************************************

bool UnixRtlDriver::SimulatorDoneFileExists() const
{
    // Files are created on NFS, so it may take some time for updates to propagate over network.
    // Opening a directory enforces NFS synchronization.
    DIR *pdir = opendir(m_InterfacePath.c_str());
    if (pdir)
    {
        closedir(pdir);
    }

    struct stat buffer;
    return (stat(m_FromRtlDoneFileName.c_str(), &buffer) == 0);
}

// *************************************************************************************************

bool UnixRtlDriver::ReadSimulatorReply(const IRtlSimCommand& cmd, vector<DWORD>& reply) const
{
    string rtlReplyFilePath(m_FromRtlReplyFileName);
    LOG_VERBOSE << "Reading Simulation reply from: " << rtlReplyFilePath << std::endl;

    ifstream sim2Dm(rtlReplyFilePath);
    if (!sim2Dm)
    {
        LOG_ERROR << "Couldn't open the answer file from the simulation."
                  << "Command: " << cmd
                  << " File Name: " << rtlReplyFilePath
                  << " Error: " << strerror(errno)
                  << endl;
        return false;
    }

    DWORD value = 0;
    while (sim2Dm >> value)
    {
        reply.push_back(value);
        LOG_VERBOSE << "Got data from RTL: " << Hex<8>(value) << std::endl;
    }
    sim2Dm.close();

    return true;
}

// *************************************************************************************************

bool UnixRtlDriver::Read(DWORD address, DWORD& value)
{
    uint32_t ahbAddress = address;
    bool conversionSuccess = AddressTranslator::ToAhbAddress(address, ahbAddress, BASEBAND_TYPE_TALYN);
    if (!conversionSuccess)
    {
        LOG_ERROR << "Unsupported address: " << Hex<8>(address) << " for Talyn RTL" << std::endl;
        return false;
    }

    RtlSimCommandRead cmd(ahbAddress);
    std::vector<uint32_t> reply;

    if (!ExecuteCommand(cmd, reply))
    {
        return false;
    }

    if (reply.size() != 1)
    {
        LOG_ERROR << "Unexpected reply size for RD command. Expected: 1"
                  << " Actual: " << reply.size() << " Command: " << cmd << std::endl;
        return false;
    }

    value = reply.back();
    return true;
}

// *************************************************************************************************

bool UnixRtlDriver::ReadBlock(DWORD address, DWORD blockSize, vector<DWORD>& values)
{
    uint32_t ahbAddress = address;
    bool conversionSuccess = AddressTranslator::ToAhbAddress(address, ahbAddress, BASEBAND_TYPE_TALYN);
    if (!conversionSuccess)
    {
        LOG_ERROR << "Unsupported address: " << Hex<8>(address) << " for Talyn RTL" << std::endl;
        return false;
    }

    RtlSimCommandReadBlock cmd(ahbAddress, blockSize);

    if (!ExecuteCommand(cmd, values))
    {
        return false;
    }

    if (values.size() != blockSize)
    {
        LOG_ERROR << "Unexpected reply size for RD command. Expected: " << blockSize
                  << " Actual: " << values.size() << " Command: " << cmd << std::endl;
        return false;
    }

    return true;
}

// *************************************************************************************************

bool UnixRtlDriver::Write(DWORD address, DWORD value)
{
    uint32_t ahbAddress = address;
    bool conversionSuccess = AddressTranslator::ToAhbAddress(address, ahbAddress, BASEBAND_TYPE_TALYN);
    if (!conversionSuccess)
    {
        LOG_ERROR << "Unsupported address: " << Hex<8>(address) << " for Talyn RTL" << std::endl;
        return false;
    }

    RtlSimCommandWrite cmd(ahbAddress, value);
    std::vector<uint32_t> reply;

    if (!ExecuteCommand(cmd, reply))
    {
        return false;
    }

    if (reply.size() != 1)
    {
        LOG_ERROR << "Unexpected reply size for WR command. Expected: 1"
                  << " Actual: " << reply.size() << " Command: " << cmd << std::endl;
        return false;
    }

    // RTL Simulator returns zero for successful write
    if (reply.front() != 0)
    {
        // Meaning that the write wasn't succeeded
        LOG_ERROR << "RTL Simulator rejected the write command: " << cmd << " Exit code: " << reply.front() << std::endl;
        return false;
    }

    return true;
}

// *************************************************************************************************

bool UnixRtlDriver::WriteBlock(DWORD address, vector<DWORD> values)
{
    uint32_t ahbAddress = address;
    bool conversionSuccess = AddressTranslator::ToAhbAddress(address, ahbAddress, BASEBAND_TYPE_TALYN);
    if (!conversionSuccess)
    {
        LOG_ERROR << "Unsupported address: " << Hex<8>(address) << " for Talyn RTL" << std::endl;
        return false;
    }

    RtlSimCommandWriteBlock cmd(ahbAddress, values);
    std::vector<uint32_t> reply;

    if (!ExecuteCommand(cmd, reply))
    {
        return false;
    }

    if (reply.size() == 0 || reply.back() != 0)
    {
        // Meaning that the write wasn't succeeded
        LOG_ERROR << " RTL Simulator rejected the write block command: " << cmd << std::endl;
        return false;
    }

    return true;
}

