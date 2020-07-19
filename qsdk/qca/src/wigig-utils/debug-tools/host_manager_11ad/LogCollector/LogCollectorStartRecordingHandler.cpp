/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorStartRecordingHandler.h"
#include "LogCollector.h"
#include "LogCollectorDefinitions.h"
#include "Host.h"
#include "DeviceManager.h"

void LogCollectorStartRecordingHandler::HandleRequest(const LogCollectorStartRecordingRequest& jsonRequest, LogCollectorStartRecordingResponse& jsonResponse)
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

    const JsonValueBoxed<CpuType> cpuTypeBoxed = jsonRequest.GetCpuType();
    CpuType cpuType;
    switch (cpuTypeBoxed.GetState())
    {
    case JsonValueState::JSON_VALUE_MALFORMED:
        jsonResponse.Fail("CpuType is wrong, it should be 'fw' or 'ucode' or empty (for both cpus).");
        return;
    case JsonValueState::JSON_VALUE_MISSING:
        cpuType = CPU_TYPE_BOTH;
        break;
    default:
        // guaranty that value is provided.
        cpuType = cpuTypeBoxed;
        break;
    }

    const JsonValueBoxed<log_collector::RecordingType> RecordingTypeBoxed = jsonRequest.GetRecordingTypeBoxed();
    log_collector::RecordingType recordingType;
    switch (RecordingTypeBoxed.GetState())
    {
    case JsonValueState::JSON_VALUE_MALFORMED:
        jsonResponse.Fail("RecordingType is wrong, it should be 'raw' or 'txt' or empty (default is raw)");
        return;
    case JsonValueState::JSON_VALUE_MISSING:
        recordingType = log_collector::RECORDING_TYPE_RAW;
        break;
    default:
        // guaranty that value is provided.
        recordingType = RecordingTypeBoxed;
        break;
    }

    LOG_DEBUG << "Log Collector start recording for Device: " << deviceName
        << " with CPU type: " << cpuTypeBoxed
        << " recording type is: " << recordingType << std::endl;
    OperationStatus os = Host::GetHost().GetDeviceManager().StartStopRecording(deviceName, cpuType, true, recordingType);

    if (!os)
    {
        jsonResponse.Fail(os.GetStatusMessage());
        return;
    }
    jsonResponse.SetRecordingPath(os.GetStatusMessage());
}

