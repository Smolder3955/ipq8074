/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "RtlSimEnumerator.h"
#include "DebugLogger.h"
#include "Utils.h"

#include <sstream>
#include <set>
#include <string>

#include <pwd.h> // for getting home directory
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

using namespace std;

#ifndef DEVICE_NAME_DELIMITER
    #define DEVICE_NAME_DELIMITER '!'
#endif

// *************************************************************************************************

std::string RtlSimEnumerator::GetRtlDirectory()
{
    // RTL Simulator creates a device directory in ~DMTOOLS/RTL0
    const char *homedir = nullptr;
    if ((homedir = getenv("HOME")) == nullptr)
    {
        homedir = getpwuid(getuid())->pw_dir;
    }

    return std::string(homedir) + "/DMTOOLS/";
}

// *************************************************************************************************

std::set<std::string> RtlSimEnumerator::GetRtlInterfaceNames()
{
    std::set<std::string> rtlInterfaces;

    DIR* dp = opendir(GetRtlDirectory().c_str());
    if (!dp)
    {
        LOG_VERBOSE << "Failed to open RTL simulation directory " << GetRtlDirectory() << " Error: " << strerror(errno) << endl;
        return rtlInterfaces;
    }

    dirent* de;
    while ((de = readdir(dp)) != NULL) // iterate through directory content and search for End Point folders
    {
        string potentialRtlEndPoint(de->d_name);
        if (potentialRtlEndPoint.find(Utils::RTL) == 0) // only directories that start with RTL are eligible for being a device
        {
            struct stat statbuf;
            if (stat((GetRtlDirectory() + potentialRtlEndPoint).c_str(), &statbuf) == 0)
            {
                if (S_ISDIR(statbuf.st_mode))
                {
                    rtlInterfaces.insert(potentialRtlEndPoint);
                    LOG_VERBOSE << "Found the interface name: " << potentialRtlEndPoint << endl;
                }
                else
                {
                    LOG_VERBOSE << "Found a directory with prefix RTL which is a file and not a directory" << endl;
                }
            }
            else
            {
                LOG_VERBOSE << "There was en error with stat operation on enumerate function" << endl;
            }
        }
    }

    closedir(dp);
    return rtlInterfaces;
}

// *************************************************************************************************

set<string> RtlSimEnumerator::Enumerate()
{
    std::set<string> interfacesToCheck = GetRtlInterfaceNames();

    std::set<string> connectedInterfaces;
    for (const auto& interfaceName : interfacesToCheck)
    {
        std::stringstream ss;
        ss << Utils::RTL << DEVICE_NAME_DELIMITER << interfaceName;

        connectedInterfaces.insert(ss.str());
        LOG_DEBUG << "Found RTL Simulator interface: " << interfaceName << std::endl;
    }

    return connectedInterfaces;
}
