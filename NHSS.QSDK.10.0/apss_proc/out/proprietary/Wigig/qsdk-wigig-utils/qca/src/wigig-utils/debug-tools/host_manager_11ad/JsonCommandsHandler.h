/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _JSONCOMMANDSHANDLER_H_
#define _JSONCOMMANDSHANDLER_H_

#include <memory>
#include <map>
#include <string>

#include "HostDefinitions.h"
#include "JsonHandlerSDK.h"

// Forward declarations:
namespace Json { class Value; }

// *************************************************************************************************

class JsonCommandsHandler
{
public:
    /*
    * The constructor inserts each one of the available functions into the map - m_groupFunctionHandler - according to server type (TCP/UDP)
    */
    explicit JsonCommandsHandler(ServerType type);

    ConnectionStatus ExecuteCommand(const std::string& message, std::string &referencedResponse);

private:

    void HandleError(const char* errorMessage, std::string &referencedResponse);
    // m_groupFunctionHandler is a map that maps a string = group name, to a function
    std::map<std::string, std::unique_ptr<IJsonHandler>> m_groupHandlerMap;
    Json::StyledWriter m_jsonWriter;
};


#endif // !_JSONCOMMANDSHANDLER_H_
