/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_READ_DATA_HANDLER_H_
#define _SENSING_READ_DATA_HANDLER_H_

#include <string>
#include <vector>
#include "JsonHandlerSDK.h"

class SensingReadDataRequest : public JsonDeviceRequest
{
public:
    explicit SensingReadDataRequest(const Json::Value& jsonRequestValue) :
        JsonDeviceRequest(jsonRequestValue)
    {
    }

    JsonValueBoxed<uint32_t> GetMaxSizeDwords() const
    {
        return JsonValueParser::ParseUnsignedValue(m_jsonRequestValue, "MaxSizeDwords");
    }
};

class SensingReadDataResponse final : public JsonResponse
{
public:
    SensingReadDataResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("SensingReadDataResponse", jsonRequestValue, jsonResponseValue)
    {}

    // Passed data string is expected to be encoded Base64
    void SetDataBase64(const std::string& dataBase64)
    {
        m_jsonResponseValue["DataBase64"] = dataBase64;
    }

    void SetDriTsfs(const std::vector<uint64_t>& driTsfVec);

    void SetDropCntFromLastRead(uint32_t dropCnt)
    {
        m_jsonResponseValue["DropCntFromLastRead"] = dropCnt;
    }

    void SetNumRemainingBursts(uint32_t numRemainingBursts)
    {
        m_jsonResponseValue["NumRemainingBursts"] = numRemainingBursts;
    }
};

class SensingReadDataHandler : public JsonOpCodeHandlerBase<SensingReadDataRequest, SensingReadDataResponse>
{
private:
    void HandleRequest(const SensingReadDataRequest& jsonRequest, SensingReadDataResponse& jsonResponse) override;
};

#endif  // _SENSING_READ_DATA_HANDLER_H_

