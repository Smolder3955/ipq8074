/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_HANDLER_RESPONSE_H_
#define _LOG_COLLECTOR_HANDLER_RESPONSE_H_

#include <string>

#include "json/json.h"

/*********************************** JsonResponseBase *****************************************************/
class JsonResponseBase
{
public:
    JsonResponseBase() : m_opCode("global_failure_message"), m_success(true) {};

    // Serialize the json value into a string
    std::string Serialize();

    Json::Value& GetRoot() { return m_root; }

    // In case of failure, calling this function with fail message will result in status "Failed" and a fail message in the response
    void Failed(const char* failMessage);

    virtual ~JsonResponseBase() {}

protected:
    virtual void SerializeInternal();

    virtual const char* GetTypeName() const { return "GlobalFailureResponse"; }
    virtual const char* GetGroupName() const { return "global_failure"; }

    Json::Value m_root;
    std::string m_opCode;
    bool m_success; // true for success and false for failure
};

/*********************************** LogCollectorResponse *****************************************************/
class LogCollectorResponse final : public JsonResponseBase
{
public:
    LogCollectorResponse(const char* opCode);

private:
    void SerializeInternal() override;

    const char* GetTypeName() const override { return "LogCollectorResponse"; }
    const char* GetGroupName() const override { return "log_collector"; }
};


#endif // _LOG_COLLECTOR_HANDLER_RESPONSE_H_
