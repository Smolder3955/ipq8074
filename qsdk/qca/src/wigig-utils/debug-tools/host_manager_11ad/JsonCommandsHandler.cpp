/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <string>
#include <map>
#include <memory>
#include "HostDefinitions.h"
#include "JsonCommandsHandler.h"
#include "PmcCommandHandler/PmcCommandDispatcher.h"
#include "HostCommandsHandler/HostCommandsDispatcher.h"
#include "AccessCommandsHandler/AccessCommandsDispatcher.h"
#include "LogCollector/LogCollectorCommandDispatcher.h"
#include "SensingCommandHandler/SensingCommandDispatcher.h"

using namespace std;

// *************************************************************************************************

JsonCommandsHandler::JsonCommandsHandler(ServerType type)
{
    if (stTcp == type) // TCP server
    {
        m_groupHandlerMap.insert(make_pair("log_collector", std::unique_ptr<IJsonHandler>(new LogCollectorCommandDispatcher())));
        m_groupHandlerMap.insert(make_pair("pmc", std::unique_ptr<IJsonHandler>(new PmcCommandDispatcher())));
        m_groupHandlerMap.insert(make_pair("host", std::unique_ptr<IJsonHandler>(new HostCommandsDispatcher())));
        m_groupHandlerMap.insert(make_pair("access", std::unique_ptr<IJsonHandler>(new AccessCommandsDispatcher())));
        m_groupHandlerMap.insert(make_pair("sensing", std::unique_ptr<IJsonHandler>(new SensingCommandDispatcher())));
    }
    else // UDP server
    {
        // An example for when UDP would return here.
        //m_groupFunctionHandler.insert(make_pair(/*"get_host_network_info"*/"GetHostIdentity", &CommandsHandler::GetHostNetworkInfo));
    }
}
// *************************************************************************************************

void JsonCommandsHandler::HandleError(const char* errorMessage, std::string &referencedResponse)
{
    LOG_ERROR << errorMessage << endl;

    Json::Value jsonResponseValue;
    JsonErrorResponse jsonResponse(errorMessage, jsonResponseValue);
    referencedResponse = m_jsonWriter.write(jsonResponseValue);
}
// *************************************************************************************************

ConnectionStatus JsonCommandsHandler::ExecuteCommand(const string& jsonString, string &referencedResponse)
{
    LOG_VERBOSE << ">>> Json String (command):\n" << jsonString << std::endl;

    Json::Value root;
    Json::Reader reader;
    ConnectionStatus connectionStatus = KEEP_CONNECTION_ALIVE;
    try
    {
        if (!reader.parse(jsonString, root))
        {
            HandleError(("Error parsing the json command: " + jsonString).c_str(), referencedResponse);
            return connectionStatus;
        }

        LOG_DEBUG << ">>> JSON Request:\n" << root << std::endl;

        // Check if we should keep the connection alive by JSON value
        JsonValueBoxed<bool> keepConnectionBoxed = JsonValueParser::ParseBooleanValue(root, "KeepConnectionAlive");
        if (keepConnectionBoxed.GetState() == JsonValueState::JSON_VALUE_MALFORMED)
        {
            LOG_WARNING << "Could not parse KeepConnectionAlive value, root is:" << root << "\nkeeping connection alive by default" << std::endl;
        }
        else if (keepConnectionBoxed.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
        {
            // Only  if the parameter is provided and valid check the bool value and update the connectionStatus accordingly.
            connectionStatus = keepConnectionBoxed.GetValue() ? KEEP_CONNECTION_ALIVE : CLOSE_CONNECTION;
        }

        string groupName = root["Group"].asString();
        if (groupName.empty())
        {
            HandleError("Error parsing the json command - 'Group' field is missing", referencedResponse);
            return connectionStatus;
        }

        auto groupHandlerIter = m_groupHandlerMap.find(groupName);
        if (groupHandlerIter == m_groupHandlerMap.end())
        { //There's no such a group, the return value from the map would be null
            HandleError(("Unknown Group: " + groupName).c_str(), referencedResponse);
            return connectionStatus;
        }

        Json::Value jsonResponseValue;
        groupHandlerIter->second->HandleJsonCommand(root, jsonResponseValue); //call the function that fits groupName

        LOG_DEBUG << ">>> JSON Response: " << jsonResponseValue << std::endl;

        referencedResponse = m_jsonWriter.write(jsonResponseValue);
        return connectionStatus;
    }
    catch (const Json::LogicError &e)
    {
        stringstream ss;
        ss << "Failed to parse Json message: " << jsonString << ", error: " << e.what();
        HandleError(ss.str().c_str(), referencedResponse);
        return connectionStatus;
    }
    catch (const std::exception& e)
    {
        stringstream ss;
        ss << "Exception during request handling : " << e.what();
        HandleError(ss.str().c_str(), referencedResponse);
        return connectionStatus;
    }
}
