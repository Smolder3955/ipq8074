/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "LogCollectorCommandBuilder.h"

using namespace std;


LogCollectorCommandBuilder::LogCollectorCommandBuilder()
{
    SetOpcodeGenerators();
}

void LogCollectorCommandBuilder::SetOpcodeGenerators()
{
    RegisterOpcodeGenerators("get_version", unique_ptr<LCGetVersionGenerator>(new LCGetVersionGenerator()));
    RegisterOpcodeGenerators("get_state", unique_ptr<LCGetStateGenerator>(new LCGetStateGenerator()));
    RegisterOpcodeGenerators("get_config", unique_ptr<LCGetConfigGenerator>(new LCGetConfigGenerator()));
    RegisterOpcodeGenerators("set_config", unique_ptr<LCSetConfigGenerator>(new LCSetConfigGenerator()));
    RegisterOpcodeGenerators("reset_config", unique_ptr<LCReSetConfigGenerator>(new LCReSetConfigGenerator()));
    RegisterOpcodeGenerators("set_verbosity", unique_ptr<LCSetVerbosityGenerator>(new LCSetVerbosityGenerator()));
    RegisterOpcodeGenerators("get_verbosity", unique_ptr<LCGetVerbosityGenerator>(new LCGetVerbosityGenerator()));
    RegisterOpcodeGenerators("start_recording", unique_ptr<LCStartRecordingGenerator>(new LCStartRecordingGenerator()));
    RegisterOpcodeGenerators("stop_recording", unique_ptr<LCStopRecordingGenerator>(new LCStopRecordingGenerator()));
    RegisterOpcodeGenerators("start_deferred_recording", unique_ptr<LCStartDeferredRecordingGenerator>(new LCStartDeferredRecordingGenerator()));
    RegisterOpcodeGenerators("stop_deferred_recording", unique_ptr<LCStopDeferredRecordingGenerator>(new LCStopDeferredRecordingGenerator()));
}


LCGetVersionGenerator::LCGetVersionGenerator():
    OpCodeGeneratorBase("Print the LogCollector version implemented by host_manager_11ad")
{
}

LCGetStateGenerator::LCGetStateGenerator():
    OpCodeGeneratorBase("Get Recording and Publishing state of host manager ")
{
    SetOpcodeCommonParams();
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("CpuType", true, "Select specific cpu.", "fw / ucode")));
}

LCGetConfigGenerator::LCGetConfigGenerator() :
    OpCodeGeneratorBase("Get log collector configuration.")
{
    SetOpcodeCommonParams();
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("CpuType", true, "Select specific cpu.", "fw / ucode")));
}

LCStartRecordingGenerator::LCStartRecordingGenerator() :
    OpCodeGeneratorBase("Start log recording according to provided parameters")
{
    SetOpcodeCommonParams();
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("CpuType", false, "Select specific cpu.", "fw / ucode")));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("RecordingType", false, "Select a recording type.", "raw(default) / txt")));

}

LCStopRecordingGenerator::LCStopRecordingGenerator() :
    OpCodeGeneratorBase("Stop log recording according to provided parameters")
{
    SetOpcodeCommonParams();
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("CpuType", false, "Select specific cpu.", "fw / ucode")));
}

LCStartDeferredRecordingGenerator::LCStartDeferredRecordingGenerator():
    OpCodeGeneratorBase("Start log recording for a new discovered device")
{
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("RecordingType", false, "Select a recording type.", "raw(default) / txt")));
}


LCStopDeferredRecordingGenerator::LCStopDeferredRecordingGenerator() :
    OpCodeGeneratorBase("Stop log recording for a new discovered device")
{
}


LCSetConfigGenerator::LCSetConfigGenerator() :
    OpCodeGeneratorBase("Set log collector configuration- \npolling interval, log file maximal size, file suffix etc..")
{
    SetOpcodeCommonParams();
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("CpuType", true, "Select specific cpu.", "fw / ucode")));
    RegisterParam(unique_ptr<ParameterObject>(new IntParameterObject("PollingIntervalMs", false, "Set the polling interval for log collection")));
    RegisterParam(unique_ptr<ParameterObject>(new IntParameterObject("MaxSingleFileSizeMb", false, "Set max size in MB for a single file")));
    RegisterParam(unique_ptr<ParameterObject>(new IntParameterObject("MaxNumberOfLogFiles", false, "Set max number of files created before overriding")));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("OutputFileSuffix", false, "Set the file suffix (default is .log)")));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("ConversionFilePath", false, "Set the path to strings conversion files (both ucode and fw)")));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("LogFilesDirectory", false, "Set the path for the log recording directory")));
}

LCReSetConfigGenerator::LCReSetConfigGenerator():
    OpCodeGeneratorBase("ReSet default log collector configuration")
{
}

LCSetVerbosityGenerator::LCSetVerbosityGenerator() :
    OpCodeGeneratorBase("Set FW / UCODE module verbosity. \nV-verbose, I-information, E-error, W-warning")
{
    const string moduleParamDescription = "Set specific module verbosity - Advanced";
    const string moduleParamValues = "Any combination of {V, I, E, W}, e.g V, VI, IE .. or empty(\"\")";
    SetOpcodeCommonParams();

    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("CpuType", false, "Select specific cpu.", "fw / ucode")));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("DefaultVerbosity", false, "Set Verbosity level for all modules", moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module0", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module1", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module2", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module3", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module4", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module5", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module6", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module7", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module8", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module9", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module10", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module11", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module12", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module13", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module14", false, moduleParamDescription, moduleParamValues, true)));
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Module15", false, moduleParamDescription, moduleParamValues, true)));
}

LCGetVerbosityGenerator::LCGetVerbosityGenerator() :
    OpCodeGeneratorBase("Get FW / UCODE module verbosity. \nV-verbose, I-information, E-error, W-warning")
{
    SetOpcodeCommonParams();
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("CpuType", true, "Select specific cpu.", "fw / ucode")));
}


