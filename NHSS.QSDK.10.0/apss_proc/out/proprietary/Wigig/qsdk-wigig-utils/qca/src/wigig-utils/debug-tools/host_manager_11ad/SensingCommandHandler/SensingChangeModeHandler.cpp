/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "SensingChangeModeHandler.h"
#include "SensingService.h"
#include "DebugLogger.h"

void SensingChangeModeHandler::HandleRequest(const SensingChangeModeRequest& jsonRequest, SensingChangeModeResponse& jsonResponse)
{
    LOG_DEBUG << "Sensing change_mode request" << std::endl;

    const JsonValueBoxed<std::string> modeBoxed = jsonRequest.GetMode();
    const JsonValueBoxed<uint32_t> preferredChannelBoxed = jsonRequest.GetPreferredChannel();

    if ( !(modeBoxed.GetState() == JsonValueState::JSON_VALUE_PROVIDED
        && preferredChannelBoxed.GetState() == JsonValueState::JSON_VALUE_PROVIDED) )
    {
        const char* errStr = "'Mode' and 'PreferredChannel' parameters are required";
        LOG_ERROR << "Failed to change Sensing mode: " << errStr << std::endl;
        jsonResponse.Fail(errStr);
        return;
    }

    // note: we still can have malformed (string) value for mode in this point and it will be handled in the Service call
    uint32_t burstSizeBytes = 0U;
    const auto changeModeRes = SensingService::GetInstance().ChangeMode(modeBoxed, preferredChannelBoxed, burstSizeBytes);
    if (!changeModeRes)
    {
        jsonResponse.Fail(changeModeRes.GetStatusMessage());
        return;
    }

    jsonResponse.SetBurstSizeBytes(burstSizeBytes);
}
