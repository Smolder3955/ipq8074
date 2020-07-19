/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcSetConfigHandler.h"
#include "HostManagerDefinitions.h"
#include "PmcSequence.h"
#include "PmcSetConfigHandlerSequences.h"
#include "PmcActions.h"
#include "Host.h"
#include "DeviceManager.h"

void PmcSetConfigHandler::HandleRequest(const PmcSetConfigRequest& jsonRequest, PmcSetConfigResponse& jsonResponse)
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

    JsonValueBoxed<bool> setCollectIdleSm = jsonRequest.GetCollectIdleSmEvents();
    JsonValueBoxed<bool> setCollectRxPpdu = jsonRequest.GetCollectRxPpduEvents();
    JsonValueBoxed<bool> setCollectTxPpdu = jsonRequest.GetCollectTxPpduEvents();
    JsonValueBoxed<bool> setCollectUCode = jsonRequest.GetCollectUCodeEvents();
    LOG_DEBUG << "Setting Configurations." << " Device: " << deviceName << " IDLE_SM: " << setCollectIdleSm << " RX PPDU: " << setCollectRxPpdu << " TX PPDU: " << setCollectTxPpdu << " uCode: "
            << setCollectUCode << std::endl;
    if (setCollectIdleSm.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        LOG_DEBUG << "Setting PMC IDLE_SM event collection: " << setCollectIdleSm << std::endl;
        auto collectSmRes = PmcSetConfigHandlerSequences::ConfigCollectIdleSm(deviceName, setCollectIdleSm.GetValue());
        if (!collectSmRes.IsSuccess())
        {
            jsonResponse.Fail(collectSmRes.GetStatusMessage());
            return;
        }
    }
    if (setCollectRxPpdu.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        LOG_DEBUG << "Setting PMC RX PPDU event collection: " << setCollectRxPpdu << std::endl;
        auto rxPpduRes = PmcSetConfigHandlerSequences::ConfigCollectRxPpdu(deviceName, setCollectRxPpdu.GetValue());
        if (!rxPpduRes.IsSuccess())
        {
            jsonResponse.Fail(rxPpduRes.GetStatusMessage());
            return;
        }
    }
    if (setCollectTxPpdu.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        LOG_DEBUG << "Setting PMC TX PPDU event collection: " << setCollectTxPpdu << std::endl;
        auto txPpduRes = PmcSetConfigHandlerSequences::ConfigCollectTxPpdu(deviceName, setCollectTxPpdu.GetValue());
        if (!txPpduRes.IsSuccess())
        {
            jsonResponse.Fail(txPpduRes.GetStatusMessage());
            return;
        }
    }

    if (setCollectUCode.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        LOG_DEBUG << "Setting PMC uCode event collection: " << setCollectUCode << std::endl;
        auto collectUcode = PmcSetConfigHandlerSequences::ConfigCollectUcode(deviceName, setCollectUCode.GetValue());
        if (!collectUcode.IsSuccess())
        {
            jsonResponse.Fail(collectUcode.GetStatusMessage());
            return;
        }
    }

    //TODO: for now, everything else gets default values
    auto setDefaultConfigRes = PmcSequence::SetDefaultConfiguration(deviceName, type);
    if (!setDefaultConfigRes.IsSuccess())
    {
        jsonResponse.Fail(setDefaultConfigRes.GetStatusMessage());
        return;
    }

    jsonResponse.SetCurrentState(PmcActions::GetCurrentStateString(deviceName));
    LOG_DEBUG << "PMC SetConfig Response: " << jsonResponse << std::endl;
}

