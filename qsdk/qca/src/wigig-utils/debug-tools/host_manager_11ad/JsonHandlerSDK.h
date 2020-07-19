/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _JSON_HANDLER_SDK_H_
#define _JSON_HANDLER_SDK_H_

#include <string>
#include <ostream>
#include <sstream>
#include <map>
#include <memory>
#include <type_traits>

#include <json/json.h>
#include "SharedVersionInfo.h"
#include "Utils.h"
#include "DebugLogger.h"
#include "HostInfo.h"

// *************************************************************************************************
// JSON Handler SDK provides a means to create command groups and corresponding handlers
// SDK Contains:
// * IJsonHandler - an interface to handle json messages
// * JsonResponseBase - a base class for all responses that defines common response structure
// * JsonHandlerBase - a base class for opcode handlers
//
// To create a command group 'Example':
// 1. Define class JsonExampleOpcodeRequest for every opcode in the group
// 2. Define class JsonExampleOpcodeResponse: public JsonResponseBase for every opcode in the group
// 3. Define class JsonExampleOpcodeHandler: public JsonHandlerBase<JsonExampleOpcodeRequest, JsonExampleOpcodeResponse>
// 4. Define class JsonExampleGroupHandler to dispatch Example requests
// *************************************************************************************************

class IJsonHandler
{
public:
    virtual void HandleJsonCommand(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) = 0;
    virtual ~IJsonHandler() {}
};

// *************************************************************************************************


// *************************************************************************************************
// All specific response classes should derive from this class to construct protocol-compliant
// responses.
class JsonResponse
{
    friend std::ostream& operator<<(std::ostream& os, const JsonResponse& jsonResponse);

public:
    JsonResponse(const char* szTypeName, const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue)
        : m_jsonResponseValue(jsonResponseValue)
    {
        m_jsonResponseValue["$type"] = szTypeName;
        m_jsonResponseValue["Group"] = jsonRequestValue["Group"];
        m_jsonResponseValue["OpCode"] = jsonRequestValue["OpCode"];
        m_jsonResponseValue["Success"] = true;
        m_jsonResponseValue["BeginTimestamp"] = Utils::GetCurrentLocalTimeString();
        m_jsonResponseValue["HostVersion"] = GetToolsVersion();
    }

    void Fail(const std::string& errorMessage)
    {
        Fail(errorMessage.c_str());
    }

    void Fail(const char* szErrorMessage)
    {
        m_jsonResponseValue["Success"] = false;
        m_jsonResponseValue["ErrorMessage"] = szErrorMessage;
    }

    void Complete()
    {
        m_jsonResponseValue["EndTimestamp"] = Utils::GetCurrentLocalTimeString();
    }

protected:
    Json::Value& m_jsonResponseValue;
};

inline std::ostream& operator<<(std::ostream& os, const JsonResponse& jsonResponse)
{
    return os << jsonResponse.m_jsonResponseValue;
}

// *************************************************************************************************
// Basic response class with Success status
class JsonBasicResponse final : public JsonResponse
{
public:
    JsonBasicResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
        JsonResponse("JsonBasicResponse", jsonRequestValue, jsonResponseValue)
    {
    }
};

// *************************************************************************************************
// JsonErrorResponse should be generated when no real handling can be performed (malformed request,
// unexpected group/opcode etc.)
class JsonErrorResponse
{
public:
    JsonErrorResponse(const char* szErrorMessage, Json::Value& jsonResponseValue)
        : m_jsonResponseValue(jsonResponseValue)
    {
        m_jsonResponseValue["$type"] = "JsonErrorResponse";
        m_jsonResponseValue["Success"] = false;
        m_jsonResponseValue["BeginTimestamp"] = Utils::GetCurrentLocalTimeString();
        m_jsonResponseValue["ErrorMessage"] = szErrorMessage;
        m_jsonResponseValue["HostVersion"] = GetToolsVersion();
    }

private:
    Json::Value& m_jsonResponseValue;
};

// *************************************************************************************************

