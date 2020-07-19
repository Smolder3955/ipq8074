/*
* Copyright (c) 2017-2019 Qualcomm Technologies, Inc.
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

#include <memory>
#include <string>

#include "SharedVersionInfo.h"
#include "OsHandler.h"
#include "ArgumentsParser.h"
#include "DebugLogger.h"
#include "Host.h"
#include "TcpNetworkInterface.h"

using namespace std;

// *************************************************************************************************


bool Is3pp(const std::string& programName)
{
    const bool logCollectorTracer3ppLogsFilterOn = programName.find("3pp") != std::string::npos;
    if (logCollectorTracer3ppLogsFilterOn)
    {
        LOG_VERBOSE << "LogCollector Tracer 3pp LogsFilter is On" << std::endl;
    }
    else
    {
        LOG_VERBOSE << "LogCollector Tracer 3pp LogsFilter is Off, programName is:" << programName << std::endl;
    }
    return logCollectorTracer3ppLogsFilterOn;
}


int main(int argc, char* argv[])
{
    try
    {
        unique_ptr<ArgumentsParser> pArgumentsParser(new ArgumentsParser());
        bool continueRunningHostManager = pArgumentsParser->ParseAndHandleArguments(argc, argv);

        if (continueRunningHostManager)
        {
            LOG_INFO << "802.11ad Host Manager: " << GetToolsVersion() << endl;
            LOG_DEBUG << GetToolsBuildInfo() << endl;

            //Handle OS specific configurations
            unique_ptr<OsHandler> pOsHandler(new OsHandler());
            pOsHandler->HandleOsSignals();

            if (!TcpNetworkInterfaces::NetworkInterface::InitializeNetwork())
            {
                return 1;
            }

            const bool logCollectorTracer3ppLogsFilterOn = Is3pp(std::string(argv[0]));

            //Start Host object
            Host::GetHost().StartHost(logCollectorTracer3ppLogsFilterOn, *pArgumentsParser);

            TcpNetworkInterfaces::NetworkInterface::FinalizeNetwork();

            LOG_INFO << "Stopping 802.11ad Host Manager" << endl;
        }

    }
    catch (exception& e)
    {
        LOG_ERROR << "Stopping host_manager_11ad due to failure: " << e.what() << endl;
    }

    return 0;


}
