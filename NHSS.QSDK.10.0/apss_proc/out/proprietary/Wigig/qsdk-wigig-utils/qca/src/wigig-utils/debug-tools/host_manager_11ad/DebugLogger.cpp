/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
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
#include "Utils.h"

#include <stdio.h>

// *************************************************************************************************

LogConfig g_LogConfig(LOG_SEV_INFO, false, true);

// *************************************************************************************************

LogConfig::LogConfig(LogSeverity maxSeverity, bool bPrintLocation, bool bPrintTimestamp)
    : m_MaxSeverity(maxSeverity)

    , m_PrintLocation(bPrintLocation)
    , m_PrintTimestamp(bPrintTimestamp)
    , m_ExitOnAssert(false)
{
}

void LogConfig::SetMaxSeverity(int traceLevel)
{
    if (traceLevel > LOG_SEV_VERBOSE)
    {
        fprintf(stderr, "Invalid trace level, setting %d\n", LOG_SEV_VERBOSE);
        m_MaxSeverity = LOG_SEV_VERBOSE;
    }
    else
    {
        m_MaxSeverity = static_cast<LogSeverity>(traceLevel);
        fprintf(stdout, "Setting trace level to %d\n", m_MaxSeverity);
    }
}

// *************************************************************************************************

const char* LogMsgPrefix::SeverityToString(LogSeverity sev)
{
    static const char* const pSeverityToString[] = { "ERR", "WRN", "INF", "DBG", "VRB" };

    size_t index = static_cast<size_t>(sev);
    if (index >= sizeof(pSeverityToString) / sizeof(pSeverityToString[0]))
    {
        return "---";
    }

    return pSeverityToString[index];
}

std::ostream& operator<<(std::ostream& os, const LogMsgPrefix& prefix)
{
    os << '[' << LogMsgPrefix::SeverityToString(prefix.Severity) << "] ";
    if (g_LogConfig.ShouldPrintLocation())
    {
        os << "(" << prefix.File << ':' << prefix.Line << ") ";
    }

    if (g_LogConfig.ShouldPrintTimestamp())
    {
        os << "{" <<  Utils::GetCurrentLocalTimeString() << "} ";
    }

    return os;
}
