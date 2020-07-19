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

#include <iostream>

#include "HostInfo.h"
#include "DebugLogger.h"
#include "FileSystemOsAbstraction.h"

using namespace std;

// Initialize the OS name according to OS specific macros
const string HostInfo::s_osName =
//should check __ANDROID__ first since __LINUX flag also set in Android
#ifdef __ANDROID__
    "Android"
#elif __linux
    "Linux"
#elif _WINDOWS
    "Windows"
#else //OS3
    "NA"
#endif
;

HostInfo::HostInfo() :
    m_alias(""),
    m_persistencyPath(FileSystemOsAbstraction::GetConfigurationFilesLocation()),
    m_aliasFileName("host_alias"),
    m_oldHostAliasFile(GetOldPersistencyLocation() + "host_manager_11ad_host_info"),
    m_isAliasFileChanged(false),
    m_capabilitiesMask(0)
{
    CreateHostDirectories();
    SetHostCapabilities();
    LoadHostInfo();
}

const string HostInfo::GetOldPersistencyLocation()
{
#ifdef __linux
    return "/etc/";
#elif __OS3__
    return "/tmp/";
#else
    return "C:\\Temp\\"; // TODO: change
#endif // __linux
}

const HostIps& HostInfo::GetIps() const
{
    return m_ips;
}

string HostInfo::GetAlias()
{
    if (m_isAliasFileChanged)
    {
        UpdateAliasFromFile();
    }
    return m_alias;
}

bool HostInfo::SaveAliasToFile(const string& newAlias)
{
    lock_guard<mutex> lock(m_persistencyLock);
    if (!FileSystemOsAbstraction::WriteFile(m_persistencyPath + m_aliasFileName, newAlias))
    {
        LOG_WARNING << "Failed to write new alias to configuration file " << m_persistencyPath << m_aliasFileName << endl;
        return false;
    }
    m_isAliasFileChanged = true;
    return true;
}

bool HostInfo::UpdateAliasFromFile()
{
    lock_guard<mutex> lock(m_persistencyLock);
    if (!FileSystemOsAbstraction::ReadFile(m_persistencyPath + m_aliasFileName, m_alias))
    {
        LOG_WARNING << "Failed to write new alias to configuration file " << m_persistencyPath << m_aliasFileName << endl;
        return false;
    }
    m_isAliasFileChanged = false;
    return true;
}

set<string> HostInfo::GetConnectedUsers() const
{
    lock_guard<mutex> lock(m_connectedUsersLock);
    return m_connectedUsers;
}

void HostInfo::AddNewConnectedUser(const string& user)
{
    lock_guard<mutex> lock(m_connectedUsersLock);
    m_connectedUsers.insert(user);
}

void HostInfo::RemoveConnectedUser(const string& user)
{
    lock_guard<mutex> lock(m_connectedUsersLock);
    m_connectedUsers.erase(user);
}

void HostInfo::SetHostCapabilities()
{
    SetCapability(COLLECTING_LOGS, true);
    SetCapability(KEEP_ALIVE, true);
    SetCapability(READ_BLOCK_FAST, true);
}

void HostInfo::SetCapability(CAPABILITIES capability, bool isTrue)
{
    const DWORD mask = (DWORD)1 << capability;
    m_capabilitiesMask = isTrue ? m_capabilitiesMask | mask : m_capabilitiesMask & ~mask;
}

bool HostInfo::IsCapabilitySet(CAPABILITIES capability) const
{
    return (m_capabilitiesMask & (DWORD)1 << capability) != (DWORD)0;
}

void HostInfo::CreateHostDirectories()
{
    // configuration folder
    if (!FileSystemOsAbstraction::DoesFolderExist(m_persistencyPath))
    {
        if (!FileSystemOsAbstraction::CreateFolder(m_persistencyPath))
        {
            LOG_WARNING << "Failed to create persistencyPath " << m_persistencyPath << " directory" << endl;
        }
    }

    // output files folder
    if (!FileSystemOsAbstraction::DoesFolderExist(FileSystemOsAbstraction::GetOutputFilesLocation()))
    {
        if (!FileSystemOsAbstraction::CreateFolder(FileSystemOsAbstraction::GetOutputFilesLocation()))
        {
            LOG_WARNING << "Failed to create OutputFilesLocation " << FileSystemOsAbstraction::GetOutputFilesLocation() << " directory" << endl;
        }
    }
}

void HostInfo::LoadHostInfo()
{
    m_ips = FileSystemOsAbstraction::GetHostIps();

    if (!FileSystemOsAbstraction::DoesFolderExist(m_persistencyPath))
    {
        LOG_WARNING << "Failed to load host info since " << m_persistencyPath << " directory was not found" << endl;
        return;
    }
    // backward compatibility - copy the alias that the user already given to its new place
    if (FileSystemOsAbstraction::DoesFileExist(m_oldHostAliasFile)
        && !FileSystemOsAbstraction::MoveFileToNewLocation(m_oldHostAliasFile, m_persistencyPath))
    {
        LOG_WARNING << "Failed to move " << m_oldHostAliasFile << " file to new location " << m_persistencyPath + m_aliasFileName << endl;
        return;
    }

    bool res = FileSystemOsAbstraction::ReadFile(m_persistencyPath + m_aliasFileName, m_alias);
    if (!res) // file doesn't exist
    {
        res = FileSystemOsAbstraction::ReadHostOsAlias(m_alias);
        if (!res)
        {
            LOG_WARNING << "Failed to read OS host name" << endl;
            m_alias = "";
        }
        res = FileSystemOsAbstraction::WriteFile(m_persistencyPath + m_aliasFileName, m_alias);
        if (!res)
        {
            LOG_WARNING << "Failed to write host alias to persistency" << endl;
        }
    }
}
