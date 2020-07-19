/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <iostream>
#include <sstream>
#include <memory>
#include <set>
#include <string>

#include "LogHandler.h"
#include "CommandHandler.h"
#include "LogCollectorCommandBuilder.h"
#include "PmcCommandBuilder.h"
#include "HostCommandBuilder.h"
#include "AccessCommandBuilder.h"

#include "TcpClient.h"
#include "SharedVersionInfo.h"

using namespace std;

namespace
{
    string GetFilteredResponse(const Json::Value & responseRoot, const set<string> & stringsFilter)
    {
        stringstream ss;
        ss << std::left;
        for (string k : responseRoot.getMemberNames())
        {
            if (stringsFilter.count(k) < 1)
            {
                if (responseRoot[k].isUInt() || responseRoot[k].isUInt64())
                {
                    // we want to print numeric values in its 0x format for these groups.
                    if (responseRoot.isMember("Group") && (responseRoot["Group"].asString() == "pmc" || (responseRoot["Group"].asString() == "access")))
                    {
                        ss << std::setw(20) << k + ":" << "0x" << std::hex << responseRoot[k].asLargestUInt() << endl;
                    }
                    else
                    {
                        ss << std::setw(20) << k + ":" << responseRoot[k].asInt() << endl;
                    }
                }
                else
                {
                    // Dealing with an array. We want to print numeric values as 0x format.
                    if (responseRoot[k].isArray())
                    {
                        stringstream si;
                        bool first = true;
                        for (const auto &s : responseRoot[k])
                        {
                            if (first)
                            {
                                // if it is the first value do not put ','
                                first = false;
                            }
                            else
                            {
                                si << ", ";
                            }

                            if (s.isUInt() || s.isUInt64())
                            {
                                si << "0x" << std::hex << s.asLargestUInt();
                            }
                            else
                            {
                                si << s.asString();
                            }

                        }
                        ss << std::setw(20) << k + ":" << si.str() << endl;
                    }
                    else
                    {
                        ss << std::setw(20) << k + ":" << responseRoot[k].asString() << endl;
                    }
                }
            }
        }
        return ss.str();
    }

    /*
    Input:
        jsoneResponse - string that represent a JSON message
    Output:
        bool -  true if jsonResponse is a valid JSON object and the value of the
                    key "Success" is true
                false if jsonResponse is not a valid JSON object or the value of the
                    key "Success" is false
    */
    bool IsJsonSuccess(string jsonResponse)
    {
        Json::Value responseRoot;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(jsonResponse, responseRoot);
        if (!parsingSuccessful)
        {
            return false;
        }
        return (responseRoot.isMember("Success") && responseRoot["Success"].asBool());
    }

    string ConvertJsonResponseToUserFormat(const string & jsonResponse)
    {
        const set<string> successKeyFilter = { "$type", "BeginTimestamp", "EndTimestamp", "Group", "OpCode", "Success", "HostVersion" };
        const set<string>  failKeyFilter = { "$type", "Success" };
        Json::Value responseRoot;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse(jsonResponse, responseRoot);
        if (!parsingSuccessful)
        {
            return "Failed to parse json response, Error is:" + reader.getFormattedErrorMessages();
        }
        stringstream ss;
        ss << std::left;
        if (responseRoot.isMember("Success") && responseRoot["Success"].asBool())
        {
            ss << std::setw(20) << "Completion status:" << "Success" << endl;
            ss << GetFilteredResponse(responseRoot, successKeyFilter);
        }
        else
        {
            ss << std::setw(20) << "Completion status:" << "Fail" << endl;
            ss << GetFilteredResponse(responseRoot, failKeyFilter);
        }

        return ss.str();
    }
}

