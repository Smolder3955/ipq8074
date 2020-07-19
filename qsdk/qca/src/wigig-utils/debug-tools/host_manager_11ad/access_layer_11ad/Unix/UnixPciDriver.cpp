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

#include "DebugLogger.h"
#include "UnixPciDriver.h"
#include "PmcCfg.h"
#include "PmcData.h"
#include "ShellCommandExecutor.h"
#include "OperationStatus.h"
#include "Utils.h"

#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <net/if.h> // for struct ifreq
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h> // for struct ifreq
#include <sys/types.h> // for opendir
#include <dirent.h> // for opendir

#define EP_OPERATION_READ 0
#define EP_OPERATION_WRITE 1
#define WIL_IOCTL_MEMIO (SIOCDEVPRIVATE + 2)
#define WIL_IOCTL_MEMIO_BLOCK (SIOCDEVPRIVATE + 3)
#define WIL_IOCTL_MEMIO_RDR_GET_DATA (SIOCDEVPRIVATE + 4)

using namespace std;

static const string PCI_END_POINT_FOLDER = "/sys/module/wil6210/drivers/pci:wil6210/";
static const char* INVALID = "Invalid";

UnixPciDriver::UnixPciDriver(const std::string& interfaceName) :
    DriverAPI(interfaceName),
    m_initialized(false),
    m_fileDescriptor(-1)
{
    Open();
}

UnixPciDriver::~UnixPciDriver()
{
    Close();
}

bool UnixPciDriver::IsReady() const
{
    return m_fileDescriptor >= 0;
}


// Base access functions (to be implemented by specific device)
bool UnixPciDriver::Read(DWORD address, DWORD& value)
{
    IoctlIO io;

    io.addr = address;
    io.val = 0;
    io.op = EP_OPERATION_READ;
    if (!SendRWIoctl(io, m_fileDescriptor, m_interfaceName.c_str()))
    {
        return false;
    }

    value = io.val;
    LOG_VERBOSE << "in UnixPciDriver::Read(" << Address(address) << ") = " << Hex<8>(value) << std::endl;
    return true;
}

bool UnixPciDriver::ReadBlock(DWORD address, DWORD blockSizeInDwords, vector<DWORD>& values)
{
    values.clear();
    values.resize(blockSizeInDwords);

    char* buffer = reinterpret_cast<char*>(values.data());
    const uint32_t blockSizeInBytes = values.size() * sizeof(DWORD);

    return ReadBlock(address, blockSizeInBytes, buffer);
}

bool UnixPciDriver::ReadBlock(DWORD address, DWORD blockSizeInBytes, char *arrBlock)
{
    IoctlIOBlock io;
    io.op = EP_OPERATION_READ;
    io.addr = address;
    io.size = blockSizeInBytes;
    io.buf = arrBlock;

    return SendRWBIoctl(io, m_fileDescriptor, m_interfaceName.c_str());
}

bool UnixPciDriver::ReadRadarData(DWORD maxBlockSizeInDwords, vector<DWORD>& values)
{
    values.clear();
    values.resize(maxBlockSizeInDwords);

    char* buffer = reinterpret_cast<char*>(values.data());
    const uint32_t blockSizeInBytes = values.size() * sizeof(DWORD);
    IoctlRadarBlock rd{ blockSizeInBytes, buffer };

    struct ifreq ifr;
    ifr.ifr_data = reinterpret_cast<char*>(&rd);
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", m_interfaceName.c_str());
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    int ret = ioctl(m_fileDescriptor, WIL_IOCTL_MEMIO_RDR_GET_DATA, &ifr);
    if (ret < 0)
    {
        LOG_ERROR << "Failed to send RDR_GET_DATA IOCTL: " << strerror(errno) << std::endl;
        return false;
    }

    // Convert to number of DWORDS
    if (ret % 4 != 0)
    {
        ret += 4;
    }
    ret /= 4;

    // it is important to shrink the vector to the actual size in use
    values.resize(ret);

    return true;
}

bool UnixPciDriver::Write(DWORD address, DWORD value)
{
    IoctlIO io;

    io.addr = address;
    io.val = value;
    io.op = EP_OPERATION_WRITE;

    if (!SendRWIoctl(io, m_fileDescriptor, m_interfaceName.c_str()))
    {
        return false;
    }

    return true;
}

bool UnixPciDriver::WriteBlock(DWORD address, DWORD blockSize, const char *valuesToWrite)
{
    IoctlIOBlock io;
    io.op = EP_OPERATION_WRITE;
    io.addr = address;
    io.size = blockSize;
    io.buf = (void*)valuesToWrite;

    return SendRWBIoctl(io, m_fileDescriptor, m_interfaceName.c_str());
}

