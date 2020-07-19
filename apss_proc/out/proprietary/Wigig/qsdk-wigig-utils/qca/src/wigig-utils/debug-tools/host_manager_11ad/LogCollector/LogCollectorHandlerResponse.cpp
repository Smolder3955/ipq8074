/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorHandlerResponse.h"
#include <string>
#include <json/json.h>
#include "Utils.h"

using namespace std;


/*********************************** JsonResponseBase *****************************************************/
string JsonResponseBase::Serialize()
{
    SerializeInternal();
    m_root["Success"] = m_success;
    m_root["Timestamp"] = Utils::GetCurrentLocalTimeString();

    Json::StyledWriter writer;
    return writer.write(m_root);
}

void JsonResponseBase::Failed(const char* failMessage)
{
    m_success = false;
    m_root["FailMessage"] = failMessage;
}

void JsonResponseBase::SerializeInternal()
{
    m_root["$type"] = GetTypeName();
    m_root["Group"] = GetGroupName();
    m_root["OpCode"] = m_opCode;
}

/*********************************** LogCollectorResponse *****************************************************/
LogCollectorResponse::LogCollectorResponse(const char* opCode)
    : JsonResponseBase()
{
    m_opCode = opCode;
}

void LogCollectorResponse::SerializeInternal()
{
    m_root["$type"] = GetTypeName();
    m_root["Group"] = GetGroupName();
    m_root["OpCode"] = m_opCode;
}
