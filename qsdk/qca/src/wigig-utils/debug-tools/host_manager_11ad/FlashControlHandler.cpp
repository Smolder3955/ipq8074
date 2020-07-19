/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "FlashControlHandler.h"
#include "Utils.h"
#include "Host.h"
#include "ShellCommandExecutor.h"
#include "FileSystemOsAbstraction.h"
#include "DeviceManager.h"

#include <cstdio>
#include <cstring>
#include "DebugLogger.h"

using namespace std;

const char FlashControlCommandsHandler::LIST_DELIMITER = ',';
const char FlashControlCommandsHandler::DEVICE_RESULT_STRUCT_OPENER = '{';
const char FlashControlCommandsHandler::DEVICE_RESULT_STRUCT_CLOSER = '}';
const char* const FlashControlCommandsHandler::UNKNOWN_DEVICE = "unknown_device";
const char* const FlashControlCommandsHandler::SUCCESS = "SUCCESS";
const char* const FlashControlCommandsHandler::FULL_BURN = "full_burn";
const char* const FlashControlCommandsHandler::READ_IDS = "read_ids";
const char* const FlashControlCommandsHandler::SET_ID = "set_id";
const char* const FlashControlCommandsHandler::BURN_IDS_SECTION = "burn_ids_section";
const char* const FlashControlCommandsHandler::BURN_BOOTLOADER = "burn_bootloader";
const char* const FlashControlCommandsHandler::ERASE_FLASH = "erase_flash";
const char* const FlashControlCommandsHandler::HELP = "help";
const char* const FlashControlCommandsHandler::VERSION_NUMBER = "1.0";

bool FlashControlCommandsHandler::GetDeviceNameInWiburnFormat(const string& deviceName, string& deviceNameInWiburnFormat) const
{
    // in the old access layer, names were fixed per operating system
#ifdef _WINDOWS
    (void)deviceName;
    deviceNameInWiburnFormat = "wmp0l";
    return true;
#else
    string interfaceName;
    if (Host::GetHost().GetDeviceManager().GetDeviceInterfaceName(deviceName, interfaceName))
    {
        deviceNameInWiburnFormat = "wEP0L!SPARROW!" + interfaceName;
        return true;
    }

    // should not happen
    deviceNameInWiburnFormat = UNKNOWN_DEVICE;
    return false;
#endif
}

bool FlashControlCommandsHandler::GetDeviceBasebandTypeInWiburnFormat(const string& deviceName, string& type) const
{
    BasebandType deviceType = BASEBAND_TYPE_NONE;
    bool res = Host::GetHost().GetDeviceManager().GetDeviceBasebandType(deviceName, deviceType);
    if (res)
    {
        if (BASEBAND_TYPE_SPARROW == deviceType)
        {
            type = "SPARROW";
            return true;
        }
        type = "unsupported baseband. Currently only Sparrow is supported";
        return false;
    }

    // should not happen
    type = "unknow device";
    return false;
}

bool FlashControlCommandsHandler::CheckArgumentValidity(const std::vector<std::string> arguments,
    unsigned numberOfExpectedArguments, std::string& statusMessage) const
{
    if (arguments.size() != numberOfExpectedArguments)
    {
        stringstream ss;
        ss << "Expected " << numberOfExpectedArguments << " parameters for the flash control command but got " << arguments.size() << endl;
        statusMessage = ss.str();
        return false;
    }
    return true;
}

bool FlashControlCommandsHandler::HandleTcpCommand(const string& deviceList, const string& operation,
    const string& parameterList, string& statusMessage) const
{
    auto it = m_handlers.find(operation);
    if (m_handlers.end() == it)
    {
        statusMessage = "No handle exists for command " + operation + ". Use help command for more information.";
        return false;
    }

    return it->second(*this, Utils::Split(deviceList, LIST_DELIMITER), Utils::Split(parameterList, LIST_DELIMITER), statusMessage);
}

