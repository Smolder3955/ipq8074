/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _HOST_COMMAND_BUILDER_
#define _HOST_COMMAND_BUILDER_

#include "GroupCommandBuilderBase.h"
#include "OpCodeGeneratorBase.h"

class HostCommandBuilder : public GroupCommandBuilderBase
{
public:
    HostCommandBuilder();

private:
    void SetOpcodeGenerators() override;
};


// host get_interfaces
class HCGetInterfacesGenerator : public OpCodeGeneratorBase
{
public:
    HCGetInterfacesGenerator();
};

// host get_version
class HCGetVersionGenerator : public OpCodeGeneratorBase
{
public:
    HCGetVersionGenerator();
};


#endif // _LOG_COLLECTOR_BUILDER_