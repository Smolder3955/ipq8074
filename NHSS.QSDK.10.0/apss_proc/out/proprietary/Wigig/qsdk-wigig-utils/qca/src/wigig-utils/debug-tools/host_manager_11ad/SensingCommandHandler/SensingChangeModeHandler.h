/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_CHANGE_MODE_HANDLER_H_
#define _SENSING_CHANGE_MODE_HANDLER_H_

#include "JsonHandlerSDK.h"

class SensingChangeModeRequest : public JsonDeviceRequest
{
public:
    explicit SensingChangeModeRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    JsonValueBoxed<std::string> GetMode() const
    {
        return JsonValueParser::ParseStringValue(m_jsonRequestValue, "Mode");
    }

    JsonValueBoxed<uint32_t> GetPreferredChannel() const
    {
        return JsonValueParser::ParseUnsignedValue(m_jsonRequestValue, "PreferredChannel");
    }
};

class SensingChangeModeResponse final : public JsonResponse
{
public:
    SensingChangeModeResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("SensingChangeModeResponse", jsonRequestValue, jsonResponseValue)
    {}

    void SetBurstSizeBytes(uint32_t burstSize)
    {
        m_jsonResponseValue["BurstSizeBytes"] = burstSize;
    }
};

class SensingChangeModeHandler : public JsonOpCodeHandlerBase<SensingChangeModeRequest, SensingChangeModeResponse>
{
private:
    void HandleRequest(const SensingChangeModeRequest& jsonRequest, SensingChangeModeResponse& jsonResponse) override;
};

#endif  // _SENSING_CHANGE_MODE_HANDLER_H_

