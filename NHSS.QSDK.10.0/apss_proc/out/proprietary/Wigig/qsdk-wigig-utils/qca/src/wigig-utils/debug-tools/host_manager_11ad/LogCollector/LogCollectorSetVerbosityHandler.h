/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_SET_VERBOSITY_HANDLER_H_
#define _LOG_COLLECTOR_SET_VERBOSITY_HANDLER_H_
#pragma once

#include "JsonHandlerSDK.h"
#include "DebugLogger.h"
#include "LogCollectorDefinitions.h"
#include "LogCollectorJsonValueParser.h"

class LogCollectorSetVerbosityRequest : public JsonDeviceRequest
{
public:
    LogCollectorSetVerbosityRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue),
        m_defaultVerbosity(JsonValueParser::ParseStringValue(jsonRequestValue, "DefaultVerbosity"))
    {
    }

    const JsonValueBoxed<CpuType> GetCpuType() const
    {
        return LogCollectorJsonValueParser::ParseCpuType(m_jsonRequestValue, "CpuType");
    }

    const JsonValueBoxed<std::string>& GetDefaultVerbosity() const
    {
        return m_defaultVerbosity;
    }

    const JsonValueBoxed<std::string> GetModuleVerbosity(const std::string& module) const
    {
        return JsonValueParser::ParseStringValue(m_jsonRequestValue, module.c_str());
    }
private:
    JsonValueBoxed<std::string> m_defaultVerbosity;
};

class LogCollectorSetVerbosityResponse : public JsonResponse
{
public:
    LogCollectorSetVerbosityResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("LogCollectorSetVerbosityResponse", jsonRequestValue, jsonResponseValue)
    {
    }
};

class LogCollectorSetVerbosityHandler : public JsonOpCodeHandlerBase<LogCollectorSetVerbosityRequest, LogCollectorSetVerbosityResponse>
{
private:
    void HandleRequest(const LogCollectorSetVerbosityRequest& jsonRequest, LogCollectorSetVerbosityResponse& jsonResponse) override;
};

#endif  // _LOG_COLLECTOR_SET_VERBOSITY_HANDLER_H_
