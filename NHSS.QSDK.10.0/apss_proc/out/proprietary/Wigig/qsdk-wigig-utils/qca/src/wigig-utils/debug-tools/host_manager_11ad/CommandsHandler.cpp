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

#ifdef _WINDOWS
    #include "ioctl_if.h"
#endif

#include <sstream>
#include <string>
#include <set>

#include "CommandsHandler.h"
#include "SharedVersionInfo.h"
#include "Utils.h"
#include "Host.h"
#include "HostDefinitions.h"
#include "EventsDefinitions.h"
#include "FileReader.h"
#include "ShellCommandExecutor.h"
#include "FlashControlHandler.h"
#include "DriverConfigurationHandler.h"
#include "LegacyLogCollectorHandler.h"
#include "DeviceManager.h"
#include "HostInfo.h"
#include "MessageParser.h"
#include "DebugLogger.h"

using namespace std;

#define HANDLER_ENTER                                                                         \
    do {                                                                                      \
        std::stringstream ss;                                                                 \
        ss << "Command Handler: " << __FUNCTION__ << " args: " << numberOfArguments << " [ "; \
        for (const auto& s : arguments) ss << s << ' ';                                       \
        ss << ']';                                                                            \
        LOG_VERBOSE << ss.str() << endl;                                                      \
    } while(false)


 // *************************************************************************************************

const string CommandsHandler::TRUE_STR = "True";
const string CommandsHandler::FALSE_STR = "False";

// *************************************************************************************************

CommandsHandler::CommandsHandler(ServerType type, Host& host) :
    m_host(host)
{
    if (stTcp == type) // TCP server
    {
        m_functionHandler.insert(make_pair("r", &CommandsHandler::Read));
        m_functionHandler.insert(make_pair("rb", &CommandsHandler::ReadBlock));
        m_functionHandler.insert(make_pair("rb_fast", &CommandsHandler::ReadBlockFast));
        m_functionHandler.insert(make_pair("read_radar_data", &CommandsHandler::ReadRadarData));
        m_functionHandler.insert(make_pair("w", &CommandsHandler::Write));
        m_functionHandler.insert(make_pair("wb", &CommandsHandler::WriteBlock));
        m_functionHandler.insert(make_pair("interface_reset", &CommandsHandler::InterfaceReset));

        m_functionHandler.insert(make_pair("pmc_ver", &CommandsHandler::GetPmcAgentVersion));
        m_functionHandler.insert(make_pair("disable_pcie_aspm", &CommandsHandler::DisableASPM));
        m_functionHandler.insert(make_pair("enable_pcie_aspm", &CommandsHandler::EnableASPM));
        m_functionHandler.insert(make_pair("is_pcie_aspm_enabled", &CommandsHandler::GetASPM));
        m_functionHandler.insert(make_pair("alloc_pmc", &CommandsHandler::AllocPmc));
        m_functionHandler.insert(make_pair("dealloc_pmc", &CommandsHandler::DeallocPmc));
        m_functionHandler.insert(make_pair("create_pmc_file", &CommandsHandler::CreatePmcFile));
        m_functionHandler.insert(make_pair("read_pmc_file", &CommandsHandler::FindPmcDataFile));
        m_functionHandler.insert(make_pair("read_pmc_ring_file", &CommandsHandler::FindPmcRingFile));

        m_functionHandler.insert(make_pair("get_interfaces", &CommandsHandler::GetInterfaces));
        m_functionHandler.insert(make_pair("set_host_alias", &CommandsHandler::SetHostAlias));
        m_functionHandler.insert(make_pair("get_host_alias", &CommandsHandler::GetHostAlias));
        m_functionHandler.insert(make_pair("get_time", &CommandsHandler::GetTime));
        m_functionHandler.insert(make_pair("set_local_driver_mode", &CommandsHandler::SetDriverMode));
        m_functionHandler.insert(make_pair("get_host_manager_version", &CommandsHandler::GetHostManagerVersion));
        m_functionHandler.insert(make_pair("get_host_os", &CommandsHandler::GetHostOS));
        m_functionHandler.insert(make_pair("get_host_enumaration_mode", &CommandsHandler::GetHostEnumarationMode));
        m_functionHandler.insert(make_pair("set_host_enumaration_mode", &CommandsHandler::SetHostEnumarationMode));

        m_functionHandler.insert(make_pair("driver_control", &CommandsHandler::DriverControl));
        m_functionHandler.insert(make_pair("driver_command", &CommandsHandler::DriverCommand));
        m_functionHandler.insert(make_pair("set_silence_mode", &CommandsHandler::SetDeviceSilenceMode));
        m_functionHandler.insert(make_pair("get_silence_mode", &CommandsHandler::GetDeviceSilenceMode));
        m_functionHandler.insert(make_pair("get_connected_users", &CommandsHandler::GetConnectedUsers));
        m_functionHandler.insert(make_pair("get_device_capabilities_mask", &CommandsHandler::GetDeviceCapabilities));
        m_functionHandler.insert(make_pair("get_host_capabilities_mask", &CommandsHandler::GetHostCapabilities));
        m_functionHandler.insert(make_pair("log_collector", &CommandsHandler::LogCollectorHandler));
        m_functionHandler.insert(make_pair("flash_control", &CommandsHandler::FlashControlHandler));
        m_functionHandler.insert(make_pair("driver_configuration", &CommandsHandler::DriverConfiguration));
    }
    else // UDP server
    {
        m_functionHandler.insert(make_pair(/*"get_host_network_info"*/"GetHostIdentity", &CommandsHandler::GetHostNetworkInfo));
    }
}

