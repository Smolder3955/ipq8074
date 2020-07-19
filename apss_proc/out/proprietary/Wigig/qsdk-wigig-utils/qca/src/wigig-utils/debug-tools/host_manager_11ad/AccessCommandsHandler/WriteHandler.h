/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _WRITE_HANDLER_H_
#define _WRITE_HANDLER_H_

#include "JsonHandlerSDK.h"

class WriteRequest : public JsonDeviceRequest
{
public:
    WriteRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    JsonValueBoxed<uint32_t> GetWriteAddress() const
    {
        return JsonValueParser::ParseUnsignedValue(m_jsonRequestValue, "Address");
    }

    JsonValueBoxed<uint32_t> GetValue() const
    {
        return JsonValueParser::ParseUnsignedValue(m_jsonRequestValue, "Value");
    }

};

class WriteHandler : public JsonOpCodeHandlerBase<WriteRequest, JsonBasicResponse>
{
private:
    void HandleRequest(const WriteRequest& jsonRequest, JsonBasicResponse& jsonResponse) override;
};

#endif  // _WRITE_HANDLER_H_
