/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorStopRecordingHandler.h"
#include "LogCollector.h"
#include "Host.h"
#include "DeviceManager.h"

void LogCollectorStopRecordingHandler::HandleRequest(const LogCollectorStopRecordingRequest& jsonRequest, LogCollectorStopRecordingResponse& jsonResponse)
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
        jsonResponse.Fail("CpuType is wrong, it should be 'fw' or 'ucode' or empty");
        return;
    case JsonValueState::JSON_VALUE_MISSING:
        cpuType = CPU_TYPE_BOTH;
        break;
    default:
        // guaranty that value is provided.
        cpuType = cpuTypeBoxed;
        break;
    }

    LOG_DEBUG << "Log Collector start recording for Device: " << deviceName << " with CPU type: " << cpuTypeBoxed << std::endl;

    OperationStatus os = Host::GetHost().GetDeviceManager().StartStopRecording(deviceName, cpuType, false); // deviceName might be "" if useDefaultDevice is true, false means stop recording

    if (!os)
    {
        jsonResponse.Fail(os.GetStatusMessage());
        return;
    }
    jsonResponse.SetRecordingPath(os.GetStatusMessage());
}

