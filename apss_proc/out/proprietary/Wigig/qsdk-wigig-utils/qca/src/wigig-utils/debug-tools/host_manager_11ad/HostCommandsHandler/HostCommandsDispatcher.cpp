/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "HostCommandsDispatcher.h"
#include "HostGetInterfacesHandler.h"
#include "HostGetVersionHandler.h"

HostCommandsDispatcher::HostCommandsDispatcher()
{
    RegisterOpCodeHandler("get_interfaces", std::unique_ptr<IJsonHandler>(new HostGetInterfacesHandler()));
    RegisterOpCodeHandler("get_version", std::unique_ptr<IJsonHandler>(new HostGetVersionHandler()));
}
