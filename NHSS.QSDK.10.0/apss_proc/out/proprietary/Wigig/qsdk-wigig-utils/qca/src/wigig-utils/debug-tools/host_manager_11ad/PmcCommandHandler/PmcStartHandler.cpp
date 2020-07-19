/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcStartHandler.h"
#include "PmcActions.h"
#include "Host.h"
#include "DeviceManager.h"

void PmcStartHandler::HandleRequest(const PmcStartRequest& jsonRequest, PmcStartResponse& jsonResponse)
{
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
    LOG_DEBUG << "PMC start request for Device: " << deviceName << std::endl;

    auto StartRes = PmcActions::Start(deviceName);
    if (!StartRes.IsSuccess())
    {
        jsonResponse.Fail(StartRes.GetStatusMessage());
        return;
    }
    jsonResponse.SetCurrentState(PmcActions::GetCurrentStateString(deviceName));
    // Check if init was invoked implicitly
    bool implicitTransition = !StartRes.GetStatusMessage().empty();
    if (implicitTransition)
    {
        jsonResponse.SetImplicitInit(implicitTransition);
    }
}