// *************************************************************************************************

string CommandsHandler::DecorateResponseMessage(bool successStatus, const string& message)
{
    string status = successStatus ? "Success" : "Fail";
    string decoratedResponse = Utils::GetCurrentLocalTimeString() + m_reply_feilds_delimiter + status;
    if (message != "")
    {
        decoratedResponse += m_reply_feilds_delimiter + message;
    }
    return decoratedResponse;
}

// **************************************TCP commands handlers*********************************************************** //
ResponseMessage CommandsHandler::GetInterfaces(vector<string> arguments, unsigned int numberOfArguments)
{
    //do something with params
    (void)arguments;

    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 0, response.message))
    {
        set<string> devices;
        DeviceManagerOperationStatus status = m_host.GetDeviceManager().GetDevices(devices);

        if (dmosSuccess != status)
        {
            LOG_ERROR << "Error while trying to get interfaces. Error: " << m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
            response.message = (dmosNoSuchConnectedDevice == status) ? DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
        }
        else
        {
            // create one string that contains all connected devices
            stringstream devicesSs;
            bool firstTime = true;

            for (std::set<string>::const_iterator it = devices.begin(); it != devices.end(); ++it)
            {
                if (firstTime)
                {
                    devicesSs << *it;
                    firstTime = false;
                    continue;
                }
                devicesSs << m_device_delimiter << *it;
            }
            response.message = DecorateResponseMessage(true, devicesSs.str());
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::Read(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 2, response.message))
    {
        DWORD address;
        if (!Utils::ConvertHexStringToDword(arguments[1], address))
        {
            LOG_WARNING << "Error in Read arguments: given address isn't starting with 0x" << endl;
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            DWORD value;
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().Read(arguments[0], address, value);
            if (dmosSuccess != status)
            {
                if (dmosSilentDevice == status)
                {
                    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsDeviceIsSilent));
                }
                else
                {
                    LOG_ERROR << "Error while trying to read address " << arguments[1] << " from " + arguments[0] << ". Error: " + m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
                    response.message = (dmosNoSuchConnectedDevice == status) ? DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                        DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));

                }
            }
            else
            {
                stringstream message;
                message << "0x" << hex << value;
                response.message = DecorateResponseMessage(true, message.str());
            }
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::Write(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 3, response.message))
    {
        DWORD address, value;
        if (!Utils::ConvertHexStringToDword(arguments[1], address) || !Utils::ConvertHexStringToDword(arguments[2], value))
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().Write(arguments[0], address, value);
            if (dmosSuccess != status)
            {
                if (dmosSilentDevice == status)
                {
                    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsDeviceIsSilent));
                }
                else
                {
                    LOG_ERROR << "Error while trying to write value " << arguments[2] << " to " << arguments[1] + " on "
                        << arguments[0] + ". Error: "
                        << m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
                    response.message = (dmosNoSuchConnectedDevice == status) ?
                        DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                        DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
                }
            }
            else
            {
                response.message = DecorateResponseMessage(true);
            }
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::ReadBlock(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;

    ResponseMessage response;
    vector<DWORD> values;
    if (ReadBlockImpl(arguments, numberOfArguments, response, values))
    {
        LOG_VERBOSE << __FUNCTION__ << ": number of DWORDS received from Driver is " << Hex<>(values.size()) << endl;
        // encode each numeric result as string
        stringstream responseSs;
        auto it = values.begin();
        if (it != values.end())
        {
            responseSs << "0x" << hex << *it;
            ++it;
        }
        for (; it != values.end(); ++it)
        {
            responseSs << m_array_delimiter << "0x" << hex << *it;
        }
        response.message = DecorateResponseMessage(true, responseSs.str());
    }
    // otherwise, response is already decorated with the error

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************
ResponseMessage CommandsHandler::ReadBlockFast(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;

    ResponseMessage response;
    vector<DWORD> values;
    if (ReadBlockImpl(arguments, numberOfArguments, response, values))
    {
        LOG_VERBOSE << __FUNCTION__ << ": number of DWORDS received from Driver is " << Hex<>(values.size()) << endl;
        // encode all the binary block as Base64
        unsigned char* buffer = reinterpret_cast<unsigned char*>(values.data());
        const uint32_t blockSizeInBytes = values.size() * sizeof(DWORD);
        response.message = DecorateResponseMessage(true, Utils::Base64Encode(buffer, blockSizeInBytes)); // empty string if the buffer is empty
    }
    // otherwise, response is already decorated with the error

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************
bool CommandsHandler::ReadBlockImpl(const vector<string>& arguments, unsigned int numberOfArguments, ResponseMessage& response, vector<DWORD>& values)
{
    if (!ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 3, response.message))
    {
        return false;
    }

    DWORD address, blockSize;
    if (!Utils::ConvertHexStringToDword(arguments[1], address) || !Utils::ConvertHexStringToDword(arguments[2], blockSize))
    {
        LOG_WARNING << __FUNCTION__ << ": failed to convert address (" << arguments[1] << ") and block size in DWORDS (" << arguments[2] << ") parameters, expected in Hex format" << endl;
        response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        return false;
    }

    DeviceManagerOperationStatus status = m_host.GetDeviceManager().ReadBlock(arguments[0], address, blockSize, values);
    if (dmosSuccess != status)
    {
        if (dmosSilentDevice == status)
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsDeviceIsSilent));
        }
        else
        {
            LOG_ERROR << "Error while trying to read " << arguments[2] << " addresses starting at address "
                << arguments[1] << " from " << arguments[0] << ". Error: "
                << m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
            response.message = (dmosNoSuchConnectedDevice == status) ?
                DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
        }
        return false;
    }

    return true;
}

