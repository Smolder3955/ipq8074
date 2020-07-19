/*
* Copyright (c) 2018 Qualcomm Technologies, Inc.
*
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#pragma once

#include "JsonHandlerSDK.h"

class PmcJsonResponse : public JsonResponse
{
public:
    PmcJsonResponse(const char* szTypeName, const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue):
        JsonResponse(szTypeName, jsonRequestValue, jsonResponseValue)
    {
    }

    void SetCurrentState(const char* currentState)
    {
        m_jsonResponseValue["CurrentState"] = currentState;
    }

};

