/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <memory>
#include "PmcCommandBuilder.h"

using namespace std;

PmcCommandBuilder::PmcCommandBuilder()
{
    SetOpcodeGenerators();
}

void PmcCommandBuilder::SetOpcodeGenerators()
{
    RegisterOpcodeGenerators("get_version", unique_ptr<OpCodeGeneratorBase>(new PmcGetVersionGenerator()));
    RegisterOpcodeGenerators("get_config", unique_ptr<OpCodeGeneratorBase>(new PmcGetConfigGenerator()));
    RegisterOpcodeGenerators("set_config", unique_ptr<OpCodeGeneratorBase>(new PmcSetConfigGenerator()));
    RegisterOpcodeGenerators("init", unique_ptr<OpCodeGeneratorBase>(new PmcInitGenerator()));
    RegisterOpcodeGenerators("start", unique_ptr<OpCodeGeneratorBase>(new PmcStartGenerator()));
    RegisterOpcodeGenerators("pause", unique_ptr<OpCodeGeneratorBase>(new PmcPauseGenerator()));
    RegisterOpcodeGenerators("stop", unique_ptr<OpCodeGeneratorBase>(new PmcStopGenerator()));
    RegisterOpcodeGenerators("shutdown", unique_ptr<OpCodeGeneratorBase>(new PmcShutdownGenerator()));
    RegisterOpcodeGenerators("get_state", unique_ptr<OpCodeGeneratorBase>(new PmcGetStateGenerator()));
}

PmcGetVersionGenerator::PmcGetVersionGenerator():
    OpCodeGeneratorBase("Print the PMC version implemented by host_manager_11ad")
{
}

PmcInitGenerator::PmcInitGenerator() :
    OpCodeGeneratorBase("Initialize PMC both HW and SW")
{
    SetOpcodeCommonParams();
}

PmcGetConfigGenerator::PmcGetConfigGenerator() :
    OpCodeGeneratorBase("Returns the current value (true/false) for the parameters - \nCollectIdleSmEvents, CollectRxPpduEvents, CollectTxPpduEvents, CollectUCodeEvents ")
{
    SetOpcodeCommonParams();
}

PmcSetConfigGenerator::PmcSetConfigGenerator() :
    OpCodeGeneratorBase("Set value (true/false) for the parameters - \nCollectIdleSmEvents, CollectRxPpduEvents, CollectTxPpduEvents, CollectUCodeEvents ")
{
    SetOpcodeCommonParams();
    RegisterParam(unique_ptr<ParameterObject>(new BoolParameterObject("CollectIdleSmEvents", false, "Collect Idle Sm Events")));
    RegisterParam(unique_ptr<ParameterObject>(new BoolParameterObject("CollectRxPpduEvents", false, "Collect Rx Ppdu Events")));
    RegisterParam(unique_ptr<ParameterObject>(new BoolParameterObject("CollectTxPpduEvents", false, "Collect Tx Ppdu Events")));
    RegisterParam(unique_ptr<ParameterObject>(new BoolParameterObject("CollectUCodeEvents", false, "Collect UCode Events")));
}

PmcGetStateGenerator::PmcGetStateGenerator() :
    OpCodeGeneratorBase("Get PMC state - \nActive (true/false), and PMC_GENERAL_0, PMC_GENERAL_1 values")
{
    SetOpcodeCommonParams();
}

PmcStartGenerator::PmcStartGenerator() :
    OpCodeGeneratorBase("Start PMC recording- \nCan be called when PMC is in state IDLE or READY (if called in IDLE will perform init implicitly)")
{
    SetOpcodeCommonParams();
}

PmcPauseGenerator::PmcPauseGenerator() :
    OpCodeGeneratorBase("Pause PMC recording- \nCan be called when PMC is in state ACTIVE (otherwise has no effect)")
{
    SetOpcodeCommonParams();
}

PmcStopGenerator::PmcStopGenerator() :
    OpCodeGeneratorBase("Stop PMC recording- \nCan be called when PMC is in state ACTIVE or PAUSED (otherwise has no effect) ")
{
    SetOpcodeCommonParams();
}

PmcShutdownGenerator::PmcShutdownGenerator() :
    OpCodeGeneratorBase("Shutdown PMC recording and put it back to idle. \nCan be invoked in any state.")
{
    SetOpcodeCommonParams();
}
