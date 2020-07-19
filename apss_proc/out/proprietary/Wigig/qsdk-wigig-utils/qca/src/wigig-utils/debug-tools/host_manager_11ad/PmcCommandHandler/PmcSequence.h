/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef PMC_SEQUENCE_H_
#define PMC_SEQUENCE_H_

#include <string>
#include "OperationStatus.h"
#include "HostManagerDefinitions.h"

class PmcSequence
{
    public:
        //for now we don't have an API to get a proper revision so using this until one is created
        static BasebandRevisionEnum GetDeviceRevision(const std::string& deviceName);
        static std::string GetPmcClockRegister(const std::string& deviceName);
        static OperationStatus DisablePmcClock(const std::string& deviceName);
        static OperationStatus FreePmcRing(const std::string& deviceName, bool bSuppressError);
        static OperationStatus FindPmcRingIndex(const std::string& deviceName, uint32_t& ringIndex);
        static OperationStatus ReadPmcHead(const std::string& deviceName, uint32_t& pmcHead);
        //get recording state
        static OperationStatus IsPmcRecordingActive(const std::string& deviceName, bool& isRecordingActive);

        // init handlers
        static OperationStatus Init(const std::string & deviceName);
        static OperationStatus Shutdown(const std::string& deviceName);
        static OperationStatus DisableEventCollection(const std::string& deviceName);
        static OperationStatus DeactivateRecording(const std::string& deviceName);
        static OperationStatus EnablePmcClock(const std::string& deviceName);
        static OperationStatus AllocatePmcRing(const std::string& deviceName);
        static OperationStatus DisablePrpClockGating(const std::string& deviceName);
        //pause handler
        static OperationStatus PausePmcRecording(const std::string& deviceName);
        //start recording handler
        static OperationStatus ActivateRecording(const std::string& deviceName);
        static OperationStatus SetDefaultConfiguration(const std::string & deviceName, BasebandType & deviceType);
        static OperationStatus ApplyCurrentConfiguration(const std::string & deviceName, BasebandType & deviceType);
};

#endif /* PMC_SEQUENCE_H_ */
