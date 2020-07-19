/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _PMC_STOP_HANDLER_H_
#define _PMC_STOP_HANDLER_H_

#include "JsonHandlerSDK.h"
#include "PmcJsonResponse.h"
#include "DebugLogger.h"
#include "OperationStatus.h"

class PmcStopRequest: public JsonDeviceRequest
{
    public:
        PmcStopRequest(const Json::Value& jsonRequestValue) :
                JsonDeviceRequest(jsonRequestValue)
        {
        }
};

class PmcStopResponse: public PmcJsonResponse
{
    public:
        PmcStopResponse(const Json::Value& jsonRequestValue, Json::Value& jsonResponseValue) :
            PmcJsonResponse("PmcStopResponse", jsonRequestValue, jsonResponseValue)
        {
        }

        void SetRecordingDirectory(const char* szRecDirectory)
        {
            m_jsonResponseValue["RecordingDirectory"] = szRecDirectory;
        }
};

class PmcStopHandler: public JsonOpCodeHandlerBase<PmcStopRequest, PmcStopResponse>
{
    private:
        void HandleRequest(const PmcStopRequest& jsonRequest, PmcStopResponse& jsonResponse) override;
        OperationStatus GetNewPmcFile(const std::string& deviceName, std::string& pmcFilePath, uint32_t& refNum);
        std::string GetFinalPmcFileLocation(const std::string& pmcDataPath, const std::string& baseName);
};

#endif  // _PMC_STOP_HANDLER_H_
