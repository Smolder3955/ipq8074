/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorDeferredRecordingHandler.h"
#include "LogCollector.h"
#include "Host.h"
#include "DeviceManager.h"

void LogCollectorDeferredRecordingHandler::HandleRequest(const LogCollectorStartDeferredRecordingRequest& jsonRequest, LogCollectorStartDeferredRecordingResponse& jsonResponse)
{
    const JsonValueBoxed<log_collector::RecordingType> RecordingTypeBoxed = jsonRequest.GetRecordingTypeBoxed();
    log_collector::RecordingType recordingType;
    switch (RecordingTypeBoxed.GetState())
    {
    case JsonValueState::JSON_VALUE_MALFORMED:
        jsonResponse.Fail("RecordingType is wrong, it should be 'raw' or 'txt' or empty (default is raw)");
        return;
    case JsonValueState::JSON_VALUE_MISSING:
        recordingType = log_collector::RECORDING_TYPE_RAW;
        break;
    default:
        // guaranty that value is provided.
        recordingType = RecordingTypeBoxed;
        break;
    }
    //TODO: if recording type is txt validate conversion file.
    LOG_DEBUG << "Log Collector start deferred recording"
        << " recording type is: " << recordingType << std::endl;

    OperationStatus os = Host::GetHost().GetDeviceManager().SetLogRecordingDeferredMode(true, recordingType);
    if (!os)
    {
        jsonResponse.Fail(os.GetStatusMessage());
    }
}

