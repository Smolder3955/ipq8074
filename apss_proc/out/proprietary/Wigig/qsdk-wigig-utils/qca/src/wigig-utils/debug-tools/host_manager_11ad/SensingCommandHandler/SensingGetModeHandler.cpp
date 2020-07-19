/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <string>
#include "SensingGetModeHandler.h"
#include "SensingService.h"
#include "DebugLogger.h"

void SensingGetModeHandler::HandleRequest(const Json::Value& jsonRequest, SensingGetModeResponse& jsonResponse)
{
    LOG_DEBUG << "Sensing get_mode request" << std::endl;

    (void)jsonRequest;
    std::string modeStr;
    const auto getModeRes = SensingService::GetInstance().GetMode(modeStr);
    if (!getModeRes)
    {
        jsonResponse.Fail(getModeRes.GetStatusMessage());
        return;
    }

    jsonResponse.SetMode(modeStr);
}
