/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_CLOSE_HANDLER_H_
#define _SENSING_CLOSE_HANDLER_H_

#include "JsonHandlerSDK.h"

class SensingCloseHandler : public JsonOpCodeHandlerBase<Json::Value, JsonBasicResponse>
{
private:
    void HandleRequest(const Json::Value& jsonRequest, JsonBasicResponse& jsonResponse) override;
};

#endif  // _SENSING_CLOSE_HANDLER_H_
