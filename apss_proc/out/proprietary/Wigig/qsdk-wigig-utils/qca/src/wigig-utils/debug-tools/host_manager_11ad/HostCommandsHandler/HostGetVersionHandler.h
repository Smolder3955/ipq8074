/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _HOST_GET_VERSION_HANDLER_H_
#define _HOST_GET_VERSION_HANDLER_H_

#include "JsonHandlerSDK.h"
#include "DebugLogger.h"

class HostGetVersionResponse: public JsonResponse
{
    public:
        HostGetVersionResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
            JsonResponse("HostGetVersionResponse", jsonRequestValue, jsonResponseValue)
        {
        }

        void SetVersion(const std::string& version);

};

class HostGetVersionHandler: public JsonOpCodeHandlerBase<Json::Value, HostGetVersionResponse>
{
    private:
        void HandleRequest(const Json::Value& jsonRequest, HostGetVersionResponse& jsonResponse) override;
};

#endif  // _HOST_GET_VERSION_HANDLER_H_
