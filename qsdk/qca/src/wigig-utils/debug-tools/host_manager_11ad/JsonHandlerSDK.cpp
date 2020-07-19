/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "JsonHandlerSDK.h"
#include <json/json.h>
#include <string>
#include <map>
#include <memory>

JsonOpCodeDispatcher::JsonOpCodeDispatcher()
{
}

void JsonOpCodeDispatcher::RegisterOpCodeHandler(const char* szOpCode, std::unique_ptr<IJsonHandler> spJsonHandler)
{
    m_opcodeHandlerMap.insert(
        std::make_pair(std::string(szOpCode), std::move(spJsonHandler)));
}

void JsonOpCodeDispatcher::HandleJsonCommand(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue)
{
    std::string opcode = jsonRequestValue["OpCode"].asString();
    if (opcode.empty())
    {
        JsonErrorResponse jsonResponse("Invalid JSON request, opcode key not found in command", jsonResponseValue);
        return;
    }

    auto opcodeHandlerIter = m_opcodeHandlerMap.find(opcode);
    if (opcodeHandlerIter == m_opcodeHandlerMap.end())
    {
        std::stringstream errorInfoSs;
        errorInfoSs << "Requested opcode {"<< opcode << "} is not supported. Supported opcodes are: ";
        for (auto iter = m_opcodeHandlerMap.begin(); iter != m_opcodeHandlerMap.end(); ++iter)
        {
            errorInfoSs << iter->first << ", ";
        }

        JsonOpCodeDispatchErrorResponse response(jsonRequestValue, jsonResponseValue);
        return;
    }

    opcodeHandlerIter->second->HandleJsonCommand(jsonRequestValue, jsonResponseValue);
}