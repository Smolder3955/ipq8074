/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_VERSION_HANDLER_H_
#define _LOG_COLLECTOR_VERSION_HANDLER_H_
#pragma once

#include "JsonHandlerSDK.h"

class LogCollectorVersionResponse : public JsonResponse
{
public:
    LogCollectorVersionResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue)
        : JsonResponse("LogCollectorVersionResponse", jsonRequestValue, jsonResponseValue)
    {
        m_jsonResponseValue["Version"] = "2.1";
    }
};

class LogCollectorVersionHandler : public JsonOpCodeHandlerBase<Json::Value, LogCollectorVersionResponse>
{
private:
    void HandleRequest(const Json::Value& jsonRequest, LogCollectorVersionResponse& jsonResponse) override
    {
        (void)jsonRequest;
        (void)jsonResponse;
    }
};

#endif// _LOG_COLLECTOR_VERSION_HANDLER_H_