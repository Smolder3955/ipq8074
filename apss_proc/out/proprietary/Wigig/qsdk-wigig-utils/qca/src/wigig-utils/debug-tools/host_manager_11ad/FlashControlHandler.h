/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _FLASH_CONTROL_HANDLER_H_
#define _FLASH_CONTROL_HANDLER_H_
#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

class FlashControlCommandsHandler final
{
public:

    // singleton
    static FlashControlCommandsHandler& GetFlashControlCommandsHandler()
    {
        static FlashControlCommandsHandler flashControlHandler;
        return flashControlHandler;
    }

    bool HandleTcpCommand(const std::string& deviceList, const std::string& operation, const std::string& parameterList, std::string& statusMessage) const;

    // Non-copyable
    FlashControlCommandsHandler(const FlashControlCommandsHandler&) = delete;
    FlashControlCommandsHandler& operator=(const FlashControlCommandsHandler&) = delete;

private:
    // typedefs
    typedef std::function<bool(const FlashControlCommandsHandler&, const std::vector<std::string>& devices, const std::vector<std::string>& parameters,
        std::string& statusMessage)> t_handlerFunction;

    // consts
    static const char LIST_DELIMITER;
    static const char DEVICE_RESULT_STRUCT_OPENER;
    static const char DEVICE_RESULT_STRUCT_CLOSER;
    static const char* const UNKNOWN_DEVICE;
    static const char* const SUCCESS;
    static const char* const FULL_BURN;
    static const char* const READ_IDS;
    static const char* const SET_ID;
    static const char* const BURN_IDS_SECTION;
    static const char* const BURN_BOOTLOADER;
    static const char* const ERASE_FLASH;
    static const char* const HELP;
    static const char* const VERSION_NUMBER;

#ifdef _WINDOWS
    // todo - in addition, read program name from config file if exists
    std::string WIBURN_PROGRAM_NAME = "wiburn.exe";
#else
    std::string WIBURN_PROGRAM_NAME = "./wigig_wiburn";
#endif

    //handlers
    bool HandleFullBurnCommand(const std::vector<std::string>& devices, const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleReadIdsCommand(const std::vector<std::string>& devices, const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleSetIdsSectionCommand(const std::vector<std::string>& devices, const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleSetIdCommand(const std::vector<std::string>& devices, const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleBurnBootloaderCommand(const std::vector<std::string>& devices, const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleEraseFlashCommand(const std::vector<std::string>& devices, const std::vector<std::string>& parameters, std::string& statusMessage) const;
    bool HandleHelpCommand(const std::vector<std::string>& devices, const std::vector<std::string>& parameters, std::string& statusMessage) const;

    // wiburn operations
    bool CommandExecutionHelper(const std::vector<std::string>& devices, const std::string& command, std::string& statusMessage) const;
    bool ReadIdsToFile(const std::string& deviceName, const std::string& deviceType, const std::string& idsFileName, std::string& statusMessage) const;
    bool PerformFullBurn(const std::string& deviceName, const std::string& deviceType, const std::string& bootloaderFile, const std::string& productionFile,
                         const std::string& idsFileName, std::string& statusMessage) const;

    const std::unordered_map<std::string, t_handlerFunction> m_handlers = { {FULL_BURN, &FlashControlCommandsHandler::HandleFullBurnCommand },
                                                                      {READ_IDS, &FlashControlCommandsHandler::HandleReadIdsCommand },
                                                                      {SET_ID, &FlashControlCommandsHandler::HandleSetIdCommand },
                                                                      {BURN_IDS_SECTION, &FlashControlCommandsHandler::HandleSetIdsSectionCommand },
                                                                      {BURN_BOOTLOADER, &FlashControlCommandsHandler::HandleBurnBootloaderCommand },
                                                                      {ERASE_FLASH, &FlashControlCommandsHandler::HandleEraseFlashCommand },
                                                                      {HELP, &FlashControlCommandsHandler::HandleHelpCommand } };

    FlashControlCommandsHandler() {};
    bool GetDeviceNameInWiburnFormat(const std::string& deviceName, std::string& deviceNameInWiburnFormat) const;
    bool GetDeviceBasebandTypeInWiburnFormat(const std::string& deviceName, std::string& type) const;
    bool CheckArgumentValidity(const std::vector<std::string> arguments, unsigned numberOfExpectedArguments, std::string& statusMessage) const;
    std::string FormatErrorMessage(const std::string& deviceName, const std::string& originalErrorMessage) const;
};

#endif // !_FLASH_CONTROL_HANDLER_H_

