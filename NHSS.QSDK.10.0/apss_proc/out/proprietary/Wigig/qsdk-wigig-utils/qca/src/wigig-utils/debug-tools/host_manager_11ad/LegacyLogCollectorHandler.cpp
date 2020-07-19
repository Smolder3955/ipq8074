/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
/*
* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sstream>

#include "LegacyLogCollectorHandler.h"
#include "Host.h"
#include "DeviceManager.h"

const char LegacyLogCollectorHandler::LIST_DELIMITER = ',';
const char LegacyLogCollectorHandler::DEVICE_RESULT_STRUCT_OPENER = '{';
const char LegacyLogCollectorHandler::DEVICE_RESULT_STRUCT_CLOSER = '}';
const char* const LegacyLogCollectorHandler::UNKNOWN_DEVICE = "unknown_device";
const char* const LegacyLogCollectorHandler::SUCCESS = "SUCCESS";
const char* const LegacyLogCollectorHandler::TRUE_STR = "True"; // TODO: move message formatting to higher level
const char* const LegacyLogCollectorHandler::FALSE_STR = "False"; // TODO: move message formatting to higher level
const char* const LegacyLogCollectorHandler::START_COLLECTION = "start_collection";
const char* const LegacyLogCollectorHandler::STOP_COLLECTION = "stop_collection";
const char* const LegacyLogCollectorHandler::IS_COLLECTING = "is_collecting";
const char* const LegacyLogCollectorHandler::START_RECORDING = "start_recording";
const char* const LegacyLogCollectorHandler::STOP_RECORDING = "stop_recording";
const char* const LegacyLogCollectorHandler::IS_RECORDING = "is_recording";
const char* const LegacyLogCollectorHandler::GET_CONFIGURATION = "get_configuration";
const char* const LegacyLogCollectorHandler::GET_PARAMETER = "get_parameter";
const char* const LegacyLogCollectorHandler::SET_PARAMETER = "set_parameter";
const char* const LegacyLogCollectorHandler::SET_DEFERRED_COLLECTION = "set_deferred_collection";
const char* const LegacyLogCollectorHandler::SET_DEFERRED_RECORDING = "set_deferred_recording";
const char* const LegacyLogCollectorHandler::HELP = "help";
const char* const LegacyLogCollectorHandler::VERSION_NUMBER = "1.1";

using namespace std;

bool LegacyLogCollectorHandler::HandleTcpCommand(const string& operation, const vector<string>& parameterList, string& statusMessage) const
{
    auto it = m_handlers.find(operation);
    if (m_handlers.end() == it)
    {
        statusMessage = "No handle exists for command " + operation + ". Use help command for more information.";
        return false;
    }

    return it->second(*this, parameterList, statusMessage);
}

bool LegacyLogCollectorHandler::CheckArgumentValidity(const std::vector<std::string> arguments,
    unsigned numberOfExpectedArguments, std::string& statusMessage) const
{
    if (arguments.size() != numberOfExpectedArguments)
    {
        stringstream ss;
        ss << "Expected " << numberOfExpectedArguments << " parameters but got " << arguments.size() << endl;
        statusMessage = ss.str();
        return false;
    }
    return true;
}

bool LegacyLogCollectorHandler::StartCollection(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 2 parameters: device list, cpu list
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    // currently start and stop are done for all devices and cpus
    if (Host::GetHost().GetDeviceManager().GetLogCollectionMode())
    {
        statusMessage = "logs are already being collected";
        return false;
    }

    Host::GetHost().GetDeviceManager().SetLogCollectionMode(true);

    return true;
}

bool LegacyLogCollectorHandler::StopCollection(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 2 parameters: device list, cpu list
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    // currently start and stop are done for all devices and cpus
    if (!Host::GetHost().GetDeviceManager().GetLogCollectionMode())
    {
        statusMessage = "logs aren't being collected";
        return false;
    }

    Host::GetHost().GetDeviceManager().SetLogCollectionMode(false);

    return true;
}

bool LegacyLogCollectorHandler::IsCollecting(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 2 parameters: device list, cpu list
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    // currently start and stop are done for all devices and cpus
    statusMessage = Host::GetHost().GetDeviceManager().GetLogCollectionMode() ? TRUE_STR : FALSE_STR;
    return true;
}

bool LegacyLogCollectorHandler::StartRecording(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 2 parameters: device list, cpu list
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    // currently start and stop are done for all devices and cpus
    if (Host::GetHost().GetDeviceManager().GetLogRecordingState())
    {
        statusMessage = "already recording logs";
        return false;
    }

    Host::GetHost().GetDeviceManager().SetLogRecording(true);

    return true;
}

bool LegacyLogCollectorHandler::StopRecording(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 2 parameters: device list, cpu list
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    // currently start and stop are done for all devices and cpus
    if (!Host::GetHost().GetDeviceManager().GetLogRecordingState())
    {
        statusMessage = "logs aren't being recorded";
        return false;
    }

    Host::GetHost().GetDeviceManager().SetLogRecording(false);

    return true;
}

bool LegacyLogCollectorHandler::IsRecording(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 2 parameters: device list, cpu list
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    // currently start and stop are done for all devices and cpus
    statusMessage = Host::GetHost().GetDeviceManager().GetLogRecordingState() ? TRUE_STR : FALSE_STR;
    return true;
}

bool LegacyLogCollectorHandler::GetConfiguration(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 2 parameters: device list, cpu list
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    // currently parameters[0] must be only one device and parameter[1] must be one cpu
    statusMessage = Host::GetHost().GetDeviceManager().GetLogCollectorConfigurationString(parameters[0], parameters[1]);
    return true;
}

bool LegacyLogCollectorHandler::GetParameter(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 3 parameters: device list, cpu list, parameter to be retrieved
    if (!CheckArgumentValidity(parameters, 3, statusMessage))
    {
        return false;
    }

    // currently parameters[0] must be only one device and parameter[1] must be one cpu
    statusMessage = Host::GetHost().GetDeviceManager().GetLogCollectionParameter(parameters[0], parameters[1], parameters[2]);
    return true;
}

bool LegacyLogCollectorHandler::SetParameter(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 3 parameters: device list, cpu list, parameter to be set and its new value (e.g. polling_interval_ms=300)
    if (!CheckArgumentValidity(parameters, 3, statusMessage))
    {
        return false;
    }

    // currently parameters[0] must be only one device and parameter[1] must be one cpu
    return Host::GetHost().GetDeviceManager().SetLogCollectionParameter(parameters[0], parameters[1], parameters[2], statusMessage);
}

bool LegacyLogCollectorHandler::StartDeferredCollection(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 3 parameters: device list, cpu list, whether to enter deferred mode or exit this mode
    if (!CheckArgumentValidity(parameters, 3, statusMessage))
    {
        return false;
    }

    // currently parameters[0] must be only one device and parameter[1] must be one cpu
    bool deferredCollection = TRUE_STR == parameters[2];
    Host::GetHost().GetDeviceManager().SetLogCollectionMode(deferredCollection);
    return true;
}

bool LegacyLogCollectorHandler::StartDeferredRecoring(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // 3 parameters: device list, cpu list, whether to enter deferred mode or exit this mode
    if (!CheckArgumentValidity(parameters, 3, statusMessage))
    {
        return false;
    }

    // currently parameters[0] must be only one device and parameter[1] must be one cpu
    bool deferredRecording = TRUE_STR == parameters[2];
    Host::GetHost().GetDeviceManager().SetLogRecording(deferredRecording);
    return true;
}


bool LegacyLogCollectorHandler::HandleHelpCommand(const std::vector<std::string>& parameters, std::string& statusMessage) const // TODO - to be completed
{
    // no parameters checks - we want help text to be displayed in all situations
    (void)parameters;

    const static string logCollectorCommand("log_collector");
    const static string deviceList("<device list>");
    const static string tracerType("<fw/ucode>");
    const static string commandArgumentSeparator("|");

    stringstream command;
    command << "Log collector version : " << VERSION_NUMBER << endl
        << "Start Recording       : " << logCollectorCommand << commandArgumentSeparator << START_RECORDING << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Stop Recording       : " << logCollectorCommand << commandArgumentSeparator << STOP_RECORDING << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Is Recording       : " << logCollectorCommand << commandArgumentSeparator << IS_RECORDING << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Set deferred Recording       : " << logCollectorCommand << commandArgumentSeparator << IS_RECORDING << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Start Collection       : " << logCollectorCommand << commandArgumentSeparator << START_COLLECTION << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Stop Collection       : " << logCollectorCommand << commandArgumentSeparator << STOP_COLLECTION << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Is Collecting       : " << logCollectorCommand << commandArgumentSeparator << IS_COLLECTING << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Get Configuration       : " << logCollectorCommand << commandArgumentSeparator << GET_CONFIGURATION << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Get Configuration Parameter       : " << logCollectorCommand << commandArgumentSeparator << SET_PARAMETER << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl
        << "Set Configuration Parameter       : " << logCollectorCommand << commandArgumentSeparator << SET_PARAMETER << commandArgumentSeparator << deviceList << commandArgumentSeparator << tracerType << endl;

    statusMessage = command.str();
    return true;
}
