/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_GET_NUM_AVAILABLE_BURSTS_HANDLER_H_
#define _SENSING_GET_NUM_AVAILABLE_BURSTS_HANDLER_H_

#include "JsonHandlerSDK.h"

class SensingGetNumRemainingBurstsResponse final : public JsonResponse
{
public:
    SensingGetNumRemainingBurstsResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("SensingGetNumRemainingBurstsResponse", jsonRequestValue, jsonResponseValue)
    {}

    void SetNumRemainingBursts(uint32_t numRemainingBursts)
    {
        m_jsonResponseValue["NumRemainingBursts"] = numRemainingBursts;
    }
};

class SensingGetNumRemainingBurstsHandler : public JsonOpCodeHandlerBase<Json::Value, SensingGetNumRemainingBurstsResponse>
{
private:
    void HandleRequest(const Json::Value& jsonRequest, SensingGetNumRemainingBurstsResponse& jsonResponse) override;
};

#endif  // _SENSING_GET_NUM_AVAILABLE_BURSTS_HANDLER_H_
