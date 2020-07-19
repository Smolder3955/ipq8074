/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "AccessCommandBuilder.h"

using namespace std;

AccessCommandBuilder::AccessCommandBuilder()
{
    SetOpcodeGenerators();
}

void AccessCommandBuilder::SetOpcodeGenerators()
{
    RegisterOpcodeGenerators("read", unique_ptr<ACReadGenerator>(new ACReadGenerator()));
    RegisterOpcodeGenerators("write", unique_ptr<ACWriteGenerator>(new ACWriteGenerator()));
    RegisterOpcodeGenerators("read_block", unique_ptr<ACReadBlockGenerator>(new ACReadBlockGenerator()));
}

ACReadGenerator::ACReadGenerator() :
    OpCodeGeneratorBase("Read one DWORD form the device")
{
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Device", false, "Select specific device.", "Any valid device name")));
    RegisterParam(unique_ptr<ParameterObject>(new IntParameterObject("Address", true, "Address to read from")));
}

ACWriteGenerator::ACWriteGenerator() :
    OpCodeGeneratorBase("Write one DWORD to the device")
{
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Device", false, "Select specific device.", "Any valid device name")));
    RegisterParam(unique_ptr<ParameterObject>(new IntParameterObject("Address", true, "Address to read from")));
    RegisterParam(unique_ptr<ParameterObject>(new IntParameterObject("Value", true, "one DWORD to write")));
}

ACReadBlockGenerator::ACReadBlockGenerator() :
OpCodeGeneratorBase("Read a block of DWORDs from the device")
{
    RegisterParam(unique_ptr<ParameterObject>(new StringParameterObject("Device", false, "Select specific device.", "Any valid device name")));
    RegisterParam(unique_ptr<ParameterObject>(new IntParameterObject("Address", true, "Start address to read from")));
    RegisterParam(unique_ptr<ParameterObject>(new IntParameterObject("Size", true, "Size in DWORDs")));
}




