/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_GET_STATE_HANDLER_H_
#define _LOG_COLLECTOR_GET_STATE_HANDLER_H_
#pragma once

#include "JsonHandlerSDK.h"
#include "DebugLogger.h"
#include "Host.h"
#include "LogCollectorJsonValueParser.h"

class LogCollectorGetStateRequest : public JsonDeviceRequest
{
public:
    LogCollectorGetStateRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    const JsonValueBoxed<CpuType> GetCpuType() const
    {
        return LogCollectorJsonValueParser::ParseCpuType(m_jsonRequestValue, "CpuType");
    }
};

class LogCollectorGetStateResponse : public JsonResponse
{
public:
    LogCollectorGetStateResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("LogCollectorGetStateResponse", jsonRequestValue, jsonResponseValue)
    {
    }

    void SetIsRecording(bool value)
    {
        m_jsonResponseValue["IsRecording"] = value;
    }

    void SetIsPublishing(bool value)
    {
        m_jsonResponseValue["IsPublishing"] = value;
    }
};

class LogCollectorGetStateHandler : public JsonOpCodeHandlerBase<LogCollectorGetStateRequest, LogCollectorGetStateResponse>
{
private:
    void HandleRequest(const LogCollectorGetStateRequest& jsonRequest, LogCollectorGetStateResponse& jsonResponse) override;
};

#endif// _LOG_COLLECTOR_GET_STATE_HANDLER_H_
