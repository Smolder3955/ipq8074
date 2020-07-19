/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _PMC_GET_CONFIG_HANDLER_H_
#define _PMC_GET_CONFIG_HANDLER_H_

#include "JsonHandlerSDK.h"
#include "PmcJsonResponse.h"
#include "DebugLogger.h"

class PmcGetConfigRequest: public JsonDeviceRequest
{
    public:
        PmcGetConfigRequest(const Json::Value& jsonRequestValue) :
                JsonDeviceRequest(jsonRequestValue)
        {
        }
};

class PmcGetConfigResponse: public PmcJsonResponse
{
    public:
        PmcGetConfigResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue):
            PmcJsonResponse("PmcGetConfigResponse", jsonRequestValue, jsonResponseValue)
        {
            // Make sure default values are always set
            SetCollectIdleSmEvents(false);
            SetCollectRxPpduEvents(false);
            SetCollectTxPpduEvents(false);
            SetCollectUCodeEvents(false);
        }

        void SetCollectIdleSmEvents(bool value)
        {
            m_jsonResponseValue["CollectIdleSmEvents"] = value;
        }
        void SetCollectRxPpduEvents(bool value)
        {
            m_jsonResponseValue["CollectRxPpduEvents"] = value;
        }
        void SetCollectTxPpduEvents(bool value)
        {
            m_jsonResponseValue["CollectTxPpduEvents"] = value;
        }
        void SetCollectUCodeEvents(bool value)
        {
            m_jsonResponseValue["CollectUCodeEvents"] = value;
        }
};

class PmcGetConfigHandler: public JsonOpCodeHandlerBase<PmcGetConfigRequest, PmcGetConfigResponse>
{
    private:
        void HandleRequest(const PmcGetConfigRequest& jsonRequest, PmcGetConfigResponse& jsonResponse) override;
};

#endif  // _PMC_GET_CONFIG_HANDLER_H_