CommandHandlerResult CommandHandler::HandleCommand(CommandLineArgs& commandLineArgs)
{
    if (commandLineArgs.IsGeneralHelpRequired())
    {
        // No group selected or first argument is -h / --help
        PrintGeneralHelp();

        // Not an Error if Group name not provided just help printed.
        return CH_SUCCESS;
    }
    if (commandLineArgs.VersionRequired())
    {
        // first argument is -v / --version
        PrintVersion();
        return CH_SUCCESS;
    }
    // If not IsGeneralHelpRequired we expect a valid Group code
    if (m_groupCommandBuilders.find(commandLineArgs.Group()) == m_groupCommandBuilders.end())
    {
        // Group name is not in m_groupCommandBuilders print relevant help and return error
        LOG_ERROR << "Invalid command group: " << commandLineArgs.Group() << endl;
        PrintGeneralHelp();
        return CH_ERROR;
    }

    // Now, we know that the Group code is valid.
    if (!commandLineArgs.ArgsAreValid())
    {
        // If we got here that means that the command group is valid and we can ask for more precise help message
        PrintCommandHelp(commandLineArgs);
        return CH_ERROR;
    }

    string outCommandOrHelp;
    if (!GetCommandOrHelpMessage(commandLineArgs, outCommandOrHelp))
    {
        //in this case Generate command fail so the help text is in the command out
        cout << outCommandOrHelp << endl;
        return CH_ERROR;
    }

    // We got a valid command to send
    TcpClient tcpClient;
    string response;
    if (!tcpClient.ConnectSendCommandGetResponse(outCommandOrHelp, commandLineArgs.Target(), commandLineArgs.Port(), response))
    {
        // Connection Error. Error Message is provided by tcpClient
        return CH_CONNECTION_ERROR;
    }

    if (!ValidateHostResponse(response))
    {
        return CH_ERROR;
    }

    // We got a response, print it in the requested format
    cout << (commandLineArgs.JsonResponse() ? response : ConvertJsonResponseToUserFormat(response)) << endl;
    if (IsJsonSuccess(response))
    {
        // Command executed successfully
        return CH_SUCCESS;
    }
    else
    {
        // Command was not executed successfully, but we got a valid JSON response (Failure in host manager)
        return CH_FAIL;
    }
}

void CommandHandler::PrintGeneralHelp()
{
    // Create a detailed help message with all available group codes
    stringstream availableGroupsHelp;
    for (auto it = m_groupCommandBuilders.begin(); it != m_groupCommandBuilders.end(); ++it)
    {
        availableGroupsHelp << (it->first) << "\n";
    }

    // Use help template to decorate the Help message
    cout << GroupCommandBuilderBase::HelpTemplate(availableGroupsHelp.str());
}

void CommandHandler::PrintVersion()
{
    cout << "802.11ad CLI version: " << GetToolsVersion() << endl;
    cout << GetToolsBuildInfo() << endl;
}

void CommandHandler::PrintCommandHelp(const CommandLineArgs & commandLineArgs)
{
    string helpString;
    // Not checking return value because we know we are in HELP flow.
    GetCommandOrHelpMessage(commandLineArgs, helpString); // will go through the command layers to construct the help message
    cout << helpString << endl;
}

bool CommandHandler::ValidateHostResponse(const string & response)
{
    Json::Value responseRoot;
    Json::Reader reader;
    if (response.empty())
    {
        LOG_ERROR << "host_manager_11ad version is not supported. Expected host_manager_11ad version: " << GetToolsVersion() << endl;
        return false;
    }
    bool parsingSuccessful = reader.parse(response, responseRoot);
    if (!parsingSuccessful)
    {
        // Should not happen
        LOG_ERROR << "Failed to parse json response, \nRaw response is: " << response << " Error is:" + reader.getFormattedErrorMessages() << endl;
        LOG_ERROR << "Expected host_manager_11ad version: " << GetToolsVersion() << endl;
        return false;
    }
    if (!responseRoot.isMember("HostVersion"))
    {
        LOG_ERROR << "Failed to validate host response. \nRaw Response is: " << response << " \nPlease update host_manager_11ad on target." << endl;
        LOG_ERROR << "Expected host_manager_11ad version: " << GetToolsVersion() << endl;
        return false;
    }
    if (responseRoot["HostVersion"].asString() != GetToolsVersion())
    {
        LOG_ERROR << "Unexpected host_manager_11ad. shell_11ad version is: " << GetToolsVersion() << " host_manager_11ad version on target is: " << responseRoot["HostVersion"].asString() << endl;
        LOG_ERROR << "host_manager_11ad version should be identical to shell_11ad version" << endl;
        return false;
    }
    return true;
}


bool CommandHandler::GetCommandOrHelpMessage(const CommandLineArgs& commandLineArgs, string& command)
{
    // At this point we know that group exists, so it is safe to use HandleCommand
    return (m_groupCommandBuilders[commandLineArgs.Group()]->HandleCommand(commandLineArgs, command));
}

void CommandHandler::SetHandlers()
{
    // Add here all command builders.
    // Map values are copy protected you'll get a compilation error if you try reference it.
    m_groupCommandBuilders.insert(make_pair("log_collector", unique_ptr<LogCollectorCommandBuilder>(new LogCollectorCommandBuilder())));
    m_groupCommandBuilders.insert(make_pair("pmc", unique_ptr<PmcCommandBuilder>(new PmcCommandBuilder())));
    m_groupCommandBuilders.insert(make_pair("host", unique_ptr<HostCommandBuilder>(new HostCommandBuilder())));
    m_groupCommandBuilders.insert(make_pair("access", unique_ptr<AccessCommandBuilder>(new AccessCommandBuilder())));

}
