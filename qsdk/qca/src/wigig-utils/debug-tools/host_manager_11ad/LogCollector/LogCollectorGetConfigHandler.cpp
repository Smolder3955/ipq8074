/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorGetConfigHandler.h"
#include "Host.h"
#include "LogCollectorConfiguration.h"
#include "DeviceManager.h"

void LogCollectorGetConfigHandler::HandleRequest(const LogCollectorGetConfigRequest& jsonRequest, LogCollectorGetConfigResponse& jsonResponse)
{
    std::string deviceName = jsonRequest.GetDeviceName();
    if (deviceName.empty()) // key is missing or an empty string provided
    {
        const OperationStatus os = Host::GetHost().GetDeviceManager().GetDefaultDevice(deviceName);
        if (!os)
        {
            jsonResponse.Fail(os.GetStatusMessage());
            return;
        }
    }


    const JsonValueBoxed<CpuType> cpuType = jsonRequest.GetCpuType();
    if (cpuType.GetState() == JsonValueState::JSON_VALUE_MISSING)
    {
        jsonResponse.Fail("CpuType field is missing");
        return;
    }
    if (cpuType.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
    {
        jsonResponse.Fail("CpuType is wrong, it should be 'fw' or 'ucode'");
        return;
    }

    LOG_DEBUG << "Log Collector get configuration request for Device: " << deviceName << " with CPU type: " << cpuType << std::endl;

    log_collector::LogCollectorConfiguration logCollectorConfiguration;
    OperationStatus os = Host::GetHost().GetDeviceManager().GetLogCollectorConfiguration(deviceName, cpuType, logCollectorConfiguration);
    if (!os)
    {
        jsonResponse.Fail(os.GetStatusMessage());
        return;
    }
    jsonResponse.SetPollingIntervalMS((uint32_t)logCollectorConfiguration.GetPollingIntervalMs().count());
    jsonResponse.SetMaxSingleFileSizeMB(logCollectorConfiguration.GetMaxSingleFileSizeMb());
    jsonResponse.SetMaxNumOfLogFiles(logCollectorConfiguration.GetMaxNumOfLogFiles());
    jsonResponse.SetOutputFileSuffix(logCollectorConfiguration.GetOutputFileSuffix());
    jsonResponse.SetConversionFilePath(logCollectorConfiguration.GetConversionFilePath());
    jsonResponse.SetLogFilesDirectoryPath(logCollectorConfiguration.GetLogFilesDirectoryPath());
}
