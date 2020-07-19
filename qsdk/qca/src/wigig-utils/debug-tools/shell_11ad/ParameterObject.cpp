/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <sstream>
#include <locale>
#include <string>
#include <algorithm>
#include "ParameterObject.h"
#include "Utils.h"
using namespace std;




ParameterObject::ParameterObject(const std::string & name, bool isMandatory, const std::string & description, const std::string & supportedValues) :
    m_name(name), m_mandatory(isMandatory), m_description(description), m_supportedValues(supportedValues) {};

// Prints specific help message for every parameter object.
string ParameterObject::HelpMessage() const
{
    stringstream ss;
    ss << std::left;
    ss << std::setw(20) << "\tParameter Name: "  << m_name << endl;
    ss << std::setw(20) << "\tDescription: "  << m_description << endl;
    ss << std::setw(20) << "\tSupported values: " << m_supportedValues << endl;
    ss << std::setw(20) << "\tIs Mandatory: " << (m_mandatory ? "true" : "false") << endl;

    return ss.str();
}

SetJsonValueResult ParameterObject::SetJsonValueBase(Json::Value & root, const CommandLineArgs & cla, string &outVal) const
{
    if (root.isMember(m_name))
    {
        LOG_ERROR << "ERROR: the key: " << m_name << " already exist, please check the command line" << endl;
        return FAIL;
    }

    // Check if key was  in the cli arguments then if the parameter is mandatory return false. otherwise return true;
    bool tmpFindRes = cla.FindParam(Utils::ToLower(m_name), outVal);
    if (!tmpFindRes)
    {
        if(m_mandatory)
        {
            LOG_ERROR << "ERROR: the key: " << m_name << " is mandatory but not provided." << endl;
            return FAIL;
        }
        else
        {
            return SUCCESS_NOT_SET;
        }
    }
    return SUCCESS_SET;
}


StringParameterObject::StringParameterObject(const std::string & name, bool isMandatory, const std::string & description, const std::string & supportedValues, bool canBeEmpty):
    ParameterObject(name, isMandatory, description, supportedValues)
{
    m_canBeEmpty = canBeEmpty;
};

bool StringParameterObject::SetJsonValue(Json::Value & root, const CommandLineArgs & cla) const
{
    string outVal;
    SetJsonValueResult res = SetJsonValueBase(root, cla, outVal);
    if (res == SUCCESS_SET)
    {
        if (outVal.empty() && !m_canBeEmpty)
        {
            return false;
        }
        root[m_name] = Json::Value(outVal);
    }
    return res != FAIL;
}

BoolParameterObject::BoolParameterObject(const std::string & name, bool isMandatory, const std::string & description):
    ParameterObject(name, isMandatory, description, "true, false") {};

bool BoolParameterObject::SetJsonValue(Json::Value & root, const CommandLineArgs & cla) const
{
    string outVal;
    SetJsonValueResult res = SetJsonValueBase(root, cla, outVal);
    transform(outVal.begin(), outVal.end(), outVal.begin(), ::tolower);
    if (res == SUCCESS_SET)
    {
        bool tmpVal = (outVal == "true");
        if(!tmpVal && outVal != "false")
        {
            LOG_ERROR << "For key: " << m_name << ". The provided value:" << outVal << " cannot be parsed to a boolean value." << endl;
            return false;
        }
        root[m_name] = Json::Value(tmpVal);
    }
    return res != FAIL;
}



IntParameterObject::IntParameterObject(const std::string & name, bool isMandatory, const std::string & description):
    ParameterObject(name, isMandatory, description, "Any number, Decimal (no prefix) or Hex (prefix 0x / 0X )") {};

bool IntParameterObject::SetJsonValue(Json::Value & root, const CommandLineArgs & cla) const
{
    string outVal;
    SetJsonValueResult res = SetJsonValueBase(root, cla, outVal);
    if (res == SUCCESS_SET)
    {
        // can take decimal (no prefix), octal (prefix 0), Hexadecimal (prefix 0x / 0X)
        long int tmpVal = strtol(outVal.c_str(), nullptr, 0);
        if (tmpVal == 0 && outVal != "0")
        {
            LOG_ERROR << "For key: " << m_name << ". The provided value:" << outVal << " can not be parsed to a numeric value." << endl;
            return false;
        }
        root[m_name] = Json::Value(static_cast<int64_t>(tmpVal));
    }
    return res != FAIL;
}
