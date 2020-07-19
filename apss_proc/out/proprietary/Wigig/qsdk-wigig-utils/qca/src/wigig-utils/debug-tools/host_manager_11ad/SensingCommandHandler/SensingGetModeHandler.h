/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_GET_MODE_HANDLER_H_
#define _SENSING_GET_MODE_HANDLER_H_

#include <string>
#include "JsonHandlerSDK.h"

class SensingGetModeResponse final : public JsonResponse
{
public:
    SensingGetModeResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("SensingGetModeResponse", jsonRequestValue, jsonResponseValue)
    {}

    void SetMode(const std::string& modeStr)
    {
        m_jsonResponseValue["Mode"] = modeStr;
    }
};

class SensingGetModeHandler : public JsonOpCodeHandlerBase<Json::Value, SensingGetModeResponse>
{
private:
    void HandleRequest(const Json::Value& jsonRequest, SensingGetModeResponse& jsonResponse) override;
};

#endif // _SENSING_GET_MODE_HANDLER_H_
