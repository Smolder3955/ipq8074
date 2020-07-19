/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcActions.h"
#include "PmcRecordingFileCreator.h"
#include "PmcSequence.h"
#include "Host.h"
#include "DebugLogger.h"
#include "PmcService.h"
#include "DeviceManager.h"

OperationStatus PmcActions::Shutdown(const std::string & deviceName)
{
    BasebandType type;
    auto res = ValidateDeviceName(deviceName, type);
    if (!res)
    {
        return res;
    }
    // No Validation needed. ShutDown can be called in any state
    res = PmcSequence::Shutdown(deviceName);
    if (!res)
    {
        return res;
    }
    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().SetState(PMC_RECORDING_STATE::PMC_RECORDING_STATE_IDLE);
    return OperationStatus(true);
}


OperationStatus PmcActions::Init(const std::string & deviceName)
{
    BasebandType type;
    auto res = ValidateDeviceName(deviceName, type);
    if (!res)
    {
        return res;
    }

    if (PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().GetState() != PMC_RECORDING_STATE::PMC_RECORDING_STATE_IDLE)
    {
        return OperationStatus(false, "Pmc is already initialized. To restart the process please use shutdown command followed by init / start ");
    }

    res = PmcSequence::Init(deviceName);
    if (!res)
    {
        return res;
    }

    // We set the current configuration since shutdown deletes it
    res = PmcSequence::ApplyCurrentConfiguration(deviceName, type);
    if (!res)
    {
        return res;
    }

    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().SetState(PMC_RECORDING_STATE::PMC_RECORDING_STATE_READY);
    return OperationStatus(true);
}


OperationStatus PmcActions::Start(const std::string & deviceName)
{
    BasebandType type;
    auto res = ValidateDeviceName(deviceName, type);
    if (!res)
    {
        return res;
    }

    // Explicit check  if the device is not initialized (i.e. in IDLE) do init and then start
    bool implicitInit = false;
    if (PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().GetState() == PMC_RECORDING_STATE::PMC_RECORDING_STATE_IDLE)
    {
        LOG_INFO << "Performing implicit PMC initialization" << std::endl;

        res = PmcSequence::Init(deviceName);
        if (!res)
        {
            return res;
        }
        // We set the current configuration since shutdown deletes it
        res = PmcSequence::ApplyCurrentConfiguration(deviceName, type);
        if (!res)
        {
            return res;
        }

        implicitInit = true;

        // Set device to READY to continue start flow
        PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().SetState(PMC_RECORDING_STATE::PMC_RECORDING_STATE_READY);
    }

    // If PMC is not READY now (also means it was not in IDLE before) START transition is invalid
    if (PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().GetState() != PMC_RECORDING_STATE::PMC_RECORDING_STATE_READY)
    {
        return OperationStatus(false, "Invalid transition, start can be called only after init or shutdown");
    }
    // Check if PMC is already running
    bool recActive;
    res = PmcSequence::IsPmcRecordingActive(deviceName, recActive);
    if (!res)
    {
        return res;
    }
    if (recActive)
    {
        return OperationStatus(false, "Recording already active. To restart recording please use shutdown command followed by init / start");
    }
    res = PmcSequence::ActivateRecording(deviceName);
    if (!res)
    {
        return res;
    }
    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().SetState(PMC_RECORDING_STATE::PMC_RECORDING_STATE_ACTIVE);
    return OperationStatus(true, implicitInit ? "true" : "");
}

OperationStatus PmcActions::Pause(const std::string & deviceName)
{
    BasebandType type;
    auto res = ValidateDeviceName(deviceName, type);
    if (!res)
    {
        return res;
    }
    if (PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().GetState() != PMC_RECORDING_STATE::PMC_RECORDING_STATE_ACTIVE)
    {
        return OperationStatus(false, "Pmc is not Active, Pause has no effect");
    }

    res = PmcSequence::PausePmcRecording(deviceName);
    if (!res)
    {
        return res;
    }
    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().SetState(PMC_RECORDING_STATE::PMC_RECORDING_STATE_PAUSED);
    return OperationStatus(true);
}

OperationStatus PmcActions::Stop(const std::string & deviceName)
{
    BasebandType type = BASEBAND_TYPE_NONE;
    auto res = ValidateDeviceName(deviceName, type);

    if (!res)
    {
        return res;
    }
    auto currentState = PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().GetState();
    if (currentState == PMC_RECORDING_STATE::PMC_RECORDING_STATE_IDLE
        || currentState == PMC_RECORDING_STATE::PMC_RECORDING_STATE_READY)
    {
        return OperationStatus(false, "Pmc is not Active or Paused. Stop has no effect");
    }

    if (currentState == PMC_RECORDING_STATE::PMC_RECORDING_STATE_ACTIVE)
    {
        res = PmcSequence::PausePmcRecording(deviceName);
        if (!res)
        {
            return res;
        }
        PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().SetState(PMC_RECORDING_STATE::PMC_RECORDING_STATE_PAUSED);
    }


    PmcRecordingFileCreator pmcFilesCreator(deviceName, type);

    std::string pmcDataPath;
    res = pmcFilesCreator.CreatePmcFiles(pmcDataPath);
    if (!res)
    {
        return res;
    }

    PmcSequence::Shutdown(deviceName);

    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().SetState(PMC_RECORDING_STATE::PMC_RECORDING_STATE_IDLE);
    return OperationStatus(true, pmcDataPath.c_str()); // Returns the data path to be added to the JSON
}

OperationStatus PmcActions::ValidateDeviceName(const std::string& deviceName, BasebandType& deviceType)
{
    LOG_DEBUG << "Checking if device name " << deviceName << " is valid " << std::endl;
    if (!Host::GetHost().GetDeviceManager().GetDeviceByName(deviceName))
    {
        return OperationStatus(false, "Device Name does not exist");
    }
    if (!Host::GetHost().GetDeviceManager().GetDeviceBasebandType(deviceName, deviceType))
    {
        return OperationStatus(false, "Failed reading device type");
    }
    return OperationStatus(true);
}


const char* PmcActions::GetCurrentStateString(const std::string & deviceName)
{
    auto currentState = PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceRecordingState().GetState();
    return ToString(currentState);
}

