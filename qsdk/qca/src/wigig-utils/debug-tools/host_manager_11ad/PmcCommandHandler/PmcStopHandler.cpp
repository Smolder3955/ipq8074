/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcStopHandler.h"
#include "PmcRecordingState.h"
#include "PmcSequence.h"
#include "PmcActions.h"
#include "Host.h"
#include "DeviceManager.h"

void PmcStopHandler::HandleRequest(const PmcStopRequest& jsonRequest, PmcStopResponse& jsonResponse)
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

    LOG_DEBUG << "PMC stop request for Device: " << jsonRequest.GetDeviceName() << std::endl;
    auto StopRes = PmcActions::Stop(deviceName);
    if (!StopRes.IsSuccess())
    {
        jsonResponse.Fail(StopRes.GetStatusMessage());
        return;
    }
    jsonResponse.SetCurrentState(PmcActions::GetCurrentStateString(deviceName));
    jsonResponse.SetRecordingDirectory(StopRes.GetStatusMessage().c_str());
    // When the PMC is stopped after SysAssert, the ring deallocation request returns error (-11).
    // We should ignore this error to have the graph rendered.
}