bool UnixPciDriver::WriteBlock(DWORD addr, vector<DWORD> values)
{
    char* valuesToWrite = (char*)&values[0];
    DWORD sizeToWrite = values.size() * 4;

    return WriteBlock(addr, sizeToWrite, valuesToWrite);
}

string GetInterfaceNameFromEP(string pciEndPoint)
{
    // const definitions
    static const char* CURRENT_DIRECTORY = ".";
    static const char* PARENT_DIRECTORY = "..";

    stringstream interfaceNameContaingFolder; // path of a folder which contains the interface name of the specific EP
    interfaceNameContaingFolder << PCI_END_POINT_FOLDER << pciEndPoint << "/net";
    DIR* dp = opendir(interfaceNameContaingFolder.str().c_str());
    if (!dp)
    {
        return INVALID;
    }

    // For supporting multi vap (=virtual access point), we get all the interfaces from the same end point, and choose the one without the '-' (which supposed to be the physical interface).
    // If there is more than one interface without '-', we choose the first one.
    // We assume that only virtual interfaces has '-' in their name.
    // TODO : This is kind of a hack until we fully support multi VIF
    dirent* de;
    bool foundInterface = false;
    string interfaceName = INVALID;
    while ((de = readdir(dp)) != NULL)
    {
        if ((strncmp(CURRENT_DIRECTORY, de->d_name, strlen(CURRENT_DIRECTORY)) == 0) || (strncmp(PARENT_DIRECTORY, de->d_name, strlen(PARENT_DIRECTORY)) == 0))
        { // The first two directories that returned from readdir are "." and "..", of course we ignore them
            continue;
        }

        if (strchr(de->d_name,'-') == nullptr)
        { // Interface without '-' means that it is physical interface and not virtual
            LOG_VERBOSE << "Interface name from the directory: " << interfaceNameContaingFolder.str() << " is: " << de->d_name << endl;
            interfaceName = de->d_name;
            break;
        }
        else if (!foundInterface)
        { // In case we can't find an interface name without '-', we take the first one available
            foundInterface = true;
            interfaceName = de->d_name;
        }
    }

    closedir(dp);
    return interfaceName;
}

set<string> GetNetworkInterfaceNames()
{
    set<string> networkInterfaces;

    DIR* dp = opendir(PCI_END_POINT_FOLDER.c_str());
    if (!dp)
    {
        LOG_VERBOSE << "Failed to open PCI EP directories" << endl;
        return networkInterfaces;
    }
    dirent* de;
    while ((de = readdir(dp)) != NULL) // iterate through directory content and search for End Point folders
    {
        string potentialPciEndPoint(de->d_name);
        if (potentialPciEndPoint.find(":") != string::npos) // only PCI end point has ":" in its name
        {
            // Get interface name from End Point
            string networkInterfaceName = GetInterfaceNameFromEP(potentialPciEndPoint);
            if (networkInterfaceName != INVALID)
            {
                networkInterfaces.insert(networkInterfaceName);
            }
            else
            {
                LOG_VERBOSE << "Failed to find interface name for EP: " << potentialPciEndPoint << endl;
            }
        }
    }

    closedir(dp);
    return networkInterfaces;
}

set<string> UnixPciDriver::Enumerate()
{
    set<string> interfacesToCheck = GetNetworkInterfaceNames();

    set<string> connectedInterfaces;
    for (const auto& interfaceName : interfacesToCheck)
    {
        UnixPciDriver pciDriver(interfaceName);
        if (pciDriver.IsReady())
        {
            connectedInterfaces.insert(UnixPciDriver::BuidDeviceName(interfaceName));
        }
    }
    return connectedInterfaces;
}

bool UnixPciDriver::Open()
{
    if(m_initialized)
    {
        return true;
    }

    m_fileDescriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_fileDescriptor < 0)
    {
        // do not report error here, legitimate scenario during enumeration
        return false;
    }

    // Validate interface is 11ad interface
    if(!ValidateInterface())
    {
        Close();
        return false;
    }

    if (!SetDebugFsPath())
    {
        LOG_VERBOSE << "Failed to find debug FS" << endl;
    }

    m_initialized = true;
    return true;
}

