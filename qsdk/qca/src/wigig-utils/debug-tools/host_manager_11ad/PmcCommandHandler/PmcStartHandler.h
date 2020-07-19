/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _PMC_START_HANDLER_H_
#define _PMC_START_HANDLER_H_

#include "JsonHandlerSDK.h"
#include "PmcJsonResponse.h"
#include "DebugLogger.h"

class PmcStartRequest: public JsonDeviceRequest
{
    public:
        PmcStartRequest(const Json::Value& jsonRequestValue) :
                JsonDeviceRequest(jsonRequestValue)
        {
        }
};

class PmcStartResponse: public PmcJsonResponse
{
    public:
        PmcStartResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
                PmcJsonResponse("PmcStartResponse", jsonRequestValue, jsonResponseValue)
        {
        }
        void SetImplicitInit(bool implicitInit)
        {
            m_jsonResponseValue["ImplicitInit"] = implicitInit;
        }
};

class PmcStartHandler: public JsonOpCodeHandlerBase<PmcStartRequest, PmcStartResponse>
{
    private:
        void HandleRequest(const PmcStartRequest& jsonRequest, PmcStartResponse& jsonResponse) override;
};

#endif  // _PMC_START_HANDLER_H_
