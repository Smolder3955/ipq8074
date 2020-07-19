/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcGetConfigHandler.h"
#include "Host.h"
#include "PmcService.h"
#include "PmcSequence.h"
#include "PmcActions.h"
#include "DeviceManager.h"

void PmcGetConfigHandler::HandleRequest(const PmcGetConfigRequest& jsonRequest, PmcGetConfigResponse& jsonResponse)
{
    LOG_DEBUG << "PMC configuration request for Device: " << jsonRequest.GetDeviceName() << std::endl;
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

    BasebandType type;
    auto nameTypeValidRes = PmcActions::ValidateDeviceName(deviceName, type);
    if (!nameTypeValidRes.IsSuccess())
    {
        jsonResponse.Fail(nameTypeValidRes.GetStatusMessage());
        return;
    }
    auto configuration = PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceConfigurations();
    LOG_DEBUG << "Getting configurations. " << configuration << std::endl;
    jsonResponse.SetCollectIdleSmEvents(configuration.IsCollectIdleSmEvents());
    jsonResponse.SetCollectRxPpduEvents(configuration.IsCollectRxPpduEvents());
    jsonResponse.SetCollectTxPpduEvents(configuration.IsCollectTxPpduEvents());
    jsonResponse.SetCollectUCodeEvents(configuration.IsCollectUcodeEvents());
    jsonResponse.SetCurrentState(PmcActions::GetCurrentStateString(deviceName));
}