bool UnixPciDriver::SetDebugFsPath() // assuming m_interfaceName is set
{
    // const definitions
    static const char* PHY = "phy";

    // find phy number
    stringstream phyContaingFolder; // path of a folder which contains the phy of the specific interface
    phyContaingFolder << "/sys/class/net/" << m_interfaceName << "/device/ieee80211";
    DIR* dp = opendir(phyContaingFolder.str().c_str());
    if (!dp) // ieee80211 doesn't exist, meaning this isn't an 11ad interface
    {
        return false;
    }
    dirent* de;// read phy name (phy folder is named phyX where X is a digit). We assume there is only one folder, if not we take the first one
    do
    {
        de = readdir(dp);
    } while ((de != NULL) && (strncmp(PHY, de->d_name, strlen(PHY)) != 0));
    if (NULL == de)
    {
        closedir(dp);
        return false;
    }

    // find debug FS (using phy name)
    stringstream debugFsPath;
    debugFsPath << "/sys/kernel/debug/ieee80211/" << de->d_name << "/wil6210";
    if(-1 == access(debugFsPath.str().c_str(), F_OK)) // didn't find debug FS
    {
        closedir(dp);
        return false;
    }
    closedir(dp);

    // update debug FS path
    m_debugFsPath = debugFsPath.str();
    return true;
}

bool UnixPciDriver::ReOpen()
{
    return Open();
}

bool UnixPciDriver::AllocPmc(unsigned descSize, unsigned descNum, string& outMessage)
{
    PmcCfg pmcCfg(m_debugFsPath.c_str());
    OperationStatus st = pmcCfg.AllocateDmaDescriptors(descSize, descNum);

    outMessage = st.GetStatusMessage();
    return st.IsSuccess();
}

bool UnixPciDriver::DeallocPmc(bool bSuppressError, std::string& outMessage)
{
    PmcCfg pmcCfg(m_debugFsPath.c_str());
    OperationStatus st = pmcCfg.FreeDmaDescriptors(bSuppressError);

    outMessage = st.GetStatusMessage();
    return st.IsSuccess();
}

bool UnixPciDriver::CreatePmcFiles(unsigned refNumber, std::string& outMessage)
{
    LOG_DEBUG << "Creating PMC files #" << refNumber << std::endl;

    PmcFileWriter pmcFileWriter(refNumber, m_debugFsPath);

    OperationStatus st = pmcFileWriter.WriteFiles();
    outMessage = st.GetStatusMessage();

    if (!st.IsSuccess())
    {
        LOG_ERROR << "Error creating PMC files for #" << refNumber << std::endl;
        return false;
    }

    LOG_DEBUG << "PMC files created. Reported size: " << st.GetStatusMessage() << std::endl;
    return true;
}

bool UnixPciDriver::FindPmcDataFile(unsigned refNumber, std::string& outMessage)
{
    LOG_DEBUG << "Looking for the PMC data file #" << refNumber << endl;

    PmcFileLocator pmcFileLocator(refNumber);

    if (!pmcFileLocator.DataFileExists())
    {
        std::stringstream errorMsgBuilder;
        errorMsgBuilder << "Cannot find PMC data file " << pmcFileLocator.GetDataFileName();
        outMessage = errorMsgBuilder.str();
        return false;
    }

    outMessage = pmcFileLocator.GetDataFileName();
    return true;
}

bool UnixPciDriver::FindPmcRingFile(unsigned refNumber, std::string& outMessage)
{
    LOG_DEBUG << "Looking for the PMC ring file #" << refNumber << endl;

    PmcFileLocator pmcFileLocator(refNumber);

    if (!pmcFileLocator.RingFileExists())
    {
        std::stringstream errorMsgBuilder;
        errorMsgBuilder << "Cannot find PMC ring file " << pmcFileLocator.GetRingFileName();
        outMessage = errorMsgBuilder.str();
        return false;
    }

    outMessage = pmcFileLocator.GetRingFileName();
    return true;
}

void UnixPciDriver::Close()
{
    if (IsReady())
    {
        close(m_fileDescriptor);
        m_fileDescriptor = -1;
    }
}

bool UnixPciDriver::GetDriverMode(int &currentState)
{
    //do something with params
    (void)currentState;

    return false;
}
bool UnixPciDriver::SetDriverMode(int newState, int &oldState)
{
    //do something with params
    (void)newState;
    (void)oldState;
    return false;
}

