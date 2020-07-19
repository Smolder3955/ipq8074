/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <vector>
#include "SensingReadDataHandler.h"
#include "SensingService.h"
#include "DebugLogger.h"

void SensingReadDataResponse::SetDriTsfs(const std::vector<uint64_t>& driTsfVec)
{
    Json::Value driTsfs(Json::arrayValue);
    for (const auto& driTsf : driTsfVec)
    {
        driTsfs.append(driTsf);
    }
    m_jsonResponseValue["DriTsfs"] = driTsfs;
}


void SensingReadDataHandler::HandleRequest(const SensingReadDataRequest& jsonRequest, SensingReadDataResponse& jsonResponse)
{
    LOG_DEBUG << "Sensing read_data request" << std::endl;

    const JsonValueBoxed<uint32_t> maxSizeDwordsBoxed = jsonRequest.GetMaxSizeDwords();
    if (maxSizeDwordsBoxed.GetState() != JsonValueState::JSON_VALUE_PROVIDED)
    {
        const char* errStr = "'MaxSizeDwords' parameter is required";
        LOG_ERROR << "Failed to read Sensing data: " << errStr << std::endl;
        jsonResponse.Fail(errStr);
        return;
    }
    const uint32_t maxSizeBytes = maxSizeDwordsBoxed * sizeof(uint32_t);

    std::vector<unsigned char> sensingData;
    std::vector<uint64_t> driTsfVec;
    uint32_t dropCntFromLastRead = 0U, numRemainingBursts = 0U;
    const auto readDataRes = SensingService::GetInstance().ReadData(maxSizeBytes, sensingData, driTsfVec, dropCntFromLastRead, numRemainingBursts);
    if (!readDataRes)
    {
        jsonResponse.Fail(readDataRes.GetStatusMessage());
        return;
    }

    // vector contains only data to be returned to the caller, encode it as Base64 string
    jsonResponse.SetDataBase64(Utils::Base64Encode(sensingData.data(), sensingData.size())); // empty string if the buffer is empty

    jsonResponse.SetDriTsfs(driTsfVec);
    jsonResponse.SetDropCntFromLastRead(dropCntFromLastRead);
    jsonResponse.SetNumRemainingBursts(numRemainingBursts);
}
