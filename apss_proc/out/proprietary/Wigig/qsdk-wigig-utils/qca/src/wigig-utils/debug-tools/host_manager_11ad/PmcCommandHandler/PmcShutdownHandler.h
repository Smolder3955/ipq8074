/*
* Copyright (c) 2017 - 2018 Qualcomm Technologies, Inc.
*
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#pragma once

#include "JsonHandlerSDK.h"
#include "PmcJsonResponse.h"
#include "DebugLogger.h"
#include "OperationStatus.h"

class PmcShutdownRequest : public JsonDeviceRequest
{
public:
    PmcShutdownRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }
};

class PmcShutdownResponse : public PmcJsonResponse
{
public:
    PmcShutdownResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        PmcJsonResponse("PmcShutdownResponse", jsonRequestValue, jsonResponseValue)
    {
    }
};

class PmcShutdownHandler : public JsonOpCodeHandlerBase<PmcShutdownRequest, PmcShutdownResponse>
{
private:
    void HandleRequest(const PmcShutdownRequest& jsonRequest, PmcShutdownResponse& jsonResponse) override;
};

