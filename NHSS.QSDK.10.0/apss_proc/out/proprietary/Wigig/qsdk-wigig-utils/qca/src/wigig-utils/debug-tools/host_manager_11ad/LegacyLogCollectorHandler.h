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

#ifndef _LOG_COLLECTOR_HANDLER_H_
#define _LOG_COLLECTOR_HANDLER_H_
#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

class LegacyLogCollectorHandler final
{
public:

    // singleton
    static LegacyLogCollectorHandler& GetLogCollectorHandler()
    {
        static LegacyLogCollectorHandler logCollectorHandler;
        return logCollectorHandler;
    }

    bool HandleTcpCommand(const std::string& operation, const std::vector<std::string>& parameterList, std::string& statusMessage) const;

    // Non-copyable
    LegacyLogCollectorHandler(const LegacyLogCollectorHandler&) = delete;
    LegacyLogCollectorHandler& operator=(const LegacyLogCollectorHandler&) = delete;

private:
    // typedefs
    typedef std::function<bool(const LegacyLogCollectorHandler&, const std::vector<std::string>& parameters, std::string& statusMessage)> t_handlerFunction;

    // consts
    static const char LIST_DELIMITER;
    static const char DEVICE_RESULT_STRUCT_OPENER;
    static const char DEVICE_RESULT_STRUCT_CLOSER;
    static const char* const UNKNOWN_DEVICE;
    static const char* const SUCCESS;
    static const char* const TRUE_STR; // TODO: move message formating to higher level
    static const char* const FALSE_STR; // TODO: move message formating to higher level
    static const char* const START_COLLECTION;
    static const char* const STOP_COLLECTION;
    static const char* const IS_COLLECTING;
    static const char* const START_RECORDING;
    static const char* const STOP_RECORDING;
    static const char* const IS_RECORDING;
    static const char* const GET_CONFIGURATION;
    static const char* const GET_PARAMETER;
    static const char* const SET_PARAMETER;
    static const char* const SET_DEFERRED_COLLECTION;
    static const char* const SET_DEFERRED_RECORDING;
    static const char* const HELP;
    static const char* const VERSION_NUMBER;

    bool CheckArgumentValidity(const std::vector<std::string> arguments, unsigned numberOfExpectedArguments, std::string& statusMessage) const; // TODO: move this function to utils

                                                                                                                                                //handlers
    bool StartCollection(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool StopCollection(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool IsCollecting(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool StartRecording(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool StopRecording(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool IsRecording(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool GetConfiguration(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool GetParameter(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool SetParameter(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool StartDeferredCollection(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool StartDeferredRecoring(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleHelpCommand(const std::vector<std::string>& parameters, std::string& statusMessage) const;

    const std::unordered_map<std::string, t_handlerFunction> m_handlers = {
        { START_COLLECTION, &LegacyLogCollectorHandler::StartCollection },
        { STOP_COLLECTION, &LegacyLogCollectorHandler::StopCollection },
        { IS_COLLECTING, &LegacyLogCollectorHandler::IsCollecting },
        { START_RECORDING, &LegacyLogCollectorHandler::StartRecording },
        { STOP_RECORDING, &LegacyLogCollectorHandler::StopRecording },
        { IS_RECORDING, &LegacyLogCollectorHandler::IsRecording },
        { GET_CONFIGURATION, &LegacyLogCollectorHandler::GetConfiguration },
        { GET_PARAMETER, &LegacyLogCollectorHandler::GetParameter },
        { SET_PARAMETER, &LegacyLogCollectorHandler::SetParameter },
        { SET_DEFERRED_COLLECTION, &LegacyLogCollectorHandler::StartDeferredCollection },
        { SET_DEFERRED_RECORDING, &LegacyLogCollectorHandler::StartDeferredRecoring },
        { HELP, &LegacyLogCollectorHandler::HandleHelpCommand }
    };

    LegacyLogCollectorHandler() {};
};

#endif // _LOG_COLLECTOR_HANDLER_H_

