/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_GET_PARAMETERS_HANDLER_H_
#define _SENSING_GET_PARAMETERS_HANDLER_H_

#include <string>
#include "JsonHandlerSDK.h"

class SensingGetParametersResponse final : public JsonResponse
{
public:
    SensingGetParametersResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("SensingGetParametersResponse", jsonRequestValue, jsonResponseValue)
    {}

    // Passed parameters string is expected to be encoded Base64
    // JsonValue does not support moving, pass by const reference
    void SetParametersBase64(const std::string& parametersBase64)
    {
        m_jsonResponseValue["ParametersBase64"] = parametersBase64;
    }
};

class SensingGetParametersHandler : public JsonOpCodeHandlerBase<Json::Value, SensingGetParametersResponse>
{
private:
    void HandleRequest(const Json::Value& jsonRequest, SensingGetParametersResponse& jsonResponse) override;
};

#endif  // _SENSING_GET_PARAMETERS_HANDLER_H_
