/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <set>
#include <string>

#include "Host.h"
#include "HostGetInterfacesHandler.h"
#include "DeviceManager.h"

void HostGetInterfacesHandler::HandleRequest(const Json::Value& jsonRequest, HostGetInterfacesResponse& jsonResponse)
{
    (void)jsonRequest;
    LOG_DEBUG << "Host commands, get_interfaces"<<  std::endl;
    std::set<std::string> interfaces;
    DeviceManagerOperationStatus dmos = Host::GetHost().GetDeviceManager().GetDevices(interfaces);
    if (dmos != dmosSuccess)
    {
        jsonResponse.Fail("Fail to get interfaces");
        return;
    }
    LOG_DEBUG << "There are " << interfaces.size() << " interfaces"  << std::endl;
    for (const std::string& i : interfaces)
    {
        jsonResponse.AddInterface(i);
    }
}

void HostGetInterfacesResponse::AddInterface(const std::string& interfaceName)
{
    m_jsonResponseValue["Interfaces"].append(interfaceName);
}
