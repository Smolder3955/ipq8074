/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _PMC_INIT_HANDLER_H_
#define _PMC_INIT_HANDLER_H_

#include "JsonHandlerSDK.h"
#include "PmcJsonResponse.h"
#include "DebugLogger.h"
#include "OperationStatus.h"

class PmcInitRequest: public JsonDeviceRequest
{
    public:
        PmcInitRequest(const Json::Value& jsonRequestValue) :
                JsonDeviceRequest(jsonRequestValue)
        {
        }
};

class PmcInitResponse: public PmcJsonResponse
{
    public:
        PmcInitResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
                PmcJsonResponse("PmcInitResponse", jsonRequestValue, jsonResponseValue)
        {
        }
};

class PmcInitHandler: public JsonOpCodeHandlerBase<PmcInitRequest, PmcInitResponse>
{
    private:
        void HandleRequest(const PmcInitRequest& jsonRequest, PmcInitResponse& jsonResponse) override;
};

#endif  // _PMC_INIT_HANDLER_H_
