/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _PMC_VERSION_HANDLER_H_
#define _PMC_VERSION_HANDLER_H_

#include "JsonHandlerSDK.h"

class PmcVersionResponse: public JsonResponse
{
public:
    PmcVersionResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue)
        : JsonResponse("PmcVersionResponse", jsonRequestValue, jsonResponseValue)
    {
        m_jsonResponseValue["Version"] = "2.0";
    }
};

class PmcVersionHandler : public JsonOpCodeHandlerBase<Json::Value, PmcVersionResponse>
{
private:
    void HandleRequest(const Json::Value& jsonRequest, PmcVersionResponse& jsonResponse) override
    {
        (void)jsonRequest;
        (void)jsonResponse;

    }
};

#endif// _PMC_VERSION_HANDLER_H_
