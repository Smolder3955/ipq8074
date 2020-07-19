/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_HANDLER_
#define _LOG_HANDLER_

#include <iostream>
#include <iomanip>
#include <stdio.h>

#ifdef _WINDOWS
    #pragma warning(push)
    #pragma warning(disable: 4702) // suppress false unreachable code warning
#endif

// Severity values are expected to raise from zero
enum LogSeverity
{
    LOG_SEV_ERROR = 0,   // Unexpected input/events that may cause server misbehavior
    LOG_SEV_WARNING = 1,   // Tcp messages
    LOG_SEV_INFO = 2,   // Stream stats
    LOG_SEV_DEBUG = 3,   // Message to send
    LOG_SEV_VERBOSE = 4,   // Excessive debug
};

#define TRACE_WITH_PREFIX(SEV)                                          \
    g_LogConfig.ShouldPrint(SEV) && std::cout << "["<< g_LogConfig.SeverityToString(SEV) << "]"


#define LOG_ERROR   TRACE_WITH_PREFIX(LOG_SEV_ERROR)
#define LOG_WARNING TRACE_WITH_PREFIX(LOG_SEV_WARNING)
#define LOG_INFO    TRACE_WITH_PREFIX(LOG_SEV_INFO)
#define LOG_DEBUG   TRACE_WITH_PREFIX(LOG_SEV_DEBUG)
#define LOG_VERBOSE TRACE_WITH_PREFIX(LOG_SEV_VERBOSE)



// Decoupling from system assert allows to print an error message when
// assert is disabled.
#define LOG_ASSERT(CONDITION)                                           \
    do {                                                                \
        if (!(CONDITION)) {                                             \
            LOG_ERROR << "ASSERTION FAILURE: " << #CONDITION            \
                << " at " << __FILE__ << ':' << __LINE__                \
                << std::endl;                                           \
                if (g_LogConfig.ShouldExitOnAssert()) exit(1);          \
        }                                                               \
    } while (false)



class LogConfig
{
public:
    LogConfig() { SetMaxSeverity(0); };
    LogSeverity m_MaxSeverity;
    inline bool ShouldPrint(LogSeverity sev) const { return sev <= m_MaxSeverity; }
    inline void SetMaxSeverity(int traceLevel)
    {
        if (traceLevel > LOG_SEV_VERBOSE)
        {
            fprintf(stderr, "Invalid trace level, setting %d\n", LOG_SEV_VERBOSE);
            m_MaxSeverity = LOG_SEV_VERBOSE;
        }
        else
        {
            m_MaxSeverity = static_cast<LogSeverity>(traceLevel);
        }
    }

    inline const char* SeverityToString(LogSeverity sev)
    {
        static const char* const pSeverityToString[] = { "ERR", "WRN", "INF", "DBG", "VRB" };

        size_t index = static_cast<size_t>(sev);
        if (index >= sizeof(pSeverityToString) / sizeof(pSeverityToString[0]))
        {
            return "---";
        }

        return pSeverityToString[index];
    }
};

extern LogConfig g_LogConfig;

extern char* g_ApplicationName;

#endif