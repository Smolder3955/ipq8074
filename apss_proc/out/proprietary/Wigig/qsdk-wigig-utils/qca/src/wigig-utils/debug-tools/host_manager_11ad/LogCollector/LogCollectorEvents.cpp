/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorEvents.h"
#include <json/json.h>

void SerializableLogLine::ToJsonImpl(Json::Value& valueToUpdate) const
{
    valueToUpdate["Module"] = m_module;
    valueToUpdate["Level"] = m_level;
    valueToUpdate["StringOffset"] = m_strOffset;
    valueToUpdate["Signature"] = m_signature;
    valueToUpdate["IsString"] = m_isString;
    Json::Value parametersArray(Json::arrayValue);
    for (auto parameter : m_dwordArray)
    {
        parametersArray.append(parameter);
    }
    valueToUpdate["Parameters"] = parametersArray;
    valueToUpdate["MLLR"] = m_missingLogsReason;
    valueToUpdate["NOMLLS"] = m_numOfMissingLogDwords;
}

NewLogsEvent::NewLogsEvent(const std::string& deviceName, const log_collector::CpuType& cpuType, const std::vector<log_collector::RawLogLine>& logLines) :
    HostManagerEventBase(deviceName),
    m_cpuType(cpuType)
{
    m_logLines.reserve(logLines.size());
    for (const auto& logLine : logLines)
    {
        m_logLines.emplace_back(new SerializableLogLine(logLine));
    }
}

void NewLogsEvent::ToJsonInternal(Json::Value& root) const
{
    std::stringstream cpuTypeSs;
    cpuTypeSs << m_cpuType;
    root["TracerType"] = cpuTypeSs.str();

    Json::Value linesArray(Json::arrayValue);
    for (const auto & line : m_logLines)
    {
        Json::Value lineValue;
        line->ToJsonImpl(lineValue);
        linesArray.append(lineValue);
    }
    root["LogLines"] = linesArray;
}
