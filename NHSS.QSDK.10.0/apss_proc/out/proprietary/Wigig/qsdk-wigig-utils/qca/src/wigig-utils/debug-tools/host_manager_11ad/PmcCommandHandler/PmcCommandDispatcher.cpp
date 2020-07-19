/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcCommandDispatcher.h"
#include "PmcVersionHandler.h"
#include "PmcGetConfigHandler.h"
#include "PmcSetConfigHandler.h"
#include "PmcInitHandler.h"
#include "PmcStartHandler.h"
#include "PmcPauseHandler.h"
#include "PmcStopHandler.h"
#include "PmcShutdownHandler.h"
#include "PmcGetRecordingStateHandler.h"

PmcCommandDispatcher::PmcCommandDispatcher()
{
    RegisterOpCodeHandler("get_version", std::unique_ptr<IJsonHandler>(new PmcVersionHandler()));
    RegisterOpCodeHandler("get_config", std::unique_ptr<IJsonHandler>(new PmcGetConfigHandler()));
    RegisterOpCodeHandler("set_config", std::unique_ptr<IJsonHandler>(new PmcSetConfigHandler()));
    RegisterOpCodeHandler("init", std::unique_ptr<IJsonHandler>(new PmcInitHandler()));
    RegisterOpCodeHandler("start", std::unique_ptr<IJsonHandler>(new PmcStartHandler()));
    RegisterOpCodeHandler("pause", std::unique_ptr<IJsonHandler>(new PmcPauseHandler()));
    RegisterOpCodeHandler("stop", std::unique_ptr<IJsonHandler>(new PmcStopHandler()));
    RegisterOpCodeHandler("shutdown", std::unique_ptr<IJsonHandler>(new PmcShutdownHandler()));
    RegisterOpCodeHandler("get_state", std::unique_ptr<IJsonHandler>(new PmcGetRecordingStateHandler()));

}