bool FlashControlCommandsHandler::CommandExecutionHelper(const vector<string>& devices, const string& commandToRun, string& statusMessage) const
{
    stringstream result;
    bool resultStatus = true;
    bool firstDevice = true;
    for (auto& device : devices)
    {
        if (!firstDevice)
        {
            result << ",";
        }
        else
        {
            firstDevice = false;
        }

        // get device name in wiburn format
        string deviceName;
        if (!GetDeviceNameInWiburnFormat(device, deviceName))
        {
            result << DEVICE_RESULT_STRUCT_OPENER << device << ",failed to get device name in wiburn format: " << deviceName << DEVICE_RESULT_STRUCT_CLOSER;
            resultStatus = false;
            continue;
        }

        // get baseband type in wiburn format
        string basebandType;
        if (!GetDeviceBasebandTypeInWiburnFormat(device, basebandType))
        {
            result << DEVICE_RESULT_STRUCT_OPENER << device << ",failed to get baseband type: " << basebandType << DEVICE_RESULT_STRUCT_CLOSER;
            resultStatus = false;
            continue;
        }

        // execute command
        stringstream ss;
        ss << WIBURN_PROGRAM_NAME << " -force -ignore_lock -interface " << deviceName << " -device " << basebandType << " " << commandToRun;
        ShellCommandExecutor command(ss.str().c_str());

        // process result
        if (0 == command.ExitStatus())
        {
            LOG_INFO << command.ToString() << std::endl;
            result << DEVICE_RESULT_STRUCT_OPENER << device << "," << SUCCESS << DEVICE_RESULT_STRUCT_CLOSER;
        }
        else
        {
            result << DEVICE_RESULT_STRUCT_OPENER << device << "," << FormatErrorMessage(device, command.ToString()) << DEVICE_RESULT_STRUCT_CLOSER;
            resultStatus = false;
        }
    }

    if (firstDevice) // meaning we didn't enter the loop
    {
        result << "no device was given";
    }

    statusMessage = result.str();
    return resultStatus;
}

bool FlashControlCommandsHandler::HandleFullBurnCommand(const vector<string>& devices, const vector<string>& parameters, string& statusMessage) const
{
    LOG_DEBUG << "Execute full burn command on device " << Utils::Concatenate(devices, ",") << " with BL file, production file, ids file: "
        << Utils::Concatenate(parameters, ",") << endl;

    // 3 parameters: BL file, production file, ids file
    if (!CheckArgumentValidity(parameters, 3, statusMessage))
    {
        return false;
    }

    stringstream command;
    command << "-erase -burn " << " -fw " << parameters[0] << " -production " << parameters[1] << " -ids " << parameters[2];
    return CommandExecutionHelper(devices, command.str(), statusMessage);
}

bool FlashControlCommandsHandler::HandleReadIdsCommand(const std::vector<std::string>& devices,
    const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    LOG_DEBUG << "Execute read ids command on device " << Utils::Concatenate(devices, ",") << " with ids file: " << parameters[0] << endl;

    // 1 parameter: ids file
    if (!CheckArgumentValidity(parameters, 1, statusMessage))
    {
        return false;
    }

    stringstream command;
    command << "-read_ids_to_file -ids " << parameters[0];
    return CommandExecutionHelper(devices, command.str(), statusMessage);
}

bool FlashControlCommandsHandler::HandleSetIdsSectionCommand(const std::vector<std::string>& devices,
    const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    LOG_DEBUG << "Execute burn ids section command on device " << Utils::Concatenate(devices, ",") << " with ids file: " << Utils::Concatenate(parameters,",") << endl;

    // 1 parameter: ids file
    if (!CheckArgumentValidity(parameters, 1, statusMessage))
    {
        return false;
    }

    stringstream command;
    command << "-burn_ids_section -ids " << parameters[0];
    return CommandExecutionHelper(devices, command.str(), statusMessage);
}

