/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <string>

#include "Host.h"
#include "ReadBlockHandler.h"
#include "DebugLogger.h"
#include "OperationStatus.h"
#include "DeviceManager.h"

using namespace std;

void ReadBlockHandler::HandleRequest(const ReadBlockRequest& jsonRequest, ReadBlockResponse& jsonResponse)
{
    LOG_VERBOSE << "Access commands, ReadBlock" << std::endl;
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

    JsonValueBoxed<uint32_t> blockSize = jsonRequest.GetBlockSize();

    if (blockSize.GetState() == JsonValueState::JSON_VALUE_MISSING)
    {
        jsonResponse.Fail("size is not provided");
        return;
    }
    if (blockSize.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
    {
        jsonResponse.Fail("size is Malformed");
        return;
    }

    // We know that the address is provided and valid.
    LOG_VERBOSE << "Reading from: " << Address(address) << endl;
    std::vector<DWORD> values;
    DeviceManagerOperationStatus dmos = Host::GetHost().GetDeviceManager().ReadBlock(deviceName, address, blockSize, values);
    if (dmos != dmosSuccess)
    {
        stringstream failMessage;
        failMessage << "Fail to read from address: " << Address(address) << std::endl;
        jsonResponse.Fail(failMessage.str());
        return;
    }

    jsonResponse.AddValuesToJson(values);

}


void ReadBlockResponse::AddValuesToJson(const std::vector<DWORD>& valuesVec)
{
    Json::Value values(Json::arrayValue);
    for (const auto& value : valuesVec)
    {
        values.append(static_cast<uint32_t>(value));
    }
    m_jsonResponseValue["Values"] = values;
}


