/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <string>

#include "Host.h"
#include "HostGetVersionHandler.h"
#include "SharedVersionInfo.h"

using namespace std;

void HostGetVersionHandler::HandleRequest(const Json::Value& jsonRequest, HostGetVersionResponse& jsonResponse)
{
    LOG_DEBUG << "Host commands, get_version" << std::endl;

    (void)jsonRequest;
    string version = GetToolsVersion();

    jsonResponse.SetVersion(version);

}

void HostGetVersionResponse::SetVersion(const std::string& version)
{
    m_jsonResponseValue["Version"] = version;
}