bool FlashControlCommandsHandler::HandleSetIdCommand(const std::vector<std::string>& devices,
    const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    LOG_DEBUG << "Execute set id command on device " << Utils::Concatenate(devices, ",") << " with (parameter, value): "
        << Utils::Concatenate(parameters, ",") << endl;

    // 2 parameter: parameter name, value
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    stringstream command;
    command << "-burn_id -id_name " << parameters[0] << " -id_value " << parameters[1];
    return CommandExecutionHelper(devices, command.str(), statusMessage);
}

bool FlashControlCommandsHandler::HandleBurnBootloaderCommand(const std::vector<std::string>& devices,
    const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    LOG_DEBUG << "Execute burn bootloader on device " << Utils::Concatenate(devices, ",")
        << " with (bootloader file, production file): " << Utils::Concatenate(parameters, ",") << endl;

    // 2 parameter: BL file, production fil
    if (!CheckArgumentValidity(parameters, 2, statusMessage))
    {
        return false;
    }

    stringstream result;
    bool resultStatus = true;
    bool firstDevice = true;

    for (auto& device : devices)
    {
        // prepare temp ids file
        string idsFullFileName = FileSystemOsAbstraction::GetOutputFilesLocation() + "ids_" + Utils::GetCurrentLocalTimeForFileName() + ".ini";

        if (!firstDevice)
        {
            result << ",";
        }
        else
        {
            firstDevice = false;
        }

        // get device name in wiburn format
        string deviceName;
        if (!GetDeviceNameInWiburnFormat(device, deviceName))
        {
            result << DEVICE_RESULT_STRUCT_OPENER << device << ",failed to get device name in wiburn format: " << deviceName << DEVICE_RESULT_STRUCT_CLOSER;
            resultStatus = false;
            continue;
        }

        // get baseband type in wiburn format
        string basebandType;
        if (!GetDeviceBasebandTypeInWiburnFormat(device, basebandType))
        {
            result << DEVICE_RESULT_STRUCT_OPENER << device << ",failed to get baseband type: " << basebandType << DEVICE_RESULT_STRUCT_CLOSER;
            resultStatus = false;
            continue;
        }

        bool succeedToRead = ReadIdsToFile(deviceName, basebandType, idsFullFileName, statusMessage);
        result << DEVICE_RESULT_STRUCT_OPENER << device << "," << statusMessage << DEVICE_RESULT_STRUCT_CLOSER;
        if (!succeedToRead)
        {
            resultStatus = false;
            continue;
        }

        bool succeedToBurn = PerformFullBurn(deviceName, basebandType, parameters[0], parameters[1], idsFullFileName, statusMessage);
        result << DEVICE_RESULT_STRUCT_OPENER << device << "," << statusMessage << DEVICE_RESULT_STRUCT_CLOSER;
        if (!succeedToBurn)
        {
            resultStatus = false;
            continue;
        }

        // delete temporary ids file in case of successful operation
        if (std::remove(idsFullFileName.c_str()))
        {
            LOG_WARNING << " failed to remove temporary ids file " << idsFullFileName << " error: " << std::strerror(errno) << endl;
        }
    }

    if (firstDevice) // meaning we didn't enter the loop
    {
        result << "no device was given";
    }

    statusMessage = result.str();
    return resultStatus;
}

bool FlashControlCommandsHandler::HandleEraseFlashCommand(const vector<string>& devices, const vector<string>& parameters, string& statusMessage) const
{
    LOG_DEBUG << "Execute erase flash command on device " << Utils::Concatenate(devices, ",") << endl;

    if (!CheckArgumentValidity(parameters, 0, statusMessage))
    {
        return false;
    }

    return CommandExecutionHelper(devices, "-erase", statusMessage);
}

