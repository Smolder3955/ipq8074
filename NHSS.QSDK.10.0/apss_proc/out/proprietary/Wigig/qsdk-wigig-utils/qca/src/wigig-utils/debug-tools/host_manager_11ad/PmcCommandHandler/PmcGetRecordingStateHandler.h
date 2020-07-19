/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _PMC_GET_RECORDING_STATE_HANDLER_H_
#define _PMC_GET_RECORDING_STATE_HANDLER_H_

#include "JsonHandlerSDK.h"
#include "DebugLogger.h"

class PmcGetRecordingStateRequest: public JsonDeviceRequest
{
    public:
        PmcGetRecordingStateRequest(const Json::Value& jsonRequestValue) : JsonDeviceRequest(jsonRequestValue)
        {
        }
};

class PmcGetRecordingStateResponse: public JsonResponse
{
    public:
        PmcGetRecordingStateResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
                JsonResponse("PmcGetRecordingStateResponse", jsonRequestValue, jsonResponseValue)
        {
            SetPmcGeneral0(0);
            SetPmcGeneral1(0);
            SetIsActive(false);
        }

        void SetPmcGeneral0(uint32_t value)
        {
            m_jsonResponseValue["PMC_GENERAL_0"] = value;
        }

        void SetPmcGeneral1(uint32_t value)
        {
            m_jsonResponseValue["PMC_GENERAL_1"] = value;
        }

        void SetIsActive(bool active)
        {
            m_jsonResponseValue["Active"] = active;
        }
};

class PmcGetRecordingStateHandler: public JsonOpCodeHandlerBase<PmcGetRecordingStateRequest, PmcGetRecordingStateResponse>
{
    private:
        void HandleRequest(const PmcGetRecordingStateRequest& jsonRequest, PmcGetRecordingStateResponse& jsonResponse) override;
};

#endif// _PMC_GET_RECORDING_STATE_HANDLER_H_
