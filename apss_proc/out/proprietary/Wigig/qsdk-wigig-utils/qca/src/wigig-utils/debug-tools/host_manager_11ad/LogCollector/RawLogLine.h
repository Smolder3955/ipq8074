/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef RAW_LOG_LINE
#define RAW_LOG_LINE
#pragma once

#include <vector>

namespace log_collector
{
    struct RawLogLine
    {
        enum missing_log_lines_reason {
            NO_MISSED_LOGS = 0,
            RPTR_LARGER_THAN_WPTR,
            BUFFER_OVERRUN,
            INVALID_SIGNATURE,
            DEVICE_WAS_RESTARTED
        };

        RawLogLine(unsigned module, unsigned level, unsigned signature, unsigned strOffset, unsigned isString, const std::vector<int>& params) :
            m_strOffset(strOffset),
            m_module(module),
            m_level(level),
            m_dwordArray(params),
            m_isString(isString),
            m_signature(signature),
            m_numOfMissingLogDwords(0),
            m_missingLogsReason(NO_MISSED_LOGS)
        {}

        RawLogLine(missing_log_lines_reason missingLogLinesReason, unsigned numOfMissingLogLines) :
            m_strOffset(0),
            m_module(0),
            m_level(0),
            m_isString(0),
            m_signature(0),
            m_numOfMissingLogDwords(numOfMissingLogLines),
            m_missingLogsReason(missingLogLinesReason)
        {}

        unsigned m_strOffset;
        unsigned m_module;
        unsigned m_level;
        std::vector<int> m_dwordArray;
        unsigned m_isString;
        unsigned m_signature;
        unsigned m_numOfMissingLogDwords;
        missing_log_lines_reason m_missingLogsReason;
    };
}

#endif // RAW_LOG_LINE
