/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _HOST_GET_INTERFACES_HANDLER_H_
#define _HOST_GET_INTERFACES_HANDLER_H_

#include "JsonHandlerSDK.h"
#include "DebugLogger.h"


class HostGetInterfacesResponse: public JsonResponse
{
    public:
        HostGetInterfacesResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
            JsonResponse("HostGetInterfacesResponse", jsonRequestValue, jsonResponseValue)
        {
            m_jsonResponseValue["Interfaces"] = Json::Value(Json::arrayValue);
        }

        void AddInterface(const std::string& interfaceName);

};

class HostGetInterfacesHandler: public JsonOpCodeHandlerBase<Json::Value, HostGetInterfacesResponse>
{
    private:
        void HandleRequest(const Json::Value& jsonRequest, HostGetInterfacesResponse& jsonResponse) override;
};

#endif  // _HOST_GET_INTERFACES_HANDLER_H_