bool FlashControlCommandsHandler::HandleHelpCommand(const std::vector<std::string>& devices, const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // no parameters nor devices checks - we want help text to be displayed in all situations
    (void) devices;
    (void) parameters;

    const static string flashCommand("flash_control|");
    const static string deviceList("|<device list>");

    stringstream command;
    command << "Flash control version : " << VERSION_NUMBER << endl
        << "Burn Full flash       : " << flashCommand << FULL_BURN << deviceList << "|<BL file>,<production file>,<ids file>" << endl
        << "Burn Bootloader       : " << flashCommand << BURN_BOOTLOADER << deviceList << "|<BL file, production file>" << endl
        << "Burn ids section      : " << flashCommand << BURN_IDS_SECTION << deviceList << "|<ids file>" << endl
        << "Update id             : " << flashCommand << SET_ID << deviceList << "|<id>,<value> (no support in updating ssid)" << endl
        << "Read ids to file      : " << flashCommand << READ_IDS << deviceList << "|<ids file to be written>" << endl
        << "Erase flash           : " << flashCommand << ERASE_FLASH << deviceList << endl;
    statusMessage = command.str();
    return true;
}

bool FlashControlCommandsHandler::ReadIdsToFile(const std::string& deviceName, const std::string& deviceType,
    const std::string& idsFileName, std::string& statusMessage) const
{
    LOG_DEBUG << "Read ids section of device " << deviceName << " with ids file: " << idsFileName << endl;
    stringstream ss;
    ss << WIBURN_PROGRAM_NAME << " -force -ignore_lock -interface " << deviceName << " -device " << deviceType
        << " -read_ids_to_file -ids " << idsFileName;
    ShellCommandExecutor command(ss.str().c_str());

    if (0 == command.ExitStatus())
    {
        LOG_INFO << command.ToString() << std::endl;
        statusMessage = SUCCESS;
        return true;
    }
    else
    {
        statusMessage = FormatErrorMessage(deviceName, command.ToString());
        return false;
    }
}


bool FlashControlCommandsHandler::PerformFullBurn(const string& deviceName, const string& deviceType,
    const string& bootloaderFile, const string& productionFile, const string& idsFileName, string& statusMessage) const
{
    LOG_DEBUG << "Perform full burn of device " << deviceName << " with (BL, profuction file, ids file): "
        << bootloaderFile << "," << productionFile << "," << idsFileName << endl;

    stringstream ss;
    ss << WIBURN_PROGRAM_NAME << " -force -ignore_lock -interface " << deviceName << " -device " << deviceType
        << " -erase -burn -fw " << bootloaderFile << " -production " << productionFile << " -ids " << idsFileName;
    ShellCommandExecutor command(ss.str().c_str());

    if (0 == command.ExitStatus())
    {
        LOG_INFO << command.ToString() << std::endl;
        statusMessage = SUCCESS;
        return true;
    }
    else
    {
        statusMessage = FormatErrorMessage(deviceName, command.ToString());
        return false;
    }
}

std::string FlashControlCommandsHandler::FormatErrorMessage(const std::string& deviceName, const std::string& originalErrorMessage) const
{
    static const char* const LOG_FILE_PREFIX = "flash_control_";
    static const char* const LOG_FILE_EXTENSION = ".txt";
    static const char* const GENERAL_ERROR_MESSAGE = "Failed to perform operation. For more info please check: ";

    stringstream res;
    stringstream errorFileName;
    errorFileName << FileSystemOsAbstraction::GetOutputFilesLocation() << LOG_FILE_PREFIX << Utils::GetCurrentLocalTimeForFileName() << "_" << deviceName << LOG_FILE_EXTENSION;
    if (FileSystemOsAbstraction::WriteFile(errorFileName.str(), originalErrorMessage))
    {
        LOG_ERROR << "failed to write error for device " << deviceName << "to file " << errorFileName.str() << ". Errno: " << strerror(errno) << endl;
        LOG_ERROR << "error for device " << deviceName << " is: " << originalErrorMessage << endl;
        res << GENERAL_ERROR_MESSAGE << errorFileName.str();
    }
    else
    {
        res << GENERAL_ERROR_MESSAGE << "host manager 11ad output";
    }
    return res.str();
}