OperationStatus UnixPciDriver::InterfaceReset()
{
    // For doing an interface reset, ifconfig down and up is done
    // When this method does not work, try reseting FW through a Driver command

    stringstream ss;

    // Interface down:
    ss << "ifconfig " << m_interfaceName << " down";
    ShellCommandExecutor sce(ss.str().c_str());
    if (sce.ExitStatus() != 0)
    {
        stringstream errStr;
        errStr << "Couldn't make '" << ss.str() << "': " << (sce.Output().size() > 0 ? sce.Output().front() : "");
        return OperationStatus(false, errStr.str());
    }

    ss.str(string());

    // Interface up:
    ss << "ifconfig " << m_interfaceName << " up";
    ShellCommandExecutor sce1(ss.str().c_str());
    if (sce1.ExitStatus() == 0)
    {
        LOG_INFO << "Interface reset for " << m_interfaceName << " completed through ifconfig" << endl;
        return OperationStatus(true);
    }
    else
    {
        // print this message only for debug
        LOG_DEBUG << "Couldn't make '" << ss.str() << "': " << (sce1.Output().size() > 0 ? sce1.Output().front() : "") << endl;
    }

    // If we got here, it may be a case of WMI_ONLY FW. Try to reset FW through a Driver command.
    // Note: This will not work when using Debug mailbox or using older driver before adding support for this command.
    return m_driverEventsHandler.TryFwReset();
}

bool UnixPciDriver::RegisterDriverControlEvents()
{
    if (!IsReady())
    {   // shouldn't happen
        LOG_ERROR << "Failed to register for driver events, driver is not ready\n";
        return false;
    }

    return m_driverEventsHandler.RegisterForDriverEvents(m_interfaceName, BuidDeviceName(m_interfaceName));
}

bool UnixPciDriver::DriverControl(uint32_t id,
                                  const void *inBuf, uint32_t inBufSize,
                                  void *outBuf, uint32_t outBufSize,
                                  DWORD* pLastError /*= nullptr*/)
{
    if (!IsReady())
    {   // shouldn't happen
        LOG_ERROR << "Failed to send driver command, driver is not ready\n";
        return false;
    }

    return m_driverEventsHandler.SendDriverCommand(id, inBuf, inBufSize, outBuf, outBufSize, pLastError);
}

bool UnixPciDriver::SendRWIoctl(IoctlIO & io, int fd, const char* interfaceName)
{
    int ret;
    struct ifreq ifr;
    ifr.ifr_data = (char*)&io;

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", interfaceName);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    ret = ioctl(fd, WIL_IOCTL_MEMIO, &ifr); // read/write DWORD
    if (ret < 0)
    {
        LOG_ERROR << __FUNCTION__
                  << " Failed to send RW (dword) IOCTL:"
                  << " Addr: " << Address(io.addr)
                  << " Op: " << io.op
                  << " Error: " << strerror(errno)
                  << std::endl;
        return false;
    }

    return true;
}

bool UnixPciDriver::SendRWBIoctl(IoctlIOBlock & io, int fd, const char* interfaceName)
{
    struct ifreq ifr;
    ifr.ifr_data = (char*)&io;

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", interfaceName);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    int ret = ioctl(fd, WIL_IOCTL_MEMIO_BLOCK, &ifr);  // read/write BYTES. number of bytes must be multiple of 4
    if (ret < 0)
    {
        LOG_ERROR << __FUNCTION__
                  << " Failed to send RW (bytes) IOCTL:"
                  << " Addr: " << Address(io.addr)
                  << " Size: " << io.size
                  << " Op: " << io.op
                  << " Error: " << strerror(errno)
                  << std::endl;

        return false;
    }

    return true;
}

// Receives interface name (wigig#, wlan#) and checks if it is responding
bool UnixPciDriver::ValidateInterface()
{
    IoctlIO io;
    io.addr = BAUD_RATE_REGISTER;
    io.op = EP_OPERATION_READ;

    if(SendRWIoctl(io, m_fileDescriptor, m_interfaceName.c_str()))
    {
        return true;
    }

    return false;
}

bool UnixPciDriver::IsValidInternal()
{
    if(-1 == access(m_debugFsPath.c_str(), F_OK)) // didn't find debug FS
    {
        LOG_WARNING << "No Debug FS found at " << m_debugFsPath << ": PMC functionality is disabled" << std::endl;
    }

    // If we cannot re-enable events transport, the Driver was restarted, mark device invalid.
    // Will do nothing when working with Debug Mailbox.
    return m_driverEventsHandler.ReEnableDriverEventsTransport();
}

string UnixPciDriver::BuidDeviceName(const string& interfaceName)
{
    stringstream ss;
    ss << Utils::PCI << DEVICE_NAME_DELIMITER << interfaceName;
    return ss.str();
}
