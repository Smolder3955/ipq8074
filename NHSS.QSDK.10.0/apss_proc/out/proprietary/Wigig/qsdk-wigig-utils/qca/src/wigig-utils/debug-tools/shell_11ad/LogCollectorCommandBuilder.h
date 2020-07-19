/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_BUILDER_
#define _LOG_COLLECTOR_BUILDER_

#include "GroupCommandBuilderBase.h"
#include "OpCodeGeneratorBase.h"

class LogCollectorCommandBuilder : public GroupCommandBuilderBase
{
public:
    LogCollectorCommandBuilder();

private:
    void SetOpcodeGenerators() override;
};


// log_collector get_version
class LCGetVersionGenerator : public OpCodeGeneratorBase
{
public:
    LCGetVersionGenerator();
};

// log_collector get_state
class LCGetStateGenerator : public OpCodeGeneratorBase
{
public:
    LCGetStateGenerator();
};

// log_collector start_recording
class LCStartRecordingGenerator : public OpCodeGeneratorBase
{
public:
    LCStartRecordingGenerator();
};

// log_collector stop_recording
class LCStopRecordingGenerator : public OpCodeGeneratorBase
{
public:
    LCStopRecordingGenerator();
};

// log_collector start_deferred_recording
class LCStartDeferredRecordingGenerator : public OpCodeGeneratorBase
{
public:
    LCStartDeferredRecordingGenerator();
};


// log_collector stop_deferred_recording
class LCStopDeferredRecordingGenerator : public OpCodeGeneratorBase
{
public:
    LCStopDeferredRecordingGenerator();
};

// log_collector get_config
class LCGetConfigGenerator : public OpCodeGeneratorBase
{
public:
    LCGetConfigGenerator();
};

// log_collector set_config
class LCSetConfigGenerator : public OpCodeGeneratorBase
{
public:
    LCSetConfigGenerator();
};

//log_collector reset_config
class LCReSetConfigGenerator : public OpCodeGeneratorBase
{
public:
    LCReSetConfigGenerator();
};

// log_collector set_verbosity
class LCSetVerbosityGenerator : public OpCodeGeneratorBase
{
public:
    LCSetVerbosityGenerator();
};

// log_collector get_verbosity
class LCGetVerbosityGenerator : public OpCodeGeneratorBase
{
public:
    LCGetVerbosityGenerator();
};


#endif // _LOG_COLLECTOR_BUILDER_