/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcGetRecordingStateHandler.h"
#include "PmcSequence.h"
#include "PmcService.h"
#include "PmcActions.h"
#include "Host.h"
#include "DeviceManager.h"

void PmcGetRecordingStateHandler::HandleRequest(const PmcGetRecordingStateRequest& jsonRequest, PmcGetRecordingStateResponse& jsonResponse)
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
    BasebandType type;
    auto nameTypeValidRes = PmcActions::ValidateDeviceName(deviceName, type);
    if (!nameTypeValidRes.IsSuccess())
    {
        jsonResponse.Fail(nameTypeValidRes.GetStatusMessage());
        return;
    }
    uint32_t pmcGeneral0Value;
    auto status = PmcService::GetInstance().GetPmcRegistersAccessor().ReadPmcRegister(deviceName, "MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0", pmcGeneral0Value);
    if (!status.IsSuccess())
    {
        LOG_ERROR << status;
        jsonResponse.Fail(status.GetStatusMessage());
        return;
    }
    uint32_t pmcGeneral1Value;
    auto status1 = PmcService::GetInstance().GetPmcRegistersAccessor().ReadPmcRegister(deviceName, "MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_1", pmcGeneral1Value);
    if (!status1.IsSuccess())
    {
        LOG_ERROR << status;
        jsonResponse.Fail(status1.GetStatusMessage());
        return;
    }
    jsonResponse.SetPmcGeneral0(pmcGeneral0Value);
    jsonResponse.SetPmcGeneral1(pmcGeneral1Value);
    bool recActive;
    auto res = PmcSequence::IsPmcRecordingActive(deviceName,recActive);
    if(!res.IsSuccess())
    {
        jsonResponse.Fail(res.GetStatusMessage());
        return;
    }
    jsonResponse.SetIsActive(recActive);
}