// *************************************************************************************************
ResponseMessage CommandsHandler::ReadRadarData(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 2, response.message))
    {
        DWORD maxBlockSize; // in DWORDS
        vector<DWORD> values;
        if (!Utils::ConvertHexStringToDword(arguments[1], maxBlockSize))
        {
            LOG_WARNING << __FUNCTION__ << ": failed to convert max block size in DWORDS parameter (" << arguments[1] << ") expected in Hex format" << endl;
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            LOG_DEBUG << __FUNCTION__ << ": maximal block size (in DWORDS) requested is " << Hex<>(maxBlockSize) << endl;
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().ReadRadarData(arguments[0], maxBlockSize, values);
            if (dmosSuccess != status)
            {
                if (dmosSilentDevice == status)
                {
                    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsDeviceIsSilent));
                }
                else
                {
                    LOG_ERROR << __FUNCTION__ << ": Error while trying to read radar data (max size " << arguments[1] << ") from " << arguments[0]
                        << ". Error: " << m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
                    response.message = (dmosNoSuchConnectedDevice == status) ?
                        DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                        DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
                }
            }
            else
            {
                LOG_DEBUG << __FUNCTION__ << ": number of DWORDS received from Driver is " << Hex<>(values.size()) << endl;
                // encode all the binary block received as Base64
                // the returned vector contains only data to be returned to the caller
                unsigned char* buffer = reinterpret_cast<unsigned char*>(values.data());
                const uint32_t blockSizeInBytes = values.size() * sizeof(DWORD);
                response.message = DecorateResponseMessage(true, Utils::Base64Encode(buffer, blockSizeInBytes)); // empty string if the buffer is empty
            }
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

// *************************************************************************************************
ResponseMessage CommandsHandler::WriteBlock(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 3, response.message))
    {
        DWORD address;
        vector<DWORD> values;
        if (!Utils::ConvertHexStringToDword(arguments[1], address) || !Utils::ConvertHexStringToDwordVector(arguments[2], m_array_delimiter, values))
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            // perform write block
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().WriteBlock(arguments[0], address, values);
            if (dmosSuccess != status)
            {
                if (dmosSilentDevice == status)
                {
                    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsDeviceIsSilent));
                }
                else
                {
                    LOG_ERROR << "Error in write blocks. arguments are:\nDevice name - " << arguments[0]
                        << "\nStart address - " << arguments[1] <<
                        "\nValues - " << arguments[2] << endl;
                    response.message = (dmosNoSuchConnectedDevice == status) ?
                        DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                        DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
                }
            }
            else
            {
                response.message = DecorateResponseMessage(true);
            }
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::InterfaceReset(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 1, response.message))
    {
        OperationStatus status = m_host.GetDeviceManager().InterfaceReset(arguments[0]);
        if (!status)
        {
            LOG_ERROR << "Failed to perform interface reset on " << arguments[0] << ". Error: " << status.GetStatusMessage() << endl;
            response.message = DecorateResponseMessage(false, status.GetStatusMessage());
        }
        else
        {
            response.message = DecorateResponseMessage(true);
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

// *************************************************************************************************

ResponseMessage CommandsHandler::GetPmcAgentVersion(vector<string> arguments, unsigned int numberOfArguments)
{
    (void)numberOfArguments;
    LOG_VERBOSE << __FUNCTION__ << endl;
    stringstream ss;
    for (const auto& s : arguments)
    {
        ss << "," << s;
    }
    LOG_VERBOSE << ss.str() << endl;

    ResponseMessage response;

    static const char* const PMC_AGENT_VER = "1.1";

    if (ValidArgumentsNumber(__FUNCTION__, arguments.size(), 1, response.message))
    {
        response.message = DecorateResponseMessage(true, PMC_AGENT_VER);
    }

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}


// *************************************************************************************************

ResponseMessage CommandsHandler::EnableASPM(vector<string> arguments, unsigned int numberOfArguments)
{
    HANDLER_ENTER;
    ResponseMessage response;

#ifdef _WINDOWS
    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsLinuxSupportOnly));
#else

    LOG_DEBUG << "Enabling ASPM (SPARROW-only)" << std::endl;
    ShellCommandExecutor disableAspmCommand("setpci -d:310 80.b=2:3");
    LOG_DEBUG << disableAspmCommand << std::endl;

    // setpci always exits with zero, even when failure occurs
    if (0 == disableAspmCommand.ExitStatus() &&
        0 == disableAspmCommand.Output().size())
    {
        response.message = DecorateResponseMessage(true);
    }
    else
    {
        response.message = DecorateResponseMessage(false, disableAspmCommand.ToString());
    }

#endif

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

// ***********************************************************************************************

ResponseMessage CommandsHandler::DisableASPM(vector<string> arguments, unsigned int numberOfArguments)
{
    HANDLER_ENTER;
    ResponseMessage response;

#ifdef _WINDOWS
    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsLinuxSupportOnly));
