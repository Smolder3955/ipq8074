/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <iostream>
#include <sstream>
#include "TcpClient.h"
#include "HostCommandBuilder.h"

using namespace std;


HostCommandBuilder::HostCommandBuilder()
{
    SetOpcodeGenerators();
}

void HostCommandBuilder::SetOpcodeGenerators()
{
    RegisterOpcodeGenerators("get_interfaces", unique_ptr<HCGetInterfacesGenerator>(new HCGetInterfacesGenerator()));
    RegisterOpcodeGenerators("get_version", unique_ptr<HCGetVersionGenerator>(new HCGetVersionGenerator()));
}


HCGetInterfacesGenerator::HCGetInterfacesGenerator() :
    OpCodeGeneratorBase("Get a list of available interfaces")
{
}

HCGetVersionGenerator::HCGetVersionGenerator() :
    OpCodeGeneratorBase("Get The Host manager version on target machine")
{
}
