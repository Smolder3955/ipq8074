/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _COMMAND_HANDLER_
#define _COMMAND_HANDLER_

#include <memory>
#include <string>
#include "CommandLineArgs.h"
#include "GroupCommandBuilderBase.h"


enum CommandHandlerResult
{
    CH_SUCCESS = 0,
    CH_FAIL = 1,
    CH_ERROR = 2,
    CH_CONNECTION_ERROR = 3
};

class CommandHandler
{
public:
    CommandHandler() { SetHandlers(); }
    /*
    This is the function that perform the whole flow.
    It gets the command line args and return the output that should be displayed to the user.
    */
    CommandHandlerResult HandleCommand(CommandLineArgs& commandLineArgs);

private:
    void SetHandlers();
    // Add the command generator unique pointer to the map m_groupCommandBuilders.
    std::map<std::string, std::unique_ptr<GroupCommandBuilderBase> > m_groupCommandBuilders;
    /*
    Input:
    commandLineArgs - object that contains all the parameter to construct the command.
    command - out param to get the new command or help message
    Output:
    bool -  true if the command should be sent.
    false if help message was returned or error occurred.
    */
    bool GetCommandOrHelpMessage(const CommandLineArgs& commandLineArgs, std::string& outString);
    void PrintGeneralHelp();
    void PrintVersion();
    void PrintCommandHelp(const CommandLineArgs& commandLineArgs);
    bool ValidateHostResponse(const std::string& commandLineArgs);
};



#endif