#else

    LOG_DEBUG << "Disabling ASPM (SPARROW-only)" << std::endl;
    ShellCommandExecutor disableAspmCommand("setpci -d:310 80.b=0:3");
    LOG_DEBUG << disableAspmCommand << std::endl;

    // setpci always exits with zero, even when failure occurs
    if (0 == disableAspmCommand.ExitStatus() &&
        0 == disableAspmCommand.Output().size())
    {
        response.message = DecorateResponseMessage(true);
    }
    else
    {
        response.message = DecorateResponseMessage(false, disableAspmCommand.ToString());
    }

#endif

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

// *************************************************************************************************

ResponseMessage CommandsHandler::GetASPM(vector<string> arguments, unsigned int numberOfArguments)
{
    HANDLER_ENTER;
    ResponseMessage response;

#ifdef _WINDOWS
    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsLinuxSupportOnly));
#else

    LOG_DEBUG << "Querying ASPM (SPARROW-only)" << std::endl;
    ShellCommandExecutor disableAspmCommand("setpci -d:310 80.b");
    LOG_DEBUG << disableAspmCommand << std::endl;

    // setpci always exits with zero, even when failure occurs
    if (0 == disableAspmCommand.ExitStatus() &&
        1 == disableAspmCommand.Output().size())
    {
        response.message = DecorateResponseMessage(
            true, ((disableAspmCommand.Output()[0] == "00") ? FALSE_STR : TRUE_STR));
    }
    else
    {
        response.message = DecorateResponseMessage(false, disableAspmCommand.ToString());
    }

#endif

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

// ***********************************************************************************************

ResponseMessage CommandsHandler::AllocPmc(vector<string> arguments, unsigned int numberOfArguments)
{
    HANDLER_ENTER;
    ResponseMessage response;

#ifdef _WINDOWS
    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsLinuxSupportOnly));
#else

    if (ValidArgumentsNumber(__FUNCTION__, arguments.size(), 3, response.message))
    {
        unsigned descSize;
        unsigned descNum;
        if (!Utils::ConvertDecimalStringToUnsignedInt(arguments[1], descSize) ||
            !Utils::ConvertDecimalStringToUnsignedInt(arguments[2], descNum))
        {
            stringstream error;
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            std::string errorMsg;
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().AllocPmc(arguments[0], descSize, descNum, errorMsg);
            if (dmosSuccess != status)
            {
                stringstream error;
                LOG_ERROR << "PMC allocation Failed: " << errorMsg << std::endl;
                response.message = DecorateResponseMessage(false, errorMsg);
            }
            else
            {
                response.message = DecorateResponseMessage(true);
            }
        }
    }

#endif

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::DeallocPmc(vector<string> arguments, unsigned int numberOfArguments)
{
    HANDLER_ENTER;
    ResponseMessage response;

#ifdef _WINDOWS
    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsLinuxSupportOnly));
#else

    bool bSuppressError = false;

    if (arguments.size() < 1 || arguments.size() > 2)
    {

        stringstream error;
        LOG_WARNING << "Mismatching number of arguments in " << __FUNCTION__ << ": expected [1 .. 2] but got " << numberOfArguments << endl;
        response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidNumberOfArguments));

        response.type = REPLY_TYPE_BUFFER;
        response.length = response.message.size();
        return response;
    }

    if (2 == arguments.size())
    {
        // PMV v1.6+
        if (!Utils::ConvertStringToBool(arguments[1], bSuppressError))
        {
            stringstream error;
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
    }

    std::string errorMsg;
    DeviceManagerOperationStatus status = m_host.GetDeviceManager().DeallocPmc(arguments[0], bSuppressError, errorMsg);
    if (dmosSuccess != status)
    {
        stringstream error;
        LOG_ERROR << "PMC de-allocation Failed: " << errorMsg << std::endl;
        response.message = DecorateResponseMessage(false, errorMsg);
    }
    else
    {
        response.message = DecorateResponseMessage(true);
    }

#endif

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

// *************************************************************************************************

ResponseMessage CommandsHandler::CreatePmcFile(vector<string> arguments, unsigned int numberOfArguments)
{
    HANDLER_ENTER;
    ResponseMessage response;

#ifdef _WINDOWS
    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsLinuxSupportOnly));
