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

#pragma once

#include "DebugLogger.h"

#define LOG_MESSAGE( level, format, ...) \
    {\
        CDebugLogger::S_LOG_CONTEXT _logContext = { _T(__FUNCTION__), _T(__FILE__), __LINE__ }; \
        CDebugLogger::LogGlobalDebugMessage( level, &_logContext, format,##__VA_ARGS__ );\
    }

#define LOG_MESSAGE_ERROR( format,...)    LOG_MESSAGE(COsDebugLogger::E_LOG_LEVEL_ERROR, format,##__VA_ARGS__ )
#define LOG_MESSAGE_WARN( format,...)    LOG_MESSAGE(COsDebugLogger::E_LOG_LEVEL_WARN, format,##__VA_ARGS__ )
#define LOG_MESSAGE_INFO( format,...)    LOG_MESSAGE(COsDebugLogger::E_LOG_LEVEL_INFO, format,##__VA_ARGS__ )
#define LOG_MESSAGE_DEBUG( format,...)    LOG_MESSAGE(COsDebugLogger::E_LOG_LEVEL_DEBUG, format,##__VA_ARGS__ )
#define LOG_MESSAGE_TRACE( format,...)    LOG_MESSAGE(COsDebugLogger::E_LOG_LEVEL_TRACE, format,##__VA_ARGS__ )
#define LOG_MESSAGE_VERBOSE( format,...)    LOG_MESSAGE(COsDebugLogger::E_LOG_LEVEL_VERBOSE, format,##__VA_ARGS__ )
#define LOG_STACK_ENTER LOG_MESSAGE(COsDebugLogger::E_LOG_LEVEL_VERBOSE, _T("Enter."))
#define LOG_STACK_LEAVE LOG_MESSAGE(COsDebugLogger::E_LOG_LEVEL_VERBOSE, _T("Leave."))
