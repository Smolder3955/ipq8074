/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "AccessCommandsDispatcher.h"
#include "ReadHandler.h"
#include "WriteHandler.h"
#include "ReadBlockHandler.h"


AccessCommandsDispatcher::AccessCommandsDispatcher()
{
    RegisterOpCodeHandler("read", std::unique_ptr<IJsonHandler>(new ReadHandler()));
    RegisterOpCodeHandler("read_block", std::unique_ptr<IJsonHandler>(new ReadBlockHandler()));
    RegisterOpCodeHandler("write", std::unique_ptr<IJsonHandler>(new WriteHandler()));
}
