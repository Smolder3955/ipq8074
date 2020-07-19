/*
* Copyright (c) 2017 - 2018 Qualcomm Technologies, Inc.
*
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcShutdownHandler.h"
#include "PmcSequence.h"
#include "PmcActions.h"
#include "Host.h"
#include "DeviceManager.h"

void PmcShutdownHandler::HandleRequest(const PmcShutdownRequest& jsonRequest, PmcShutdownResponse& jsonResponse)
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
    LOG_DEBUG << "PMC Shutdown request for Device: " << deviceName << std::endl;
    auto shutDownRes = PmcActions::Shutdown(deviceName);
    if (!shutDownRes.IsSuccess())
    {
        jsonResponse.Fail(shutDownRes.GetStatusMessage());
        return;
    }
    jsonResponse.SetCurrentState(PmcActions::GetCurrentStateString(deviceName));
    LOG_INFO << "Performing PMC Shutdown on device: " << deviceName << std::endl;
}
