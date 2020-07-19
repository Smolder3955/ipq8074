/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <sstream>
#include "OpCodeGeneratorBase.h"
#include "Utils.h"

using namespace std;

string OpCodeGeneratorBase::FormatedOpcodeDescription(const string& rawOpCodeDescription)
{
    std::stringstream ss(rawOpCodeDescription);
    std::string to;
    std::stringstream sf;
    bool space = false;
    sf << left; // left make text aligned to the left
    while (std::getline(ss, to, '\n')) {
        if (space)
        {
            sf << setw(20) << " ";
        }
        else
        {
            space = true;
        }
        sf << to << endl;
    }
    return sf.str();
}


bool OpCodeGeneratorBase::AddParmsToJson(Json::Value &root, const CommandLineArgs & cla)
{
    for (auto const& x : m_parameterObjects)
    {
        if (!x->SetJsonValue(root, cla))
        {
            LOG_ERROR << "Could not construct command. "
                << "Group: " << cla.Group() << " OpCode: " << cla.OpCode()
                << ", Parameter " << x->GetName() << " is missing or invalid.\n";
            return false;
        }
    }
    return true;
}

bool OpCodeGeneratorBase::ValidateProvidedParams(const CommandLineArgs& cla)
{
    vector<string> keys;
    cla.GetParamsMapKeys(keys);
    for (auto const& claParam : keys)
    {
        bool paramValid = false;
        for (auto const& x : m_parameterObjects)
        {
            if (claParam == Utils::ToLower(x->GetName()))
            {
                paramValid = true;
            }
        }
        if(!paramValid)
        {
            stringstream ssParams;
            bool coma = false;
            for (auto const& x : m_parameterObjects)
            {
                if (coma)
                {
                    ssParams << ", ";
                }
                else
                {
                    coma = true;
                }
                ssParams << x->GetName();
            }

            LOG_ERROR << "Wrong parameter provided: " << claParam << ". Expected parameters: "<< ssParams.str() << std::endl;
            return false;
        }
    }
    return true;

}

string OpCodeGeneratorBase::GenerateHelpMessage()
{
    stringstream ss;
    ss << left; // left make text aligned to the left
    ss << setw(20) << "OpCode Description:" << FormatedOpcodeDescription(m_opCodeDescription) << endl;
    if (m_parameterObjects.empty())
    {
        ss << "There are no additional parameters for this OpCode. \n";
    }
    else
    {
        ss << "Input parameters are: \n";
        for (auto const& x : m_parameterObjects)
        {
            ss << x->HelpMessage() << endl;
        }
    }

    return ss.str();
}

void OpCodeGeneratorBase::RegisterParam(unique_ptr<ParameterObject>&& upParamObject)
{
    m_parameterObjects.push_back(move(upParamObject));
}

void OpCodeGeneratorBase::SetOpcodeCommonParams()
{
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Device", false, "Select specific device.", "Any valid device name")));
}
