/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <string>

#include "Host.h"
#include "ReadHandler.h"
#include "DebugLogger.h"
#include "DeviceManager.h"

using namespace std;

void ReadHandler::HandleRequest(const ReadRequest& jsonRequest, ReadResponse& jsonResponse)
{
    LOG_VERBOSE << "Access commands, read" << std::endl;
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

    JsonValueBoxed<uint32_t> address = jsonRequest.GetReadAddress();

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
    // We know that the address is provided and valid.
    LOG_VERBOSE << "Reading from: " << Address(address) << endl;
    DWORD val;
    const DeviceManagerOperationStatus dmos = Host::GetHost().GetDeviceManager().Read(deviceName,address, val);
    if (dmos != dmosSuccess)
    {
        stringstream failMessage;
        failMessage << "Fail to read from address: " << Address(address) << std::endl;
        jsonResponse.Fail(failMessage.str());
        return;
    }
    jsonResponse.SetValue(val);
}

void ReadResponse::SetValue(uint32_t value)
{
    m_jsonResponseValue["Value"] = value;
}
