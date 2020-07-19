/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _READ_BLOCK_HANDLER_H_
#define _READ_BLOCK_HANDLER_H_

#include "JsonHandlerSDK.h"

class ReadBlockRequest : public JsonDeviceRequest
{
public:
    ReadBlockRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    JsonValueBoxed<uint32_t> GetReadAddress() const
    {
        return JsonValueParser::ParseUnsignedValue(m_jsonRequestValue, "Address");
    }
    JsonValueBoxed<uint32_t> GetBlockSize() const
    {
        return JsonValueParser::ParseUnsignedValue(m_jsonRequestValue, "Size");
    }

};

class ReadBlockResponse : public JsonResponse
{
public:
    ReadBlockResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("ReadResponse", jsonRequestValue, jsonResponseValue)
    {
    }

    void AddValuesToJson(const std::vector<DWORD>& valuesVec);
};

class ReadBlockHandler : public JsonOpCodeHandlerBase<ReadBlockRequest, ReadBlockResponse>
{
private:
    void HandleRequest(const ReadBlockRequest& jsonRequest, ReadBlockResponse& jsonResponse) override;
};

#endif  // _READ_BLOCK_HANDLER_H_
