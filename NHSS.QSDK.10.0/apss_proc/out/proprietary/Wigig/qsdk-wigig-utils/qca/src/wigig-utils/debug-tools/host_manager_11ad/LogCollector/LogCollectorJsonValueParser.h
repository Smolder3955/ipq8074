/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_JSON_VALUE_PARSER_H_
#define _LOG_COLLECTOR_JSON_VALUE_PARSER_H_
#pragma once

#include "JsonHandlerSDK.h"
#include "LogCollectorDefinitions.h"
using namespace log_collector;
class LogCollectorJsonValueParser
{
public:
    static JsonValueBoxed<CpuType> ParseCpuType(const Json::Value& jsonValue, const char* szKey)
    {
        const Json::Value& value = jsonValue[szKey];
        if (Json::Value::nullSingleton() == value)
        {
            return JsonValueBoxed<CpuType>(JsonValueState::JSON_VALUE_MISSING);
        }

        if (value.asString() == "fw")
        {
            return JsonValueBoxed<CpuType>(CPU_TYPE_FW);
        }
        if (value.asString() == "ucode")
        {
            return JsonValueBoxed<CpuType>(CPU_TYPE_UCODE);
        }
        if (value.asString() == "both")
        {
            return JsonValueBoxed<CpuType>(CPU_TYPE_BOTH);
        }
        return JsonValueBoxed<CpuType>(JsonValueState::JSON_VALUE_MALFORMED);


    }

    static JsonValueBoxed<log_collector::RecordingType> ParseRecordingType(const Json::Value& jsonValue, const char* szKey)
    {
        const Json::Value& value = jsonValue[szKey];
        if (Json::Value::nullSingleton() == value)
        {
            return JsonValueBoxed<log_collector::RecordingType>(JsonValueState::JSON_VALUE_MISSING);
        }
        if (value.asString() == "raw")
        {
            return JsonValueBoxed<log_collector::RecordingType>(log_collector::RECORDING_TYPE_RAW);
        }
        if (value.asString() == "txt")
        {
            return JsonValueBoxed<log_collector::RecordingType>(log_collector::RECORDING_TYPE_TXT);
        }
        if (value.asString() == "both")
        {
            return JsonValueBoxed<log_collector::RecordingType>(log_collector::RECORDING_TYPE_BOTH);
        }
        return JsonValueBoxed<log_collector::RecordingType>(JsonValueState::JSON_VALUE_MALFORMED);
    }

};

#endif  // _LOG_COLLECTOR_JSON_VALUE_PARSER_H_