#else

    if (ValidArgumentsNumber(__FUNCTION__, arguments.size(), 2, response.message))
    {
        unsigned refNumber;
        if (!Utils::ConvertDecimalStringToUnsignedInt(arguments[1], refNumber))
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            std::string outMsg;
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().CreatePmcFiles(arguments[0], refNumber, outMsg);
            if (dmosSuccess != status)
            {
                stringstream error;
                LOG_ERROR << "PMC data file creation failed: " << outMsg << std::endl;
                response.message = DecorateResponseMessage(false, outMsg);
            }
            else
            {
                response.message = DecorateResponseMessage(true, outMsg);
            }
        }
    }

#endif

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::FindPmcDataFile(vector<string> arguments, unsigned int numberOfArguments)
{
    HANDLER_ENTER;
    ResponseMessage response;

#ifdef _WINDOWS
    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsLinuxSupportOnly));
#else

    if (ValidArgumentsNumber(__FUNCTION__, arguments.size(), 2, response.message))
    {
        unsigned refNumber;
        if (!Utils::ConvertDecimalStringToUnsignedInt(arguments[1], refNumber))
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            std::string outMessage;
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().FindPmcDataFile(arguments[0], refNumber, outMessage);
            if (dmosSuccess != status)
            {
                stringstream error;
                LOG_ERROR << "PMC data file lookup failed: " << outMessage << std::endl;
                response.message = DecorateResponseMessage(false, outMessage);
            }
            else
            {
                response.message = outMessage;
                response.type = REPLY_TYPE_FILE;
            }
        }
    }

#endif // _WINDOWS

    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::FindPmcRingFile(vector<string> arguments, unsigned int numberOfArguments)
{
    HANDLER_ENTER;
    ResponseMessage response;

#ifdef _WINDOWS
    response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsLinuxSupportOnly));
#else

    if (ValidArgumentsNumber(__FUNCTION__, arguments.size(), 2, response.message))
    {
        unsigned refNumber;
        if (!Utils::ConvertDecimalStringToUnsignedInt(arguments[1], refNumber))
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            std::string outMessage;
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().FindPmcRingFile(arguments[0], refNumber, outMessage);
            if (dmosSuccess != status)
            {
                stringstream error;
                LOG_ERROR << "PMC ring file lookup failed: " << outMessage << std::endl;
                response.message = DecorateResponseMessage(false, outMessage);
            }
            else
            {
                response.message = outMessage;
                response.type = REPLY_TYPE_FILE;
            }
        }
    }

