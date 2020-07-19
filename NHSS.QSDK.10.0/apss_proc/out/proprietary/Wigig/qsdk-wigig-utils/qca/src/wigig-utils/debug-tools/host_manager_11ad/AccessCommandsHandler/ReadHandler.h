/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _READ_HANDLER_H_
#define _READ_HANDLER_H_

#include "JsonHandlerSDK.h"

class ReadRequest : public JsonDeviceRequest
{
public:
    ReadRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    JsonValueBoxed<uint32_t> GetReadAddress() const
    {
        return JsonValueParser::ParseUnsignedValue(m_jsonRequestValue, "Address");
    }

};


class ReadResponse: public JsonResponse
{
    public:
        ReadResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
            JsonResponse("ReadResponse", jsonRequestValue, jsonResponseValue)
        {
        }

        void SetValue(uint32_t value);

};

class ReadHandler: public JsonOpCodeHandlerBase<ReadRequest, ReadResponse>
{
    private:
        void HandleRequest(const ReadRequest& jsonRequest, ReadResponse& jsonResponse) override;
};

#endif  // _READ_HANDLER_H_
