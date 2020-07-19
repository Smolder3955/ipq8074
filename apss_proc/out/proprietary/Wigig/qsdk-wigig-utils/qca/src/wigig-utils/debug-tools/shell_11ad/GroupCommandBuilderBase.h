/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _COMMAND_BUILDER_BASE_
#define _COMMAND_BUILDER_BASE_

#include <string>
#include <map>
#include <memory>
#include "CommandLineArgs.h"
#include "OpCodeGeneratorBase.h"


//An abstract class that every specific m_group command builder derive from.
class GroupCommandBuilderBase
{
public:
    virtual ~GroupCommandBuilderBase() {};

    // returns True only if the messageToSend was constructed successfully and should be sent to hostmanager.
    // otherwise returns false and the messagToSend contains help message and error message is printed if needed.
    bool HandleCommand(const CommandLineArgs& commandLineArgs, std::string& messageToSend);

    // returns specific help text according to the known arguments and the help text received from the lower
    // level command handlers.
    static std::string HelpTemplate(std::string helpText, std::string group="", std::string opcode="");

protected:
    // Try to generate JSON command. If something go wrong return false and messageToSend contains help message.
    bool GetCommandOrHelpMessage(const CommandLineArgs& commandLineArgs, std::string& messageToSend);

    // Generate a string contains all OpCodes and descriptions
    std::string GenerateGroupHelpMessage(const CommandLineArgs& commandLineArgs) const;

    // These parameters appears in every JSON command sent by the CLI application
    void SetCommonParams(const CommandLineArgs& commandLineArgs, Json::Value& messageRoot) const;
    void RegisterOpcodeGenerators(std::string opcodeGeneratorName, std::unique_ptr<OpCodeGeneratorBase>&& upOpcodeGenerator);

private:
    // Must be implemented and called from the constructor of the derived class
    virtual void SetOpcodeGenerators() = 0;
    std::map<const std::string, std::unique_ptr<OpCodeGeneratorBase> > m_opcodeGenerators;
};



#endif