#endif // _WINDOWS

    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::SetHostAlias(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 1, response.message))
    {
        if (m_host.GetHostInfo().SaveAliasToFile(arguments[0]))
        {
            response.message = DecorateResponseMessage(true);
        }
        else
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::GetHostAlias(vector<string> arguments, unsigned int numberOfArguments)
{
    (void)arguments;

    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 0, response.message))
    {
        response.message = DecorateResponseMessage(true, m_host.GetHostInfo().GetAlias());
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::GetHostCapabilities(vector<string> arguments, unsigned int numberOfArguments)
{
    (void)arguments;

    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 0, response.message))
    {
        stringstream message;
        message << m_host.GetHostInfo().GetHostCapabilities();
        response.message = DecorateResponseMessage(true, message.str());
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::LogCollectorHandler(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    // arguments[0] - device name
    // arguments[1] - cpu type
    // arguments[2] - operation
    if (0 == numberOfArguments)
    {
        LOG_WARNING << "Got 0 aruments while expecting at least for command to run. Use HELP command to get more info" << endl;
        response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidNumberOfArguments));
        response.type = REPLY_TYPE_BUFFER;
        response.length = response.message.size();
        return response;
    }

    string statusMessage;
    vector<string> parameters(arguments.begin() + 1, arguments.end()); // TODO: impove efficency
    LegacyLogCollectorHandler& handler = LegacyLogCollectorHandler::GetLogCollectorHandler();
    bool res = handler.HandleTcpCommand(arguments[0], parameters, statusMessage);
    response.message = DecorateResponseMessage(res, statusMessage);
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::FlashControlHandler(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    // arguments[0] - device name
    // arguments[1] - operation
    // arguments[2] - parameter list (optional)
    if (numberOfArguments < 1 || numberOfArguments > 3)
    {
        LOG_WARNING << "Mismatching number of arguments in " << __FUNCTION__ << ": expected for  1-3 arguments" << " but got " << numberOfArguments << endl;
        response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidNumberOfArguments));
        response.type = REPLY_TYPE_BUFFER;
        response.length = response.message.size();
        return response;
    }

    FlashControlCommandsHandler& flashHandler = FlashControlCommandsHandler::GetFlashControlCommandsHandler();
    string statusMessage;
    string parameterList;
    if (3 == arguments.size())
    {
        parameterList = arguments[2];
    }
    bool res = flashHandler.HandleTcpCommand(arguments[1] /*deviceList*/, arguments[0] /*operation*/, parameterList /*parameterList*/, statusMessage);
    response.message = DecorateResponseMessage(res, statusMessage);
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::DriverConfiguration(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    // arguments[0] - operation
    // arguments[1] - parameter list (optional)
    if (numberOfArguments < 1 || numberOfArguments > 2)
    {
        LOG_WARNING << "Mismatching number of arguments in " << __FUNCTION__ << ": expected for 1 or 2 arguments" << " but got " << numberOfArguments << endl;
        response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidNumberOfArguments));
        response.type = REPLY_TYPE_BUFFER;
        response.length = response.message.size();
        return response;
    }

    DriverConfigurationHandler& driverHandler = DriverConfigurationHandler::GetDriverConfigurationHandler();
    string statusMessage;
    string parameterList;
    if (2 == arguments.size())
    {
        parameterList = arguments[1];
    }
    bool res = driverHandler.HandleTcpCommand(arguments[0] /*operation*/, parameterList /*parameterList*/, statusMessage);
    response.message = DecorateResponseMessage(res, statusMessage);
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::GetTime(vector<string> arguments, unsigned int numberOfArguments)
{
    //do something with params
    (void)arguments;
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 0, response.message))
    {
        response.message = DecorateResponseMessage(true);
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

// *************************************************************************************************
ResponseMessage CommandsHandler::SetDriverMode(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 2, response.message))
    {
#ifdef _WINDOWS
        int newMode = IOCTL_WBE_MODE;
        int oldMode = IOCTL_WBE_MODE;

        if ("WBE_MODE" == arguments[1])
        {
            newMode = IOCTL_WBE_MODE;
        }
        else if ("WIFI_STA_MODE" == arguments[1])
        {
            newMode = IOCTL_WIFI_STA_MODE;
        }
        else if ("WIFI_SOFTAP_MODE" == arguments[1])
        {
            newMode = IOCTL_WIFI_SOFTAP_MODE;
        }
        else if ("CONCURRENT_MODE" == arguments[1])
        {
            newMode = IOCTL_CONCURRENT_MODE;
        }
        else if ("SAFE_MODE" == arguments[1])
        {
            newMode = IOCTL_SAFE_MODE;
        }
        else
        {
            // TODO
            response.message = dmosFail;
            response.type = REPLY_TYPE_BUFFER;
            response.length = response.message.size();
            return response;
        }
#else
        int newMode = 0;
        int oldMode = 0;
#endif
        DeviceManagerOperationStatus status = m_host.GetDeviceManager().SetDriverMode(arguments[0], newMode, oldMode);
        if (dmosSuccess != status)
        {
            LOG_ERROR << "Failed to set driver mode on " << arguments[0] << ". Error: " << m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
            response.message = (dmosNoSuchConnectedDevice == status) ? DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
        }
#ifdef _WINDOWS
        else
        {
            string message;

            switch (oldMode)
            {
            case IOCTL_WBE_MODE:
                message = "WBE_MODE";
                break;
            case IOCTL_WIFI_STA_MODE:
                message = "WIFI_STA_MODE";
                break;
            case IOCTL_WIFI_SOFTAP_MODE:
                message = "WIFI_SOFTAP_MODE";
                break;
            case IOCTL_CONCURRENT_MODE:
                message = "CONCURRENT_MODE";
                break;
            case IOCTL_SAFE_MODE:
                message = "SAFE_MODE";
                break;
            default:
                break;
            }

            response.message = DecorateResponseMessage(true, message);
        }
#endif
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::GetHostManagerVersion(vector<string> arguments, unsigned int numberOfArguments)
{
    //do something with params
    (void)arguments;
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 0, response.message))
    {
        string res = GetToolsVersion();
        response.message = DecorateResponseMessage(true, res);
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************
ResponseMessage CommandsHandler::GetHostOS(vector<string> arguments, unsigned int numberOfArguments)
{
    (void)arguments;
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 0, response.message))
    {
        string res = m_host.GetHostInfo().GetOSName();
        response.message = DecorateResponseMessage(true, res);
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

// *************************************************************************************************
ResponseMessage CommandsHandler::GetHostEnumarationMode(vector<string> arguments, unsigned int numberOfArguments)
{
    //do something with params
    (void)arguments;
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 0, response.message))
    {
        string res = m_host.IsEnumerating() ? "true" : "false";
        response.message = DecorateResponseMessage(true, res);
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ResponseMessage CommandsHandler::SetHostEnumarationMode(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    bool enumarationMode = false;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 1, response.message))
    {
        if (!Utils::ConvertStringToBool(arguments[0], enumarationMode))
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        m_host.SetIsEnumerating(enumarationMode);

        string mode = enumarationMode ? "Enumarating" : "Not_Enumarating";
        LOG_INFO << "Host:" << "is now " << mode << endl;
        stringstream message;
        message << "Enumaration mode set to:" << mode;
        response.message = DecorateResponseMessage(true, message.str());

    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}


// *************************************************************************************************

