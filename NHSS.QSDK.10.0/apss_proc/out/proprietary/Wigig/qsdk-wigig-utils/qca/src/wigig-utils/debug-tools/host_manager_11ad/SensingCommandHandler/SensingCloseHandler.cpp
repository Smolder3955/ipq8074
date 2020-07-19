/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "SensingCloseHandler.h"
#include "SensingService.h"
#include "DebugLogger.h"

void SensingCloseHandler::HandleRequest(const Json::Value& jsonRequest, JsonBasicResponse& jsonResponse)
{
    LOG_DEBUG << "Sensing close request" << std::endl;

    (void)jsonRequest;
    const auto closeRes = SensingService::GetInstance().CloseSensing();
    if (!closeRes)
    {
        jsonResponse.Fail(closeRes.GetStatusMessage());
    }
}
