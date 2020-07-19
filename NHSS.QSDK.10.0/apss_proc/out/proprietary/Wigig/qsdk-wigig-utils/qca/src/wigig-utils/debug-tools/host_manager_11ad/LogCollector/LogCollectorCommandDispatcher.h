/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_COMMAND_DISPATCHER_H_
#define _LOG_COLLECTOR_COMMAND_DISPATCHER_H_
#pragma once

#include "JsonHandlerSDK.h"

// *************************************************************************************************

class LogCollectorCommandDispatcher : public JsonOpCodeDispatcher
{
public:
    LogCollectorCommandDispatcher();
};
#endif  // _LOG_COLLECTOR_COMMAND_DISPATCHER_H_

