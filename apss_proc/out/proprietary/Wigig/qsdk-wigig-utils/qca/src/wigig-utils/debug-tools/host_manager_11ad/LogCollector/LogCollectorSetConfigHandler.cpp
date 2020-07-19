/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <sstream>

#include "LogCollectorSetConfigHandler.h"
#include "Host.h"
#include "LogCollectorConfiguration.h"
#include "DeviceManager.h"

using namespace std;

void LogCollectorSetConfigHandler::HandleRequest(const LogCollectorSetConfigRequest& jsonRequest, LogCollectorSetConfigResponse& jsonResponse)
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

    LOG_DEBUG << "Log Collector set configuration request for Device: " << deviceName << " with CPU type: " << cpuType << std::endl;

    log_collector::LogCollectorConfiguration logCollectorConfiguration;
    OperationStatus os = Host::GetHost().GetDeviceManager().GetLogCollectorConfiguration(deviceName, cpuType, logCollectorConfiguration);
    if (!os)
    {
        jsonResponse.Fail(os.GetStatusMessage());
        return;
    }

    const JsonValueBoxed<uint32_t> getPollingIntervalMs = jsonRequest.GetPollingIntervalMs();
    if (getPollingIntervalMs.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        LOG_DEBUG << "setting polling interval to: " << getPollingIntervalMs << endl;
        logCollectorConfiguration.SetPollingIntervalMs(getPollingIntervalMs);
    }
    else if (getPollingIntervalMs.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
    {
        jsonResponse.Fail("'polling_interval_ms' field is malformed");
        return;
    }

    const JsonValueBoxed<uint32_t> getMaxSingleFileSizeMB = jsonRequest.GetMaxSingleFileSizeMb();
    if (getMaxSingleFileSizeMB.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        logCollectorConfiguration.SetMaxSingleFileSizeMb(getMaxSingleFileSizeMB);
    }
    else if (getMaxSingleFileSizeMB.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
    {
        jsonResponse.Fail("'max_single_file_size_mb' field is malformed");
        return;
    }

    const JsonValueBoxed<uint32_t> getMaxNumOfLogFiles = jsonRequest.GetMaxNumOfLogFiles();
    if (getMaxNumOfLogFiles.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        logCollectorConfiguration.SetMaxNumOfLogFiles(getMaxNumOfLogFiles);
    }
    else if (getMaxNumOfLogFiles.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
    {
        jsonResponse.Fail("'max_number_of_log_files' field is malformed");
        return;
    }

    const JsonValueBoxed<std::string>& getOutputFileSuffix = jsonRequest.GetOutputFileSuffix();
    if (getOutputFileSuffix.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        logCollectorConfiguration.SetOutputFileSuffix(getOutputFileSuffix);
    }
    const JsonValueBoxed<std::string>& getConversionFilePath = jsonRequest.GetConversionFilePath();
    if (getConversionFilePath.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        LOG_DEBUG << "setting conversion path to: " << getConversionFilePath << endl;
        logCollectorConfiguration.SetConversionFilePath(getConversionFilePath);
    }
    const JsonValueBoxed<std::string>& getLogFilesDirectoryPath = jsonRequest.GetLogFilesDirectoryPath();
    if (getLogFilesDirectoryPath.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        LOG_DEBUG << "setting log output path to: " << getLogFilesDirectoryPath << endl;
        logCollectorConfiguration.SetLogFilesDirectoryPath(getLogFilesDirectoryPath);
    }

    // Get configuration (by value) and then submit it, allows us to keep the code thread safe (because the device can be destroyed
    // while this function holds the configuration object) and to keep atomicity (all parameters are set only when we know that all
    // of them are valid)
    os = Host::GetHost().GetDeviceManager().SubmitLogCollectorConfiguration(deviceName, cpuType, logCollectorConfiguration);
    if (!os)
    {
        jsonResponse.Fail(os.GetStatusMessage());
    }
}

void LogCollectorReSetConfigHandler::HandleRequest(const LogCollectorReSetConfigRequest & jsonRequest, LogCollectorReSetConfigResponse & jsonResponse)
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
    bool both = false;
    if (cpuType.GetState() == JsonValueState::JSON_VALUE_MISSING)
    {
        both = true;
    }
    else if (cpuType.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
    {
        jsonResponse.Fail("CpuType is wrong, it should be 'fw' or 'ucode'");
        return;
    }

    log_collector::LogCollectorConfiguration logCollectorConfiguration;
    logCollectorConfiguration.ResetToDefault();
    OperationStatus os;
    if (both || cpuType.GetValue() == CPU_TYPE_FW)
    {
        os = Host::GetHost().GetDeviceManager().SubmitLogCollectorConfiguration(deviceName, CPU_TYPE_FW, logCollectorConfiguration);
        if (!os)
        {
            jsonResponse.Fail(os.GetStatusMessage());
        }
    }
    if (both || cpuType.GetValue() == CPU_TYPE_UCODE)
    {
        os = Host::GetHost().GetDeviceManager().SubmitLogCollectorConfiguration(deviceName, CPU_TYPE_UCODE, logCollectorConfiguration);
        if (!os)
        {
            jsonResponse.Fail(os.GetStatusMessage());
        }
    }
}
