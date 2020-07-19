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
#include <vector>
#include <string>
#include <algorithm>

#include "ArgumentsParser.h"
#include "SharedVersionInfo.h"
#include "Host.h"
#include "DebugLogger.h"

using namespace std;
// *************************************************************************************************

bool ArgumentsParser::ParseAndHandleArguments(int argc, char * argv[])
{
    for (int i = 0; i < argc; i++)
    {
        m_arguments.emplace_back(argv[i]);
    }

    if (DoesArgumentExist("-v"))
    {
        // Argument for the version of host_manager_11ad. No need to run host manager
        std::cout << "802.11ad Host Manager: " <<  GetToolsVersion() << std::endl;
        std::cout << GetToolsBuildInfo() << std::endl;
        return false;
    }

    unsigned val;
    if (GetArgumentValue("-d", val))
    {
        //Argument for setting verbosity level
        g_LogConfig.SetMaxSeverity(val);
    }

    if (GetArgumentValueStr("-c", m_customConfigFile))
    {
        LOG_INFO << "Custom configuration file provided: " << m_customConfigFile << std::endl;
    }

    return true;
}

bool ArgumentsParser::DoesArgumentExist(const string& option)
{
    const bool doesArgumentExist = find(m_arguments.begin(), m_arguments.end(), option) != m_arguments.end();
    return doesArgumentExist;
}

bool ArgumentsParser::GetArgumentValue(const string& option, unsigned& val)
{
    auto argumentIter = find(m_arguments.begin(), m_arguments.end(), option);
    if (argumentIter != m_arguments.end())
    {
        ++argumentIter;
        if (argumentIter != m_arguments.end())
        {
            const char* valStr = (*argumentIter).c_str();
            char* end;
            val = strtoul(valStr, &end, 10);
            if (end == valStr)
            {
                LOG_WARNING << "Failed to parse input argument '" << option << "', value is '"<< valStr << "'" << endl;
                return false;
            }

            return true;
        }
    }
    // else given option wasn't passed

    return false;
}

bool ArgumentsParser::GetArgumentValueStr(const string& option, string& val)
{
    auto argumentIter = find(m_arguments.begin(), m_arguments.end(), option);
    if (argumentIter != m_arguments.end())
    {
        ++argumentIter;
        if (argumentIter != m_arguments.end())
        {
            val = *argumentIter;
            return true;
        }
    }

    // else given option wasn't passed
    return false;
}
