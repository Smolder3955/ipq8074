/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "DriverConfigurationHandler.h"
#include "Utils.h"
#include "ShellCommandExecutor.h"
#include "DebugLogger.h"

#include <sstream>
#ifdef _WINDOWS
    #include "Windows/WindowsConstants.h"
#endif

using namespace std;

DriverConfigurationHandler::DriverConfigurationHandler()
    : VERSION_NUMBER("1.0")
    , LOAD_DRIVER("load_driver")
    , UNLOAD_DRIVER("unload_driver")
    , LOAD_DRIVER_SHELL_COMMAND(SetLoadDriverShellCommand())
    , UNLOAD_DRIVER_SHELL_COMMAND(SetUnloadDriverShellCommand())
    , OPERATIONAL_MODE("operational_mode")
    , FTM_MODE("ftm_mode")
    , EMPTY_FLASH_MODE("empty_flash_mode")
#ifdef _WINDOWS
    , DEFAULT_MODE("default_reset_mode")
#endif // _WINDOWS

    , HELP("help")
{ }

const char* DriverConfigurationHandler::SetLoadDriverShellCommand() const
{
#ifdef _WINDOWS
    return "devcon enable";
#elif __ANDROID__
    //if (FileSystemOsAbstraction::DoesFileExist("/vendor/lib/modules/wil6210.ko")) // Napali
    //{
        return "insmod /vendor/lib/modules/wil6210.ko alt_ifname"; // Napali // TODO: need to decide whether we want to support older versions and if so, test on those platforms
    //}
    //else
    //{
    //    return "insmod /system/lib/modules/wil6210.ko"; // Nazgul
    //}
#else // linux and openwrt
    return "insmod /lib/modules/$(uname -r)/kernel/drivers/net/wireless/ath/wil6210/wil6210.ko";
#endif
}

const char* DriverConfigurationHandler::SetUnloadDriverShellCommand() const
{
#ifdef _WINDOWS
    return "devcon disable";
#else // All Linux based OS
    return "rmmod wil6210";
#endif
}

bool DriverConfigurationHandler::CheckArgumentValidity(const std::vector<std::string> arguments, unsigned numberOfExpectedArguments, std::string& statusMessage) const
{
    if (arguments.size() != numberOfExpectedArguments)
    {
        stringstream ss;
        ss << "Expected " << numberOfExpectedArguments << " parameters for the driver congifuration command but got " << arguments.size() << endl;
        statusMessage = ss.str();
        return false;
    }
    return true;
}

bool DriverConfigurationHandler::HandleTcpCommand(const string& operation, const string& parameterList, string& statusMessage) const
{
    auto it = m_handlers.find(operation);
    if (m_handlers.end() == it)
    {
        statusMessage = "No handle exists for command " + operation + ". Use help command for more information.";
        return false;
    }

    return it->second(*this, Utils::Split(parameterList, LIST_DELIMITER), statusMessage);
}

bool DriverConfigurationHandler::CommandExecutionHelper(const std::string& driverCommand, std::string& statusMessage) const
{
    std::vector<std::string> output;
    return CommandExecutionHelper(driverCommand, statusMessage, output);
}

bool DriverConfigurationHandler::CommandExecutionHelper(const std::string& driverCommand, std::string& statusMessage, std::vector<std::string>& output) const
{
    // execute command
    ShellCommandExecutor command(driverCommand.c_str());

    // process result
    if (0 == command.ExitStatus())
    {
        output = command.Output();
        LOG_INFO << command.ToString();
        return true;
    }
    else
    {
        statusMessage = command.ToString();
        return false;
    }
}

