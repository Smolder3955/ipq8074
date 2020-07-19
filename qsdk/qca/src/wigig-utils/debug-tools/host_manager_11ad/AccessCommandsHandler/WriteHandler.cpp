/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <string>

#include "Host.h"
#include "WriteHandler.h"
#include "DebugLogger.h"
#include "OperationStatus.h"
#include "DeviceManager.h"

using namespace std;

void WriteHandler::HandleRequest(const WriteRequest& jsonRequest, JsonBasicResponse& jsonResponse)
{
    LOG_VERBOSE << "Access commands, Write" << std::endl;
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

    JsonValueBoxed<uint32_t> address = jsonRequest.GetWriteAddress();

    if (address.GetState() == JsonValueState::JSON_VALUE_MISSING)
    {
        jsonResponse.Fail("Address is not provided");
        return;
    }
    if (address.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
    {
        jsonResponse.Fail("Address is Malformed");
        return;
    }

    const JsonValueBoxed<uint32_t> value = jsonRequest.GetValue();

    if (value.GetState() == JsonValueState::JSON_VALUE_MISSING)
    {
        jsonResponse.Fail("Value is not provided");
        return;
    }
    if (value.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
    {
        jsonResponse.Fail("Value is Malformed");
        return;
    }
    // We know that the address is provided and valid.
    LOG_VERBOSE << "Writing the value " << value << " to: " << Address(address) << endl;
    const DeviceManagerOperationStatus dmos = Host::GetHost().GetDeviceManager().Write(deviceName, address, value);
    if (dmos != dmosSuccess)
    {
        stringstream failMessage;
        failMessage << "Fail to write the value " << value << " to: " << Address(address) << endl;
        jsonResponse.Fail(failMessage.str());
    }
}