// Base class for all handlers that defines a handling flow and request/response creation/lifetime.
template<typename REQUEST, typename RESPONSE>
class JsonOpCodeHandlerBase: public IJsonHandler
{
    static_assert(std::is_base_of<JsonResponse, RESPONSE>::value, "Json response class should derive from JsonResponse");

public:
    void HandleJsonCommand(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) override
    {
        REQUEST jsonRequest(jsonRequestValue);
        RESPONSE jsonResponse(jsonRequestValue, jsonResponseValue);

        // Improper access to JSON value may throw aqn exception. At this level the exception may
        // be reported in a specific JSON response.
        try
        {
            HandleRequest(jsonRequest, jsonResponse);
        }
        catch (const Json::LogicError &e)
        {
            std::stringstream ss;
            ss << "Json LogicError during request handling : " << e.what();
            LOG_ERROR << "JSON handling error: " << ss.str() << std::endl;
            jsonResponse.Fail(ss.str());
        }
        catch (const std::exception& e)
        {
            std::stringstream ss;
            ss << "Json LogicError during request handling : " << e.what();
            LOG_ERROR << "JSON handling error: " << ss.str() << std::endl;
            jsonResponse.Fail(ss.str());
        }

        jsonResponse.Complete();
    }

protected:
    JsonOpCodeHandlerBase() {}

private:
    virtual void HandleRequest(const REQUEST& jsonRequest, RESPONSE& jsonResponse) = 0;
};

// *************************************************************************************************

enum class JsonValueState
{
    JSON_VALUE_PROVIDED,    // The value is provided according to an expected type
    JSON_VALUE_MISSING,     // The expected key is missing from a request
    JSON_VALUE_MALFORMED,   // The value is inconsistent with an expected type
};

inline std::ostream& operator<<(std::ostream& os, const JsonValueState& jsonValueState)
{
    switch (jsonValueState)
    {
    case JsonValueState::JSON_VALUE_PROVIDED:  return os << "JSON_VALUE_PROVIDED";
    case JsonValueState::JSON_VALUE_MISSING:   return os << "JSON_VALUE_MISSING";
    case JsonValueState::JSON_VALUE_MALFORMED: return os << "JSON_VALUE_MALFORMED";
    default:                                   return os << "<unknown>";
    }
}

// *************************************************************************************************

template<typename T> class JsonValueBoxed
{
public:
    JsonValueBoxed(const T& value): m_State(JsonValueState::JSON_VALUE_PROVIDED), m_Value(value) {}
    JsonValueBoxed(JsonValueState state) : m_State(state), m_Value(static_cast<T>(0)) {}

    JsonValueState GetState() const { return m_State; }
    const T& GetValue() const { return m_Value; }

    // Should only be used when GetState() == JsonValueState::JSON_VALUE_PROVIDED
    operator T() const { return m_Value; }

private:
    const JsonValueState m_State;
    const T m_Value;
};

// Partial specialization is needed here because of the initialization of the value in case 'not provided' state
template<> class JsonValueBoxed <std::string>
{
public:
    // C'tor gets const char* (and not a string) to avoid a redundant copy
    JsonValueBoxed(const char* value) : m_State(JsonValueState::JSON_VALUE_PROVIDED), m_Value(value) {}
    JsonValueBoxed(JsonValueState state) : m_State(state), m_Value() {}

    JsonValueState GetState() const { return m_State; }
    const std::string& GetValue() const { return m_Value; }

    // Should only be used when GetState() == JsonValueState::JSON_VALUE_PROVIDED
    operator std::string() const { return m_Value; }

private:
    const JsonValueState m_State;
    const std::string m_Value;
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const JsonValueBoxed<T>& jsonValue)
{
    os << jsonValue.GetState();
    if (jsonValue.GetState() == JsonValueState::JSON_VALUE_PROVIDED)
    {
        os << '(' << jsonValue.GetValue() << ')';
    }

    return os;
}

// *************************************************************************************************

class JsonValueParser
{
public:
    static JsonValueBoxed<bool> ParseBooleanValue(const Json::Value& jsonValue, const char* szKey)
    {
        const Json::Value& value = jsonValue[szKey];
        if (Json::Value::nullSingleton() == value)
        {
            return JsonValueBoxed<bool>(JsonValueState::JSON_VALUE_MISSING);
        }

        if (!value.isBool())
        {
            return JsonValueBoxed<bool>(JsonValueState::JSON_VALUE_MALFORMED);
        }

        return JsonValueBoxed<bool>(value.asBool());
    }

