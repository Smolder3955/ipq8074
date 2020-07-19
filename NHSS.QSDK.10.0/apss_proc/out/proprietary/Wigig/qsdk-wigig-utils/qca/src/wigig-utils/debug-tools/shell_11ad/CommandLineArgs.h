/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef COMMAND_LINE_ARGS_
#define COMMAND_LINE_ARGS_

#include <string>
#include <list>
#include <map>
#include <vector>
#include <sstream>
#include "LogHandler.h"


class CommandLineArgs
{
public:
    CommandLineArgs(char* args[], int argc);

    // Find the parameter string value in m_paramMap and return it as outVal
    // Return true if param name was found and false otherwise
    bool FindParam(const std::string& paramName, std::string& outVal) const;
    const std::string& Group() const { return m_group; };
    const std::string& OpCode() const { return m_opCode; };
    const std::string& Target() const { return m_target; };
    const std::string& Port() const { return m_port; };
    bool IsGroupHelpRequired() const { return m_isGroupHelpRequired; };
    bool IsParamHelpRequired() const { return m_isParamHelpRequired; };
    bool IsGeneralHelpRequired() const { return m_isGeneralHelpRequired; };
    bool JsonResponse() const { return m_jsonResponse; };
    bool ArgsAreValid() const { return m_argsAreValid; };
    bool VersionRequired() const { return m_isVersionRequired; };
    void GetParamsMapKeys(std::vector<std::string> &outVec) const;

private:
    bool AddParam(const char* param);
    std::string m_group;
    std::string m_opCode;
    std::string m_target;
    std::string m_port;

    bool m_isGeneralHelpRequired;
    bool m_isGroupHelpRequired;
    bool m_isParamHelpRequired;
    bool m_jsonResponse;
    bool m_argsAreValid;
    bool m_isVersionRequired;

    std::map<std::string, std::string> m_paramMap;
};

#endif
