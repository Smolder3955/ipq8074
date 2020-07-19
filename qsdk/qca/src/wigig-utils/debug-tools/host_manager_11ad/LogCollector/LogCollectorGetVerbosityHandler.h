/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_GET_VERBOSITY_HANDLER_H_
#define _LOG_COLLECTOR_GET_VERBOSITY_HANDLER_H_
#pragma once

#include "JsonHandlerSDK.h"
#include "DebugLogger.h"
#include "LogCollectorJsonValueParser.h"

class LogCollectorGetVerbosityRequest : public JsonDeviceRequest
{
public:
    LogCollectorGetVerbosityRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    const JsonValueBoxed<CpuType> GetCpuType() const
    {
        return LogCollectorJsonValueParser::ParseCpuType(m_jsonRequestValue, "CpuType");
    }
};

class LogCollectorGetVerbosityResponse : public JsonResponse
{
public:
    LogCollectorGetVerbosityResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("LogCollectorGetVerbosityResponse", jsonRequestValue, jsonResponseValue)
    {
    }

    void SetModuleVerbosity(const std::string& module, const std::string& value)
    {
        m_jsonResponseValue[module] = value;
    }
};

class LogCollectorGetVerbosityHandler : public JsonOpCodeHandlerBase<LogCollectorGetVerbosityRequest, LogCollectorGetVerbosityResponse>
{
private:
    void HandleRequest(const LogCollectorGetVerbosityRequest& jsonRequest, LogCollectorGetVerbosityResponse& jsonResponse) override;
};

#endif  // _LOG_COLLECTOR_GET_VERBOSITY_HANDLER_H_

