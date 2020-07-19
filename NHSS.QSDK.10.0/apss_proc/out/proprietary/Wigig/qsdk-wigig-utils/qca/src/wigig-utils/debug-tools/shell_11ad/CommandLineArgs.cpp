/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <iostream>
#include <locale>
#include <string>
#include <algorithm>
#include <stdlib.h>
#include "CommandLineArgs.h"
#include "Utils.h"

using namespace std;

CommandLineArgs::CommandLineArgs(char* args[], int argc):
    m_target("127.0.0.1"), m_port("12346"),
    m_isGeneralHelpRequired(false), m_isGroupHelpRequired(false),
    m_isParamHelpRequired(false), m_jsonResponse(false),
    m_argsAreValid(false), m_isVersionRequired(false)
{
    // If no args (but the program name) entered generate help.
    if (argc < 2)
    {
        m_isGeneralHelpRequired = true;
        return;
    }
    m_group = args[1];
    transform(m_group.begin(), m_group.end(), m_group.begin(), ::tolower);
    // If only m_group code was entered should generate help for specific group.
    if (argc < 3)
    {
        if (m_group == "-h" || m_group == "--help")
        {
            m_isGeneralHelpRequired = true;
            return;
        }
        if (m_group == "-v" || m_group == "--version")
        {
            m_isVersionRequired = true;
            return;
        }
        return;
    }

    // Otherwise set the known args and default args and parse the rest.
    m_opCode = args[2];
    transform(m_opCode.begin(), m_opCode.end(), m_opCode.begin(), ::tolower);
    if (m_opCode == "-h" || m_opCode == "--help")
    {
        m_isGroupHelpRequired = true;
        return;
    }

    for (int i = 3; i < argc; i++) {
        if (!AddParam(args[i]))
        {
            m_isParamHelpRequired = true;
            return;
        }
    }
    // If we got here that means all arguments are valid.
    // we can set m_argsAreValid to true. Otherwise it is initialized to false
    m_argsAreValid = true;
}

bool CommandLineArgs::FindParam(const string & paramName, string & outVal) const
{
    map<string, string>::const_iterator it = m_paramMap.find(paramName);
    if (it == m_paramMap.end())
    {
        outVal = string();
        return false;
    }
    outVal = it->second;
    return true;
}

void CommandLineArgs::GetParamsMapKeys(std::vector<std::string> &outVec) const
{
    outVec.clear();
    outVec.reserve(m_paramMap.size());
    for (const auto & p : m_paramMap)
    {
        outVec.push_back(p.first);
    }
}

bool CommandLineArgs::AddParam(const char* param)
{
    string stringParam(param);
    size_t pos = stringParam.find("=");
    string key = (stringParam.substr(0, pos));
    string val;
    transform(key.begin(), key.end(), key.begin(), ::tolower);
    bool isValValid = false;
    // here we check if '=' appears in the string and only then the param is valid.
    // validity is checked only for opcode params and not for cli params like -j, -t, -p..
    if (pos != string::npos)
    {
        isValValid = true;
        val = stringParam.substr(pos + 1, stringParam.length());
    }
    if (m_paramMap.find(key) != m_paramMap.end())
    {
        LOG_ERROR << "the key: " << key << " appeared twice. Please check the provided parameters." << endl;
        return false;
    }
    if ( key == "-t" || key == "--target")
    {
        m_target = val;
        return true;
    }
    if (key == "-p" ||key == "--port")
    {
        m_port = val;
        return true;
    }
    if (key == "-d")
    {
        int i = atoi(val.c_str());
        if (0 == i && val!= "0")
        {
            return false;
        }
        g_LogConfig.SetMaxSeverity(i);
        return true;
    }
    if (key == "-h" || key == "--help")
    {
        m_isParamHelpRequired = true;
        return false; // if user asked for help command is not executed.
    }
    if (key=="-j"|| key== "--json")
    {
        m_jsonResponse = true;
        return true;
    }
    if (!isValValid)
    {
        LOG_ERROR <<  "The value for key: " << key << " Is missing or invalid." << endl;
        return false;
    }
    else
    {
        m_paramMap[key] = val;
    }
    return true;
}



