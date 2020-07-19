/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_START_DEFERRED_RECORDING_HANDLER_H_
#define _LOG_COLLECTOR_START_DEFERRED_RECORDING_HANDLER_H_
#pragma once

#include "JsonHandlerSDK.h"
#include "LogCollectorJsonValueParser.h"
#include "LogCollectorDefinitions.h"

class LogCollectorStartDeferredRecordingRequest : public JsonDeviceRequest
{
public:
    LogCollectorStartDeferredRecordingRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    JsonValueBoxed<log_collector::RecordingType> GetRecordingTypeBoxed() const
    {
        return LogCollectorJsonValueParser::ParseRecordingType(m_jsonRequestValue, "RecordingType");
    }

};

class LogCollectorStartDeferredRecordingResponse : public JsonResponse
{
public:
    LogCollectorStartDeferredRecordingResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("LogCollectorSetDeferredModeResponse", jsonRequestValue, jsonResponseValue)
    {
    }
};

class LogCollectorDeferredRecordingHandler : public JsonOpCodeHandlerBase<LogCollectorStartDeferredRecordingRequest, LogCollectorStartDeferredRecordingResponse>
{
private:
    void HandleRequest(const LogCollectorStartDeferredRecordingRequest& jsonRequest, LogCollectorStartDeferredRecordingResponse& jsonResponse) override;
};

#endif  // _LOG_COLLECTOR_START_DEFERRED_RECORDING_HANDLER_H_
