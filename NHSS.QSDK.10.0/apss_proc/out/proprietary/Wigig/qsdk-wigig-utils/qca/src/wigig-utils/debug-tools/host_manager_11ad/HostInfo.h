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

#ifndef _HOSTINFO_H_
#define _HOSTINFO_H_

#include <string>
#include <atomic>
#include <mutex>
#include <set>

#include "HostDefinitions.h"

class HostInfo
{

public:

    /*
    HostInfo
    Initializes all hostInfo members (ips and alias)
    */
    HostInfo();

    /*
    GetIps
    Returns host's Ips (private and broadcast)
    @param: none
    @return: host's ips
    */
    const HostIps& GetIps() const;

    /*
    GetHostAlias
    Returns host's alias (if one was given by the user)
    @param: none
    @return: host's alias if exists, otherwise empty string
    */
    std::string GetAlias();

    /*
    SaveAliasToFile
    Gets a new alias for the host and saves it to a configuration file.
    This function is called from CommandsTcpServer only. It turns on a flag so that the UDP server (that is on a different thread) will check for the new alias
    @param: string - the new alias
    @return: bool - operation status - true for success, false otherwise
    */
    bool SaveAliasToFile(const std::string& newAlias);

    /*
    UpdateAliasFromFile
    Updates the m_alias member with the host's alias from 11ad's persistency
    @param: none
    @return: bool - operation status - true for success, false otherwise
    */
    bool UpdateAliasFromFile();

    /*
    GetConnectedUsers
    Returns a set of all the users that are connected to this host
    @param: none
    @return: set<string> of all connected users
    */
    std::set<std::string> GetConnectedUsers() const;

    /*
    AddNewConnectedUser
    Adds a user to the connected user's list
    @param: string - a new user (currently the user's DmTools's IP. TODO: change to the user's personal host's name or user's DmTools username)
    @return: none
    */
    void AddNewConnectedUser(const std::string& user);

    /*
    RemoveConnectedUser
    Removes a user from the connected user's list
    @param: string - a user (currently the user's DmTools's IP. TODO: change to the user's personal host's name or user's DmTools username)
    @return: none
    */
    void RemoveConnectedUser(const std::string& user);

    static std::string GetOSName() { return s_osName; }

    /*
    GetHostCapabilities
    Get the capabilities of the host
    @param: none
    @return: bit mask of the capabilities
    */
    DWORD GetHostCapabilities() const { return m_capabilitiesMask; }

private:

    HostIps m_ips; // host's network details // assumption: each host has only one IP address for Ethernet interfaces
    std::string m_alias; // host's alias (given by user)
    const std::string m_persistencyPath; // host server's files location
    const std::string m_aliasFileName; // host's alias file
    const std::string m_oldHostAliasFile; // old location of the host alias
    std::set<std::string> m_connectedUsers; // list of users IPs that have a connection to the commandsTcpServer // TODO: change to the user's personal host's name or user's DmTools username
    const static std::string s_osName;  // current OS name
    std::atomic<bool> m_isAliasFileChanged; // when turned on indicates that m_alias contains stale information, so we need to update it from persistency
    mutable std::mutex m_persistencyLock; // only one thread is allowed to change persistency at a time
    mutable std::mutex m_connectedUsersLock;

    // Capabilities:
    DWORD m_capabilitiesMask;

    // Enumeration of Host capabilities
    // Note: This is a contract with DmTools, the order is important!
    enum CAPABILITIES : DWORD
    {
        COLLECTING_LOGS = 0, // capability of host manager to collect logs by itself and send them to DmTools
        KEEP_ALIVE,          // capability of host manager to send keep-alive event to enable watchdog on the client side
        READ_BLOCK_FAST,     // host manager supports rb_fast command
    };

    void SetHostCapabilities();
    void SetCapability(CAPABILITIES capability, bool isTrue);
    bool IsCapabilitySet(CAPABILITIES capability) const;

    // load host's info from persistency and ioctls
    void LoadHostInfo();

    // create all directories required for host manager
    void CreateHostDirectories();

    // For backward compatibility (should be removed in next releases)
    const std::string GetOldPersistencyLocation();
};


#endif
