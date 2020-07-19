/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <vector>
#include "SensingGetParametersHandler.h"
#include "SensingService.h"
#include "DebugLogger.h"
#include "Utils.h"

void SensingGetParametersHandler::HandleRequest(const Json::Value& jsonRequest, SensingGetParametersResponse& jsonResponse)
{
    LOG_DEBUG << "Sensing get_parameters request" << std::endl;

    (void)jsonRequest;
    std::vector<unsigned char> parameters;
    const auto getParamsRes = SensingService::GetInstance().GetParameters(parameters);
    if (!getParamsRes)
    {
        jsonResponse.Fail(getParamsRes.GetStatusMessage());
        return;
    }

    // vector contains only data to be returned to the caller, encode it as Base64 string
    jsonResponse.SetParametersBase64(Utils::Base64Encode(parameters.data(), parameters.size())); // empty string if the buffer is empty
}
