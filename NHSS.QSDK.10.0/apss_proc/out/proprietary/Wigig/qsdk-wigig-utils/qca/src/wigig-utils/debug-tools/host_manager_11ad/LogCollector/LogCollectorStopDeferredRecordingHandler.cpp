/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorStopDeferredRecordingHandler.h"
#include "LogCollector.h"
#include "Host.h"
#include "DeviceManager.h"

void LogCollectorStopDeferredRecordingHandler::HandleRequest(const LogCollectorStopDeferredRecordingRequest& jsonRequest, LogCollectorStopDeferredRecordingResponse& jsonResponse)
{
    (void)jsonRequest;
    (void)jsonResponse;
    Host::GetHost().GetDeviceManager().SetLogRecordingDeferredMode(false, RECORDING_TYPE_RAW);
}

