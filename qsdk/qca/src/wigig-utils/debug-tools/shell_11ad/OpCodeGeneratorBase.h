/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _OPCODE_GEN_BASE_
#define _OPCODE_GEN_BASE_

#include <string>
#include <vector>
#include <memory>
#include "json/json.h"
#include "ParameterObject.h"
#include "CommandLineArgs.h"


class OpCodeGeneratorBase
{
public:
    static std::string FormatedOpcodeDescription(const std::string& rawOpCodeDescription);
    bool AddParmsToJson(Json::Value &root, const CommandLineArgs &cla);
    bool ValidateProvidedParams(const CommandLineArgs &cla);
    std::string GenerateHelpMessage();
    const char* OpCodeDescription() const { return m_opCodeDescription.c_str(); };

protected:
    explicit OpCodeGeneratorBase(const char* pOpCodeDescription): m_opCodeDescription(pOpCodeDescription) {};
    void RegisterParam(std::unique_ptr<ParameterObject>&& upParamObject);
    void SetOpcodeCommonParams();

private:
    const std::string m_opCodeDescription;
    std::vector<std::unique_ptr<ParameterObject> > m_parameterObjects;
};
#endif