/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "SensingCommandDispatcher.h"
#include "SensingOpenHandler.h"
#include "SensingGetParametersHandler.h"
#include "SensingChangeModeHandler.h"
#include "SensingGetModeHandler.h"
#include "SensingReadDataHandler.h"
#include "SensingGetNumRemainingBurstsHandler.h"
#include "SensingCloseHandler.h"

SensingCommandDispatcher::SensingCommandDispatcher()
{
    RegisterOpCodeHandler("open", std::unique_ptr<IJsonHandler>(new SensingOpenHandler()));
    RegisterOpCodeHandler("get_parameters", std::unique_ptr<IJsonHandler>(new SensingGetParametersHandler()));
    RegisterOpCodeHandler("change_mode", std::unique_ptr<IJsonHandler>(new SensingChangeModeHandler()));
    RegisterOpCodeHandler("get_mode", std::unique_ptr<IJsonHandler>(new SensingGetModeHandler()));
    RegisterOpCodeHandler("read_data", std::unique_ptr<IJsonHandler>(new SensingReadDataHandler()));
    RegisterOpCodeHandler("get_num_remaining_bursts", std::unique_ptr<IJsonHandler>(new SensingGetNumRemainingBurstsHandler()));
    RegisterOpCodeHandler("close", std::unique_ptr<IJsonHandler>(new SensingCloseHandler()));
}
