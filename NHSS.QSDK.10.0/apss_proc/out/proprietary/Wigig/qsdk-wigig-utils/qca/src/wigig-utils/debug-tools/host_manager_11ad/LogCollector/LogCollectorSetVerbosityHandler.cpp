/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <sstream>

#include "LogCollectorSetVerbosityHandler.h"
#include "Host.h"
#include "OperationStatus.h"
#include "DeviceManager.h"

void LogCollectorSetVerbosityHandler::HandleRequest(const LogCollectorSetVerbosityRequest& jsonRequest, LogCollectorSetVerbosityResponse& jsonResponse)
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

    LOG_DEBUG << "Log Collector set verbosity request for Device: " << deviceName << " with CPU type: " << cpuType << std::endl;

    JsonValueBoxed<std::string> getDefaultVerbosity = jsonRequest.GetDefaultVerbosity();
    OperationStatus os;

    for (int i = 0; i < log_collector::NUM_MODULES; i++)
    {
        const JsonValueBoxed<std::string> getModuleVerbosity = jsonRequest.GetModuleVerbosity(log_collector::module_names[i]);

        if (getModuleVerbosity.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
        {
            os = Host::GetHost().GetDeviceManager().SetLogCollectionModuleVerbosity(deviceName, cpuType, log_collector::module_names[i], getModuleVerbosity);
            if (!os)
            {
                jsonResponse.Fail(os.GetStatusMessage());
                return;
            }
        }
        else if (getDefaultVerbosity.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
        {
            os = Host::GetHost().GetDeviceManager().SetLogCollectionModuleVerbosity(deviceName, cpuType, log_collector::module_names[i], getDefaultVerbosity);
            if (!os)
            {
                jsonResponse.Fail(os.GetStatusMessage());
                return;
            }
        }
    }


}
