/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcInitHandler.h"
#include "PmcSequence.h"
#include "PmcActions.h"
#include "Host.h"
#include "DeviceManager.h"

void PmcInitHandler::HandleRequest(const PmcInitRequest& jsonRequest, PmcInitResponse& jsonResponse)
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
    LOG_DEBUG << "PMC init request for Device: " << deviceName << std::endl;

    auto InitRes = PmcActions::Init(deviceName);
    if (!InitRes.IsSuccess())
    {
        jsonResponse.Fail(InitRes.GetStatusMessage());
        return;
    }
    jsonResponse.SetCurrentState(PmcActions::GetCurrentStateString(deviceName));
}

