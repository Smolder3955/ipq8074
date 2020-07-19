/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <string>
#include "OperationStatus.h"

class PmcSetConfigHandlerSequences
{
    public:
        static OperationStatus ConfigureUCodeEvents(const std::string& deviceName, bool ucodeCoreWriteEnabled);
        static OperationStatus ConfigRxMpdu(const std::string& deviceName);
        static OperationStatus ConfigTxMpdu(const std::string& deviceName);
        static OperationStatus ConfigCollectIdleSm(const std::string& deviceName, bool configurationValue);
        static OperationStatus ConfigCollectRxPpdu(const std::string& deviceName, bool configurationValue);
        static OperationStatus ConfigCollectTxPpdu(const std::string& deviceName, bool configurationValue);
        static OperationStatus ConfigCollectUcode(const std::string& deviceName,  bool configurationValue);
};


