/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <sstream>
#include <iostream>
#include "json/json.h"
#include "GroupCommandBuilderBase.h"

using namespace std;

bool GroupCommandBuilderBase::GetCommandOrHelpMessage(const CommandLineArgs & commandLineArgs, string & messageToSend)
{
    auto fit = m_opcodeGenerators.find(commandLineArgs.OpCode());
    if (!fit->second->ValidateProvidedParams(commandLineArgs))
    {
        return false;
    }

    Json::Value messageRoot;
    SetCommonParams(commandLineArgs, messageRoot);
    if (fit->second->AddParmsToJson(messageRoot, commandLineArgs))
    {
        Json::StyledWriter writer;
        messageToSend = writer.write(messageRoot);
        return true;
    }

    // in this case the message is not sent just help text displayed
    string opcodeHelpMessage = fit->second->GenerateHelpMessage();
    messageToSend = HelpTemplate(opcodeHelpMessage, commandLineArgs.Group(), commandLineArgs.OpCode());
    return false;
}


bool GroupCommandBuilderBase::HandleCommand(const CommandLineArgs & commandLineArgs, string & messageToSendOrHelp)
{
    if ((commandLineArgs.IsGroupHelpRequired()))
    {
        messageToSendOrHelp = GenerateGroupHelpMessage(commandLineArgs);
        return false;
    }
    if (commandLineArgs.OpCode().empty())
    {
        LOG_ERROR << "Missing OpCode. Use \' " << g_ApplicationName << " " << commandLineArgs.Group() << " -h \' for more information." << endl;
        return false;
    }

    auto fit = m_opcodeGenerators.find(commandLineArgs.OpCode());
    if (fit == m_opcodeGenerators.end())
    {
        // OpCode is not in m_opcodeGenerators
        LOG_ERROR << "Invalid command opcode: " << commandLineArgs.OpCode() << endl;
        return false;
    }
    if (commandLineArgs.IsParamHelpRequired())
    {
        // in this case the message is not sent just help text displayed
        messageToSendOrHelp = fit->second->GenerateHelpMessage();
        return false;
    }

    // Returns true and a message to send (JSON)
    // Otherwise return false and messageToSendOrHelp contains help
    return GetCommandOrHelpMessage(commandLineArgs, messageToSendOrHelp);
}




string GroupCommandBuilderBase::GenerateGroupHelpMessage(const CommandLineArgs & commandLineArgs) const
{
    stringstream availableOpCodesHelp;
    availableOpCodesHelp << left; // left make text aligned to the left
    for (auto it = m_opcodeGenerators.cbegin(); it != m_opcodeGenerators.cend(); ++it)
    {
        availableOpCodesHelp << setw(20) << (it->first) + ":" // setw set the width to 20 to make all values aligned
            << OpCodeGeneratorBase::FormatedOpcodeDescription(it->second->OpCodeDescription()) << endl;
    }
    return HelpTemplate(availableOpCodesHelp.str(), commandLineArgs.Group());
}


string GroupCommandBuilderBase::HelpTemplate(string helpText, string group, string opcode)
{
    stringstream optionalParamsHelpText;
    optionalParamsHelpText << "Optional Params:\n" << "\t(-t/ --target),    Usage: -t=<machine name or ip>, if not provided default is localhost\n" <<
    "\t(-p/ --port),      Usage: -p=<port number>, if not provided default is 12346\n" <<
    "\t(-h/ --help),      Will print relevant help message, can be used in anywhere. The command will not be executed.\n" <<
    "\t(-j/ --json),      Reply message will be in JSON format\n" <<
    "\t(-d),              Set log verbosity level, usage: -d={0,1,2,3,4}\n\n";

    stringstream ss;
    ss << "\n##################    Wigig Shell Help    ##################\n";
    ss << "SYNOPSIS: \n";
    ss << g_ApplicationName <<" <Group> <OpCode> <ParmName-1>=<ParmValue-1> ... <ParmName-n>=<ParmValue-n> -<optional params>\n\n";

    if (!group.empty())
    {
        ss << "Help for Command Group: " << group << "\n";
    }
    else
    {
        ss << "The supported Groups are:\n" << helpText << "\n\n";
        ss << optionalParamsHelpText.str();
        ss << "For more information about specific Group please try: \n";
        ss << g_ApplicationName <<" <Group> -h \n";
        ss << "For example, use: \' " << g_ApplicationName << " log_collector -h \' for more information about log collection capabilities of host_manager_11ad. \n";
        return ss.str();
    }

    if (!opcode.empty())
    {
        ss << "Help for OpCode: " << opcode << "\n";
    }
    else
    {
        ss << "The supported opcodes for the provided group (" << group << ") are:\n" << helpText << "\n";
        ss << optionalParamsHelpText.str();
        ss << "For more information about specific OpCode please try: \n";
        ss << g_ApplicationName << " <Group> <OpCode> -h \n";
        ss << "For example, use: \' " << g_ApplicationName << " log_collector start_recording -h \' for more information about log recording. \n";
        return ss.str();
    }
    ss << "Opcode specific help:\n" << helpText << "\n";
    ss << optionalParamsHelpText.str();
    return ss.str();
}


void GroupCommandBuilderBase::SetCommonParams(const CommandLineArgs & commandLineArgs, Json::Value & messageRoot) const
{
    messageRoot["KeepConnectionAlive"] = false; // will make host manager close the connection after sending the response.
    messageRoot["Group"] = commandLineArgs.Group();
    messageRoot["OpCode"] = commandLineArgs.OpCode();
}

void GroupCommandBuilderBase::RegisterOpcodeGenerators(string opcodeGeneratorName, unique_ptr<OpCodeGeneratorBase>&& upOpcodeGenerator)
{
    m_opcodeGenerators.insert(make_pair(opcodeGeneratorName, move(upOpcodeGenerator)));
}
