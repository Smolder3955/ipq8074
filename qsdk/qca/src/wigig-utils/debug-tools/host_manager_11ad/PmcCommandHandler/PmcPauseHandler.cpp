/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcPauseHandler.h"
#include "PmcSequence.h"
#include "PmcActions.h"
#include "Host.h"
#include "DeviceManager.h"

void PmcPauseHandler::HandleRequest(const PmcPauseRequest& jsonRequest, PmcPauseResponse& jsonResponse)
{
    (void)jsonRequest;

    std::string deviceName = jsonRequest.GetDeviceName();
    if (deviceName.empty()) // key is missing or an empty string provided
    {
        OperationStatus os = Host::GetHost().GetDeviceManager().GetDefaultDevice(deviceName);
        if (!os)
        {
            jsonResponse.Fail(os.GetStatusMessage());
            return;
        }
    }
    LOG_DEBUG << "PMC pause request for Device: " << deviceName << std::endl;

    auto PauseRes = PmcActions::Pause(deviceName);

    if (!PauseRes.IsSuccess())
    {
        jsonResponse.Fail(PauseRes.GetStatusMessage());
        return;
    }
    jsonResponse.SetCurrentState(PmcActions::GetCurrentStateString(deviceName));
}
