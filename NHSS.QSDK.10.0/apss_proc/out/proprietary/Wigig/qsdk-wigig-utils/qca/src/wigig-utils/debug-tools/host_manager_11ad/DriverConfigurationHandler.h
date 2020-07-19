/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef  _DRIVER_CONFIGURATION_H_
#define _DRIVER_CONFIGURATION_H_
#pragma once

#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

class DriverConfigurationHandler final
{
public:

    // singleton
    static DriverConfigurationHandler& GetDriverConfigurationHandler()
    {
        static DriverConfigurationHandler driverConfigurationHandler;
        return driverConfigurationHandler;
    }

    bool HandleTcpCommand(const std::string& operation, const std::string& parameterList, std::string& statusMessage) const;

    // Non-copyable
    DriverConfigurationHandler(const DriverConfigurationHandler&) = delete;
    DriverConfigurationHandler& operator=(const DriverConfigurationHandler&) = delete;

private:
    // typedefs
    typedef std::function<bool(const DriverConfigurationHandler&, const std::vector<std::string>& parameters,
        std::string& statusMessage)> t_handlerFunction;

    // consts
    const char LIST_DELIMITER = ',';
    const char* const VERSION_NUMBER;
    const char* const LOAD_DRIVER;
    const char* const UNLOAD_DRIVER;
    const char* const LOAD_DRIVER_SHELL_COMMAND;
    const char* const UNLOAD_DRIVER_SHELL_COMMAND;
    const char* const OPERATIONAL_MODE;
    const char* const FTM_MODE;
    const char* const EMPTY_FLASH_MODE;
#ifdef _WINDOWS
    const char* const DEFAULT_MODE; // this mode is relevant only to windows
#endif
    const char* const HELP;

    // handlers
    bool HandleLoadDriverCommand(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleUnloadDriverCommand(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleHelpCommand(const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleWindowsDevcon(const char* operation, std::string& statusMessage) const;

    bool CommandExecutionHelper(const std::string& command, std::string& statusMessage) const;
    bool CommandExecutionHelper(const std::string& command, std::string& statusMessage, std::vector<std::string>& output) const;
    bool CheckArgumentValidity(const std::vector<std::string> arguments, unsigned numberOfExpectedArguments, std::string& statusMessage) const;
    const char* SetLoadDriverShellCommand() const;
    const char* SetUnloadDriverShellCommand() const;

    const std::unordered_map<std::string, t_handlerFunction> m_handlers =
    {
        {LOAD_DRIVER,   &DriverConfigurationHandler::HandleLoadDriverCommand   },
        {UNLOAD_DRIVER, &DriverConfigurationHandler::HandleUnloadDriverCommand },
        {HELP,          &DriverConfigurationHandler::HandleHelpCommand         }
    };

    const std::unordered_map<std::string, std::string> m_loadDriverParametersUserCommandToDriverCommand =
    {
#ifdef _WINDOWS
        { OPERATIONAL_MODE,     "/v UseFTM /t REG_SZ /d 1 /f"   },
        { FTM_MODE,             "/v UseFTM /t REG_SZ /d 2 /f"   },
        { EMPTY_FLASH_MODE,     "/v ResetMode /t REG_SZ /d 2 /f"},
        { DEFAULT_MODE,         "/v ResetMode /t REG_SZ /d 1 /f"}
#else //linux
        { OPERATIONAL_MODE,     "" /* TODO: need to consider the whole flow before enabling this command */},
        { FTM_MODE,             "ftm_mode" },
        { EMPTY_FLASH_MODE,     "debug_fw" }
#endif // _WINDOWS
    };

    DriverConfigurationHandler();
};
#endif // ! _DRIVER_CONFIGURATION_H_

