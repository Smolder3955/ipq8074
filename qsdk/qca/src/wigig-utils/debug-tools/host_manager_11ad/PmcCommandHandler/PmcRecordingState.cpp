/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcRecordingState.h"
#include "Utils.h"

void PmcRecordingState::SetState(PMC_RECORDING_STATE newState)
{
    LOG_INFO << "Changing PMC recording state: " << GetState() << " to " << newState << std::endl;
    switch (newState)
    {
    case PMC_RECORDING_STATE::PMC_RECORDING_STATE_ACTIVE:
        m_startRecordingTime = Utils::GetCurrentDotNetDateTimeString();
        break;

    case PMC_RECORDING_STATE::PMC_RECORDING_STATE_PAUSED:
        m_stopRecordingTime = Utils::GetCurrentDotNetDateTimeString();
        break;

    default:
        break;
    }
    m_state = newState;
}

const char* ToString(const PMC_RECORDING_STATE &stateEnum)
{
    switch (stateEnum)
    {
    case PMC_RECORDING_STATE_IDLE:
        return "IDLE";
    case PMC_RECORDING_STATE_READY:
        return "READY";
    case PMC_RECORDING_STATE_ACTIVE:
        return "ACTIVE";
    case PMC_RECORDING_STATE_PAUSED:
        return "PAUSED";
    default:
        LOG_INFO << "Could not get current recording state.";
        return "ERROR";
    }
}