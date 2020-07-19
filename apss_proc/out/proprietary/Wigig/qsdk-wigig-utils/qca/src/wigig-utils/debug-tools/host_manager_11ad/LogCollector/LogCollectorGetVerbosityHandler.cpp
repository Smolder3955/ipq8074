/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorGetVerbosityHandler.h"
#include "LogCollector.h"
#include "Host.h"
#include "DeviceManager.h"

void LogCollectorGetVerbosityHandler::HandleRequest(const LogCollectorGetVerbosityRequest& jsonRequest, LogCollectorGetVerbosityResponse& jsonResponse)
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

    LOG_DEBUG << "Log Collector get verbosity request for Device: " << deviceName << " with CPU type: " << cpuType << std::endl;

    std::string value;
    for (int i = 0; i < log_collector::NUM_MODULES; i++)
    {
        OperationStatus os = Host::GetHost().GetDeviceManager().GetLogCollectionModuleVerbosity(deviceName, cpuType, log_collector::module_names[i], value);
        if (!os)
        {
            jsonResponse.Fail(os.GetStatusMessage());
            return;
        }
        jsonResponse.SetModuleVerbosity(log_collector::module_names[i], value);
    }
}

