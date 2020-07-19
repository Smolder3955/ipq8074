/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _PARAMETER_OBJECT_
#define _PARAMETER_OBJECT_

#include <string>
#include <typeinfo>
#include "json/json.h"
#include "CommandLineArgs.h"
#include <memory>


enum SetJsonValueResult {SUCCESS_SET, SUCCESS_NOT_SET, FAIL};

/*
This class defines the code base for the specific type objects:
    string
    int
    bool
*/
class ParameterObject
{
public:
    virtual ~ParameterObject() = default;

    std::string HelpMessage() const;
    virtual bool SetJsonValue(Json::Value &root, const CommandLineArgs& cla) const = 0;
    const std::string& GetName() const { return m_name; };
    const std::string& GetDescription()const { return m_description; };
    const std::string& GetSupportedValues()const { return m_supportedValues; };
    void SetSupportedValues(const std::string& supportedValues) { m_supportedValues = supportedValues; };
    bool IsMandatory() const { return m_mandatory; };

    ParameterObject(const ParameterObject & po) = default;
    ParameterObject(ParameterObject && po) = default;
    ParameterObject& operator=(const ParameterObject&) = default;

protected:
    ParameterObject(const std::string & name, bool isMandatory, const std::string & description, const std::string & supportedValues = "");
    std::string m_name;
    bool m_mandatory;
    std::string m_description; // description of the param might be empty
    std::string m_supportedValues;
    SetJsonValueResult SetJsonValueBase(Json::Value &root, const CommandLineArgs& cla, std::string &outVal) const;

};

inline std::ostream &operator<<(std::ostream &output, const std::unique_ptr<ParameterObject> &po)
{
    output << po->GetName();
    return output;
}

class IntParameterObject :
    public ParameterObject
{
public:
    IntParameterObject(const std::string & name, bool isMandatory, const std::string & description);
    bool SetJsonValue(Json::Value &root, const CommandLineArgs& cla) const;
};

class StringParameterObject :
    public ParameterObject
{
public:
    StringParameterObject(const std::string & name, bool isMandatory, const std::string & description,
                                                    const std::string & supportedValues = "Any String", bool canBeEmpty = false);
    bool SetJsonValue(Json::Value &root, const CommandLineArgs& cla) const;
private:
    bool m_canBeEmpty;
};


class BoolParameterObject :
    public ParameterObject
{
public:
    BoolParameterObject(const std::string & name, bool isMandatory, const std::string & description);
    bool SetJsonValue(Json::Value &root, const CommandLineArgs& cla) const;
};


#endif

