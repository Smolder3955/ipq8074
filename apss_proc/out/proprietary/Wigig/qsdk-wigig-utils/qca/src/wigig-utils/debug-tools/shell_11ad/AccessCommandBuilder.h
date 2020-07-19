/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _ACCESS_COMMAND_BUILDER_
#define _ACCESS_COMMAND_BUILDER_

#include "GroupCommandBuilderBase.h"
#include "OpCodeGeneratorBase.h"

class AccessCommandBuilder : public GroupCommandBuilderBase
{
public:
    AccessCommandBuilder();

private:
    void SetOpcodeGenerators() override;
};


class ACReadGenerator : public OpCodeGeneratorBase
{
public:
    ACReadGenerator();
};

class ACWriteGenerator : public OpCodeGeneratorBase
{
public:
    ACWriteGenerator();
};

class ACReadBlockGenerator : public OpCodeGeneratorBase
{
public:
    ACReadBlockGenerator();
};


#endif // _ACCESS_COMMAND_BUILDER_