bool DriverConfigurationHandler::HandleWindowsDevcon(const char* operation, std::string& statusMessage) const
{
#ifdef _WINDOWS
    std::vector<std::string> output;
    // Using the devcon utility, find the path representing the driver in the devcon
    if (!CommandExecutionHelper("devcon findall =net | findstr \"" + std::string(WINDOWS_NETWORK_ADAPTER_NAME) + "\"", statusMessage, output))
    {
        statusMessage = "devcon.exe is not in the directory of host manager. Please follow the user guide.";
        return false;
    }

    // Check if there is a problem with the output from devcon
    if (output.front().find(WINDOWS_NETWORK_ADAPTER_NAME) == std::string::npos)
    {
        statusMessage = "Couldn't find the driver of " + std::string(WINDOWS_NETWORK_ADAPTER_NAME);
        return false;
    }
    // Take the Id (path) of the driver
    std::string fullDriverId = (Utils::Split(output.front(), ':')).front();
    std::vector<std::string> driverIdSplitted = Utils::Split(fullDriverId, '\\');
    if (driverIdSplitted.size() < 2)
    {
        statusMessage = "Illegal name has returned from devcon";
        return false;
    }
    std::string driverId = driverIdSplitted[0] + "\\" + driverIdSplitted[1];

    output = std::vector<std::string>();

    if (!CommandExecutionHelper(std::string(operation) + R"( "*)" + driverId + R"(")", statusMessage, output))
    {
        return false;
    }

    if (output.front().find("Enable failed") != std::string::npos)
    {
        statusMessage = "Couldn't enable the driver, please make sure to use the right version of devcon (that fits your windows version) and host manager is running as admin";
        return false;
    }

    if (output.front().find("No devices found") != std::string::npos)
    {
        statusMessage = "Couldn't find the driver.";
        return false;
    }
#else
    // irrelevant for other OS
    (void)operation;
    (void)statusMessage;
#endif
    return true;
}

bool DriverConfigurationHandler::HandleLoadDriverCommand(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    if (!CheckArgumentValidity(parameters, 1, statusMessage))
    {
        return false;
    }

    auto mode = m_loadDriverParametersUserCommandToDriverCommand.find(parameters[0]);
    if (m_loadDriverParametersUserCommandToDriverCommand.end() == mode)
    {
        statusMessage = "unknown mode " + parameters[0];
        return false;
    }
#ifdef _WINDOWS
    std::vector<std::string> output;

    // Get the path of the driver in the registery using RegistryPathFinder.exe in order to change values of the driver
    if (!CommandExecutionHelper("RegistryPathFinder.exe", statusMessage, output))
    {
        statusMessage = "Can't find RegistryPathFinder.exe, please put it in the same folder as host_manager_11ad.exe";
        return false;
    }

    if (output.front() == "No devices found")
    {
        statusMessage = "No devices found";
        return false;
    }

    // If this prefix is not part of the output - there is a problem
    if (!output.front().find(R"(HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Class\)", 0) == 0) //check if this string is prefix, if not, there is an error
    {
        statusMessage = "There was an expected error, can't find the driver";
        return false;
    }

    // Change the value of requested property
    if (!CommandExecutionHelper("Reg add " + output.front() + " " + mode->second, statusMessage))
    {
        return false;
    }

    return HandleWindowsDevcon(LOAD_DRIVER_SHELL_COMMAND, statusMessage);
#else
    stringstream command;
    command << LOAD_DRIVER_SHELL_COMMAND << " " << mode->second;
    return CommandExecutionHelper(command.str(), statusMessage);
#endif
}

bool DriverConfigurationHandler::HandleUnloadDriverCommand(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    if (!CheckArgumentValidity(parameters, 0, statusMessage))
    {
        return false;
    }

#ifdef _WINDOWS
    return HandleWindowsDevcon(UNLOAD_DRIVER_SHELL_COMMAND, statusMessage);
#else
    return CommandExecutionHelper(std::string(UNLOAD_DRIVER_SHELL_COMMAND), statusMessage);
#endif
}

bool DriverConfigurationHandler::HandleHelpCommand(const std::vector<std::string>& parameters, std::string& statusMessage) const
{
    // no parameters checks - we want help text to be displayed in all situations
    (void)parameters;

    const static string driverConfiguration("driver_configuration");
    const static string hostList("|<host list>|");

    stringstream command;
    command << "Driver configuration version : " << VERSION_NUMBER << endl
        << "Load driver       : " << driverConfiguration << hostList << LOAD_DRIVER << "|<driver mode>" << endl
        << "                    Where driver mode can be one of the following: " /*<< OPERATIONAL_MODE << LIST_DELIMITER*/ << FTM_MODE << LIST_DELIMITER << EMPTY_FLASH_MODE << endl
        << "Unload driver     : " << driverConfiguration << hostList << UNLOAD_DRIVER << endl;
    statusMessage = command.str();
    return true;
}