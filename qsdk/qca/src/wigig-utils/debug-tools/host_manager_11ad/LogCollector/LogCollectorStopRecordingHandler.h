/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_STOP_RECORDING_H_
#define _LOG_COLLECTOR_STOP_RECORDING_H_

#include "JsonHandlerSDK.h"
#include "LogCollectorJsonValueParser.h"


class LogCollectorStopRecordingRequest : public JsonDeviceRequest
{
public:
    LogCollectorStopRecordingRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    const JsonValueBoxed<CpuType> GetCpuType() const
    {
        return LogCollectorJsonValueParser::ParseCpuType(m_jsonRequestValue, "CpuType");
    }
};

class LogCollectorStopRecordingResponse : public JsonResponse
{
public:
    LogCollectorStopRecordingResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("LogCollectorStopRecordingResponse", jsonRequestValue, jsonResponseValue)
    {
    }
    void SetRecordingPath(const std::string & recPath)
    {
        m_jsonResponseValue["RecordingPath"] = recPath;
    }
};

class LogCollectorStopRecordingHandler : public JsonOpCodeHandlerBase<LogCollectorStopRecordingRequest, LogCollectorStopRecordingResponse>
{
private:
    void HandleRequest(const LogCollectorStopRecordingRequest& jsonRequest, LogCollectorStopRecordingResponse& jsonResponse) override;
};

#endif  // _LOG_COLLECTOR_STOP_RECORDING_H_