ResponseMessage CommandsHandler::DriverCommand(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 5, response.message))
    {
        DWORD commandId, inBufSize, outBufSize;
        std::vector<unsigned char> inputBuf;
        if (!(Utils::ConvertHexStringToDword(arguments[1], commandId)
            && Utils::ConvertHexStringToDword(arguments[2], inBufSize)
            && Utils::ConvertHexStringToDword(arguments[3], outBufSize)
            && Utils::Base64Decode(arguments[4], inputBuf)
            && inputBuf.size() == inBufSize)) // inBufSize is the size of the original binary buffer before Base64 encoding
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        else
        {
            if (g_LogConfig.ShouldPrint(LOG_SEV_VERBOSE)) // spare all the formatting when not needed
            {
                stringstream ss;
                ss << __FUNCTION__ << ": [command id: " << Hex<>(commandId);
                ss << " - " << (commandId == 0 ? "WMI" : commandId == 1 ? "Generic command" : "Unknown");
                ss << "], Payload (bytes):" << endl;
                for (const auto& byteVal : inputBuf)
                {
                    ss << "\t" << Hex<>(byteVal) << endl;
                }
                LOG_VERBOSE << ss.str() << endl;
            }

            std::vector<unsigned char> outputBuf(outBufSize, 0);

            DeviceManagerOperationStatus status =
                m_host.GetDeviceManager().DriverControl(arguments[0], commandId, inputBuf.data(), inBufSize, outBufSize ? outputBuf.data() : nullptr, outBufSize);

            if (dmosSuccess != status)
            {
                LOG_DEBUG << __FUNCTION__ << ": Failed to send Driver control command" << endl; // reason was reported as error
                response.message = DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status));
            }
            else
            {
                LOG_DEBUG << __FUNCTION__ << ": Success" << endl;
                response.message = DecorateResponseMessage(true, Utils::Base64Encode(outputBuf)); // empty string if the buffer is empty
            }
        }
    }

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

// *************************************************************************************************
ResponseMessage CommandsHandler::DriverControl(vector<string> arguments, unsigned int numberOfArguments)
{
    //cout << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 4, response.message))
    {
        DWORD inBufSize;
        //vector<DWORD> inputValues;
        if (!Utils::ConvertHexStringToDword(arguments[2], inBufSize))
        {
            response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
        }
        response.inputBufSize = inBufSize;
    }
    response.internalParsedMessage = arguments;
    response.type = REPLY_TYPE_WAIT_BINARY;
    return response;
}
// *************************************************************************************************

// *************************************************************************************************
ResponseMessage CommandsHandler::GenericDriverIO(vector<string> arguments, void* inputBuf, unsigned int /*inputBufSize*/)
{
    //cout << __FUNCTION__ << endl;
    ResponseMessage response;
    DWORD id, inBufSize, outBufSize;
    //vector<DWORD> inputValues;
    if (!Utils::ConvertHexStringToDword(arguments[1], id) || !Utils::ConvertHexStringToDword(arguments[2], inBufSize) || !Utils::ConvertHexStringToDword(arguments[3], outBufSize))
    {
        response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
    }

    else {
        DeviceManagerOperationStatus status;

        uint8_t* outputBuf = new uint8_t[outBufSize];
        memset(outputBuf, 0, outBufSize);

        status = m_host.GetDeviceManager().DriverControl(arguments[0], id, inputBuf, inBufSize, outputBuf, outBufSize);
        response.length = outBufSize;

        if (dmosSuccess != status)
        {
            LOG_DEBUG << "Driver IO command handler: Failed to execute driver IOCTL operation" << endl;
            response.message = (dmosNoSuchConnectedDevice == status) ? DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
            response.binaryMessage = (uint8_t*)"Failed to read from driver";
        }
        else
        {
            LOG_DEBUG << "Driver IO command handler: Success" << endl;
            response.binaryMessage = (uint8_t*)outputBuf;
        }
    }

    response.type = REPLY_TYPE_BINARY;
    return response;
}


ResponseMessage CommandsHandler::GetDeviceSilenceMode(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 1, response.message))
    {
        bool silentMode;
        DeviceManagerOperationStatus status = m_host.GetDeviceManager().GetDeviceSilentMode(arguments[0], silentMode);
        if (dmosSuccess != status)
        {
            LOG_ERROR << "Error while trying to GetDeviceSilenceMode at " << arguments[0] << ". Error: " + m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
            response.message = (dmosNoSuchConnectedDevice == status) ? DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
        }
        else
        {
            stringstream message;
            message << (silentMode ? 1 : 0);
            response.message = DecorateResponseMessage(true, message.str());
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}

ResponseMessage CommandsHandler::SetDeviceSilenceMode(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    bool silentMode = false;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 2, response.message))
    {
        {
            if (!Utils::ConvertStringToBool(arguments[1], silentMode))
            {
                response.message = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidArgument));
            }
            DeviceManagerOperationStatus status = m_host.GetDeviceManager().SetDeviceSilentMode(arguments[0], silentMode);
            if (dmosSuccess != status)
            {
                LOG_ERROR << "Error while trying to SetDeviceSilenceMode at: " << arguments[0] << " to: " + arguments[1] << ". Error: " + m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
                response.message = (dmosNoSuchConnectedDevice == status) ? DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                    DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
            }
            else
            {
                string mode = silentMode ? "Silenced" : "UnSilenced";
                LOG_INFO << "Device:" << arguments[0] << "is now " << mode << endl;
                stringstream message;
                message << "Silent mode set to:" << silentMode;
                response.message = DecorateResponseMessage(true, message.str());
            }
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    Host::GetHost().PushEvent(SilenceStatusChangedEvent(arguments[0], silentMode));
    return response;
}

