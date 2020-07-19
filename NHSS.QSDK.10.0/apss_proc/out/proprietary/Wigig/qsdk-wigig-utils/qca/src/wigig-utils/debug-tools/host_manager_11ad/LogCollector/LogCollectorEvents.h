/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_EVENTS_H_
#define _LOG_COLLECTOR_EVENTS_H_

#include <memory>
#include "EventsDefinitions.h"
#include "RawLogLine.h"
#include "LogCollector/LogCollectorDefinitions.h"

// forward declarations
namespace Json
{
    class Value;
}

class SerializableLogLine final : public log_collector::RawLogLine, public JsonSerializable
{
public:
    explicit SerializableLogLine(RawLogLine logLine) : RawLogLine(logLine) {}

    void ToJsonImpl(Json::Value& valueToUpdate) const override;
};

class NewLogsEvent final : public HostManagerEventBase
{
public:
    NewLogsEvent(const std::string& deviceName, const log_collector::CpuType& cpuType, const std::vector<log_collector::RawLogLine>& logLines);

private:
    log_collector::CpuType m_cpuType;
    std::vector<std::unique_ptr<SerializableLogLine>> m_logLines;

    const char* GetTypeName() const override { return "NewLogsEvent"; }

    void ToJsonInternal(Json::Value& root) const override;
};

#endif // _LOG_COLLECTOR_EVENTS_H_
