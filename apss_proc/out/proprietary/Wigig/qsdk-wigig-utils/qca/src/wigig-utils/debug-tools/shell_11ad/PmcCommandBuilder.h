/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#pragma once

#include "GroupCommandBuilderBase.h"
#include "OpCodeGeneratorBase.h"



class PmcCommandBuilder : public GroupCommandBuilderBase
{
public:
    PmcCommandBuilder();

private:
    void SetOpcodeGenerators() override;
};


// pmc get_version
class PmcGetVersionGenerator: public OpCodeGeneratorBase
{
public:
    PmcGetVersionGenerator();
};

// pmc init
class PmcInitGenerator : public OpCodeGeneratorBase
{
public:
    PmcInitGenerator();
};


// pmc get_config
class PmcGetConfigGenerator : public OpCodeGeneratorBase
{
public:
    PmcGetConfigGenerator();
};

// pmc set_config
class PmcSetConfigGenerator : public OpCodeGeneratorBase
{
public:
    PmcSetConfigGenerator();
};


// pmc get_state
class PmcGetStateGenerator : public OpCodeGeneratorBase
{
public:
    PmcGetStateGenerator();
};

// pmc start
class PmcStartGenerator : public OpCodeGeneratorBase
{
public:
    PmcStartGenerator();
};

// pmc pause
class PmcPauseGenerator : public OpCodeGeneratorBase
{
public:
    PmcPauseGenerator();
};

// pmc stop
class PmcStopGenerator : public OpCodeGeneratorBase
{
public:
    PmcStopGenerator();
};

// pmc shutdown
class PmcShutdownGenerator : public OpCodeGeneratorBase
{
public:
    PmcShutdownGenerator();
};