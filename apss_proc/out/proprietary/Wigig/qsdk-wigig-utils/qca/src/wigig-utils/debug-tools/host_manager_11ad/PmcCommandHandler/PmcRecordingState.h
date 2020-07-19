/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef PMC_RECORDING_STATE_H_
#define PMC_RECORDING_STATE_H_

#include <map>
#include <string>
#include "DebugLogger.h"

enum PMC_RECORDING_STATE
{
    PMC_RECORDING_STATE_IDLE = 0,           // No recording, pending command or recording data
    PMC_RECORDING_STATE_READY,              // The state of the PMC right after init (ready to activate recording)
    PMC_RECORDING_STATE_ACTIVE,             // The recording is triggered by UI/CLI command
    PMC_RECORDING_STATE_PAUSED,             // The recording was paused ready to collect data
    PMC_RECORDING_STATE_ERROR               // Something went wrong
};

const char* ToString(const PMC_RECORDING_STATE &stateenum);
inline std::ostream& operator<<(std::ostream& os, const PMC_RECORDING_STATE &stateenum)
{
    return os << ToString(stateenum);
}

// Represents the last recording state according to the executed command sequence.
// It does not reflect a real HW state but rather an expected state of the recording.
class PmcRecordingState
{
    public:
        PmcRecordingState() : m_state(PMC_RECORDING_STATE::PMC_RECORDING_STATE_IDLE) { }
        void SetState(PMC_RECORDING_STATE newState);
        enum PMC_RECORDING_STATE GetState() const
        {
            return m_state;
        }
        const std::string& GetStartRecordingTime() const
        {
            return m_startRecordingTime;
        }

        const std::string& GetStopRecordingTime() const
        {
            return m_stopRecordingTime;
        }
    private:
        PMC_RECORDING_STATE m_state;
        std::string m_startRecordingTime;
        std::string m_stopRecordingTime;
};

inline std::ostream& operator<<(std::ostream& os, const PmcRecordingState& recordingState)
{
    auto state = recordingState.GetState();
    os << "Recording state: " << state << " Start Recording time: " << recordingState.GetStartRecordingTime()
                                       << " End recording time:" << recordingState.GetStopRecordingTime() << std::endl;
    return os;
}

#endif /* PMC_RECORDING_STATE_H_ */