ResponseMessage CommandsHandler::GetConnectedUsers(vector<string> arguments, unsigned int numberOfArguments)
{
    (void)arguments;

    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 0, response.message))
    {

        std::set<std::string> connectedUserList = m_host.GetHostInfo().GetConnectedUsers();
        stringstream os;
        for (const string & cl : connectedUserList)
        {
            os << cl << " ";
        }

        response.message = DecorateResponseMessage(true, os.str());
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;

}
ResponseMessage CommandsHandler::GetDeviceCapabilities(vector<string> arguments, unsigned int numberOfArguments)
{
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;
    if (ValidArgumentsNumber(__FUNCTION__, numberOfArguments, 1, response.message))
    {
        DWORD deviceCapabilitiesMask;
        DeviceManagerOperationStatus status = m_host.GetDeviceManager().GetDeviceCapabilities(arguments[0], deviceCapabilitiesMask);
        if (dmosSuccess != status)
        {
            LOG_ERROR << "Error while trying to GetDeviceCapabilities at " << arguments[0] << ". Error: " + m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status) << endl;
            response.message = (dmosNoSuchConnectedDevice == status) ? DecorateResponseMessage(false, m_host.GetDeviceManager().GetDeviceManagerOperationStatusString(status)) :
                DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsOperationFailure));
        }
        else
        {
            stringstream message;
            message << deviceCapabilitiesMask;
            response.message = DecorateResponseMessage(true, message.str());
        }
    }
    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

// **************************************UDP commands handlers*********************************************************** //
ResponseMessage CommandsHandler::GetHostNetworkInfo(vector<string> arguments, unsigned int numberOfArguments)
{
    //do something with params
    (void)numberOfArguments;
    LOG_VERBOSE << __FUNCTION__ << endl;
    ResponseMessage response;

    if (arguments.size() != 0)
    {
        response.message = DecorateResponseMessage(false, "Failed to get host's info: expected zero argument");
    }
    else
    {
        response.message = "GetHostIdentity;" + m_host.GetHostInfo().GetIps().m_ip + ";" + m_host.GetHostInfo().GetAlias();
    }

    response.type = REPLY_TYPE_BUFFER;
    response.length = response.message.size();
    return response;
}
// *************************************************************************************************

ConnectionStatus CommandsHandler::ExecuteCommand(const string& message, ResponseMessage &referencedResponse)
{
    LOG_VERBOSE << ">>> Command: " << message << std::endl;

    MessageParser messageParser(message);
    string commandName = messageParser.GetCommandFromMessage();

    if (m_functionHandler.find(commandName) == m_functionHandler.end())
    { //There's no such a command, the return value from the map would be null
        LOG_WARNING << "Unknown command from client: " << commandName << endl;
        referencedResponse.message = "Unknown command: " + commandName;
        referencedResponse.length = referencedResponse.message.size();
        referencedResponse.type = REPLY_TYPE_BUFFER;
        return KEEP_CONNECTION_ALIVE;
    }
    referencedResponse = (this->*m_functionHandler[commandName])(messageParser.GetArgsFromMessage(), messageParser.GetNumberOfArgs()); //call the function that fits commandName

    LOG_VERBOSE << "<<< Reply: " << referencedResponse.message << std::endl;
    return KEEP_CONNECTION_ALIVE;
}

// *************************************************************************************************

ConnectionStatus CommandsHandler::ExecuteBinaryCommand(uint8_t* binaryInput, ResponseMessage &referencedResponse)
{
    referencedResponse = GenericDriverIO(referencedResponse.internalParsedMessage, binaryInput, referencedResponse.inputBufSize);

    return KEEP_CONNECTION_ALIVE;
}

// *************************************************************************************************

string CommandsHandler::GetCommandsHandlerResponseStatusString(CommandsHandlerResponseStatus status)
{
    switch (status)
    {
    case chrsInvalidNumberOfArguments:
        return "Invalid arguments number";
    case chrsInvalidArgument:
        return "Invalid argument type";
    case chrsOperationFailure:
        return "Operation failure";
    case chrsLinuxSupportOnly:
        return "Linux support only";
    case chrsSuccess:
        return "Success";
    case chrsDeviceIsSilent:
        return "SilentDevice";
    default:
        return "CommandsHandlerResponseStatus is unknown";
    }
}

// *************************************************************************************************

bool CommandsHandler::ValidArgumentsNumber(string functionName, size_t numberOfArguments, size_t expectedNumOfArguments, string& responseMessage)
{
    if (expectedNumOfArguments != numberOfArguments)
    {
        stringstream error;
        LOG_WARNING << "Mismatching number of arguments in " << functionName << ": expected " << expectedNumOfArguments << " but got " << numberOfArguments << endl;
        responseMessage = DecorateResponseMessage(false, GetCommandsHandlerResponseStatusString(chrsInvalidNumberOfArguments));
        return false;
    }
    return true;
}
