/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "SensingGetNumRemainingBurstsHandler.h"
#include "SensingService.h"
#include "DebugLogger.h"

void SensingGetNumRemainingBurstsHandler::HandleRequest(const Json::Value& jsonRequest, SensingGetNumRemainingBurstsResponse& jsonResponse)
{
    LOG_DEBUG << "Sensing get_num_remaining_bursts request" << std::endl;

    (void)jsonRequest;
    uint32_t numRemainingBursts = 0U;
    const auto getNumRemainingBurstsRes = SensingService::GetInstance().GetNumRemainingBursts(numRemainingBursts);
    if (!getNumRemainingBurstsRes)
    {
        jsonResponse.Fail(getNumRemainingBurstsRes.GetStatusMessage());
        return;
    }

    jsonResponse.SetNumRemainingBursts(numRemainingBursts);
}
