/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_OPEN_HANDLER_H_
#define _SENSING_OPEN_HANDLER_H_

#include "JsonHandlerSDK.h"

class SensingOpenRequest : public JsonDeviceRequest
{
public:
    explicit SensingOpenRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {}

    JsonValueBoxed<bool> GetAutoRecovery() const
    {
        return JsonValueParser::ParseBooleanValue(m_jsonRequestValue, "AutoRecovery");
    }

    JsonValueBoxed<bool> GetSendNotification() const
    {
        return JsonValueParser::ParseBooleanValue(m_jsonRequestValue, "SendNotification");
    }
};

class SensingOpenHandler : public JsonOpCodeHandlerBase<SensingOpenRequest, JsonBasicResponse>
{
private:
    void HandleRequest(const SensingOpenRequest& jsonRequest, JsonBasicResponse& jsonResponse) override;
};

#endif  // _SENSING_OPEN_HANDLER_H_
