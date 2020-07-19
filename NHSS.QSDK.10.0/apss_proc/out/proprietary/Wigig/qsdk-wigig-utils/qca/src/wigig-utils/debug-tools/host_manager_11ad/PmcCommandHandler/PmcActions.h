/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#pragma once

#include <string>
#include "HostManagerDefinitions.h"
#include "OperationStatus.h"

/*
PMC state machine:
++-------+-------+-------+-------+--------+
||INIT   |START  |PAUSE  |STOP   |SHUTDOWN| <- Action
+=======++=======+=======+=======+=======+========+
|IDLE   ||READY  |ACTIVE*|IDLE   |IDLE   |IDLE    |
+-------++-------+-------+-------+-------+--------+
|READY  ||READY  |ACTIVE |READY  |READY  |IDLE    |
+-------++-------+-------+-------+-------+--------+
|ACTIVE ||ACTIVE |ACTIVE |PAUSED |IDLE   |IDLE    |
+-------++-------+-------+-------+-------+--------+
|PAUSED ||PAUSED |PAUSED |PAUSED |IDLE   |IDLE    |
+-------++-------+-------+-------+-------+--------+
^
|
state
*/

class PmcActions
{
    //PMC transitions
public:
    static OperationStatus Shutdown(const std::string& deviceName);
    static OperationStatus Init(const std::string& deviceName);
    static OperationStatus Start(const std::string& deviceName);
    static OperationStatus Pause(const std::string& deviceName);
    static OperationStatus Stop(const std::string& deviceName);

    static OperationStatus ValidateDeviceName(const std::string& deviceName, BasebandType& deviceType);
    static const char* GetCurrentStateString(const std::string& deviceName);
};