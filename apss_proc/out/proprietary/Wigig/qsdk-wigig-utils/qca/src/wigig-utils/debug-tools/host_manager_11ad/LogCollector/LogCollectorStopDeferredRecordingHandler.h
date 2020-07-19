/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_STOP_DEFERRED_RECORDING_H_
#define _LOG_COLLECTOR_STOP_DEFERRED_RECORDING_H_

#include "JsonHandlerSDK.h"
#include "LogCollectorJsonValueParser.h"


class LogCollectorStopDeferredRecordingRequest : public JsonDeviceRequest
{
public:
    LogCollectorStopDeferredRecordingRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    const JsonValueBoxed<CpuType> GetCpuType() const
    {
        return LogCollectorJsonValueParser::ParseCpuType(m_jsonRequestValue, "CpuType");
    }
};

class LogCollectorStopDeferredRecordingResponse : public JsonResponse
{
public:
    LogCollectorStopDeferredRecordingResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("LogCollectorStopDeferredRecording", jsonRequestValue, jsonResponseValue)
    {
    }
};

class LogCollectorStopDeferredRecordingHandler : public JsonOpCodeHandlerBase<LogCollectorStopDeferredRecordingRequest, LogCollectorStopDeferredRecordingResponse>
{
private:
    void HandleRequest(const LogCollectorStopDeferredRecordingRequest& jsonRequest, LogCollectorStopDeferredRecordingResponse& jsonResponse) override;
};

#endif  // _LOG_COLLECTOR_STOP_DEFERRED_RECORDING_H_