    static JsonValueBoxed<uint32_t> ParseUnsignedValue(const Json::Value& jsonValue, const char* szKey)
    {
        const Json::Value& value = jsonValue[szKey];
        if (Json::Value::nullSingleton() == value)
        {
            return JsonValueBoxed<uint32_t>(JsonValueState::JSON_VALUE_MISSING);
        }

        if (!value.isUInt())
        {
            return JsonValueBoxed<uint32_t>(JsonValueState::JSON_VALUE_MALFORMED);
        }

        return JsonValueBoxed<uint32_t>(value.asUInt());
    }

    static JsonValueBoxed<std::string> ParseStringValue(const Json::Value& jsonValue, const char* szKey)
    {
        const Json::Value& value = jsonValue[szKey];
        if (Json::Value::nullSingleton() == value)
        {
            return JsonValueBoxed<std::string>(JsonValueState::JSON_VALUE_MISSING);
        }

        // if the key exists, its value can always be a string (so there can't be a malformed value)
        return JsonValueBoxed<std::string>(value.asCString());
    }
};

// *************************************************************************************************

class JsonValueExtractor
{
public:
    template <typename T>
    static JsonValueBoxed<T> Parse(const Json::Value& jsonValue, const char* szKey)
    {
        return JsonValueBoxed<T>(JsonValueState::JSON_VALUE_MISSING);
    }

    JsonValueExtractor() = delete;
};

template<>
inline JsonValueBoxed<std::string> JsonValueExtractor::Parse<std::string>(const Json::Value& jsonValue, const char* szKey)
{
    const Json::Value& value = jsonValue[szKey];
    if (Json::Value::nullSingleton() == value)
    {
        return { JsonValueState::JSON_VALUE_MISSING };
    }

    // if the key exists, its value can always be a string (so there can't be a malformed value)
    return { value.asCString() };
}


template<>
inline JsonValueBoxed<uint32_t> JsonValueExtractor::Parse<uint32_t>(const Json::Value& jsonValue, const char* szKey)
{
    const Json::Value& value = jsonValue[szKey];
    if (Json::Value::nullSingleton() == value)
    {
        return { JsonValueState::JSON_VALUE_MISSING };
    }

    if (!value.isUInt())
    {
        return { JsonValueState::JSON_VALUE_MALFORMED };
    }

    return { value.asUInt() };
}

template<>
inline JsonValueBoxed<bool> JsonValueExtractor::Parse<bool>(const Json::Value& jsonValue, const char* szKey)
{
    const Json::Value& value = jsonValue[szKey];
    if (Json::Value::nullSingleton() == value)
    {
        return{ JsonValueState::JSON_VALUE_MISSING };
    }

    if (!value.isBool())
    {
        return { JsonValueState::JSON_VALUE_MALFORMED };
    }

    return { value.asBool() };
}

// *************************************************************************************************
// A specific request class  that handles device commands should derive from this base class to
// inherit the target device name.
class JsonDeviceRequest
{
public:
    // If device name not provided returns empty string
    const std::string& GetDeviceName() const { return m_DeviceName; }
protected:
    JsonDeviceRequest(const Json::Value& jsonRequestValue)
        : m_jsonRequestValue(jsonRequestValue)
        // If device name not provided set m_DeviceName to empty string
        , m_DeviceName(m_jsonRequestValue.get("Device", Json::Value("")).asString())
    {
    }
    const Json::Value& m_jsonRequestValue;

private:
    const std::string m_DeviceName;
};


// *************************************************************************************************



class JsonOpCodeDispatchErrorResponse : public JsonResponse
{
public:
    JsonOpCodeDispatchErrorResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue, std::string errorInfo="")
        : JsonResponse("JsonDispatchErrorResponse", jsonRequestValue, jsonResponseValue)
    {
        if (errorInfo.empty())
        {
            Fail("No handler is registered for opcode");
            return;
        }
        Fail(errorInfo);

    }
};

// *************************************************************************************************

class JsonOpCodeDispatcher : public IJsonHandler
{
public:
    void HandleJsonCommand(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) override;

protected:
    JsonOpCodeDispatcher();
    void RegisterOpCodeHandler(const char* szOpCode, std::unique_ptr<IJsonHandler> spJsonHandler);

private:
    std::map<std::string, std::unique_ptr<IJsonHandler>> m_opcodeHandlerMap;
};

#endif      // _JSON_HANDLER_SDK_H_
