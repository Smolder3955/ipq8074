/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <sstream>
#include <chrono>
#include <string>
#include <memory>
#include <fstream>

#include "LogCollectorDefinitions.h"
#include "DebugLogger.h"
#include "LogCollectorConfiguration.h"

using namespace std;

namespace log_collector
{
    // Configuration class defaults:
    const std::chrono::milliseconds LogCollectorConfiguration::DEFAULT_POLLING_INTERVAL_MS = std::chrono::milliseconds(50);
    const uint32_t LogCollectorConfiguration::DEFAULT_MAX_SINGLE_FILE_SIZE_MB = 10;
    const uint32_t LogCollectorConfiguration::DEFAULT_MAX_NUM_OF_LOG_FILES = 0;
    const std::string LogCollectorConfiguration::DEFAULT_OUTPUT_FILE_SUFFIX = "";

    // LogCollectorConfigurator consts:
    const std::string LogCollectorConfigurator::MODULE = "Module";

    const std::string LogCollectorConfigurator::configuration_parameter_value_delimiter = "=";
    const char LogCollectorConfigurator::configuration_delimiter_string = ',';
    const char LogCollectorConfigurator::new_line_delimiter = '\n';

    const std::string LogCollectorConfigurator::m_configFileNamePrefix = "log_config";
    const std::string LogCollectorConfigurator::m_configFileNameExtension = ".ini";

    const std::string LogCollectorConfigurator::EMPTY_STRING = "";
    const std::string LogCollectorConfigurator::EMPTY_STRING_PLACEHOLDER = "<empty>";

    LogCollectorConfigurator::LogCollectorConfigurator(string deviceName, CpuType type)
        : m_configuration(new LogCollectorConfiguration())
        , m_configurationFileFullPath(
            FileSystemOsAbstraction::GetConfigurationFilesLocation() + m_configFileNamePrefix + "_" + deviceName + "_" + CPU_TYPE_TO_STRING[type] + m_configFileNameExtension)
        , m_debugLogPrefix(GetDebugLogPrefix(deviceName, type))
    {
        ResetModuleVerbosityToDefault();
        // Load the configuration from the configuration file:
        Load();
    }

    std::string LogCollectorConfigurator::GetDebugLogPrefix(const std::string& deviceName, const CpuType& cpuType) const
    {
        if (CPU_TYPE_FW == cpuType)
        {
            return deviceName + " FW tracer: ";
        }
        else
        {
            return deviceName + " uCode tracer: ";
        }
    }

    void LogCollectorConfigurator::ResetModuleVerbosityToDefault()
    {
        LOG_DEBUG << m_debugLogPrefix << "Reset module verbosity values to default" << std::endl;
        for (auto& module : m_modulesVerbosity)
        {
            module.verbose_level_enable = 0;
            module.info_level_enable = 1;
            module.error_level_enable = 1;
            module.warn_level_enable = 1;
            module.reserved0 = 0;
        }
    }

    OperationStatus LogCollectorConfigurator::Save()
    {
        LOG_DEBUG << m_debugLogPrefix << "Save log configuration file" << endl;
        stringstream line;
        line << "// auto generated config file" << endl;
        if (!FileSystemOsAbstraction::WriteFile(m_configurationFileFullPath, line.str()))
        {
            LOG_ERROR << m_debugLogPrefix << "Failed to write config file" << endl;
            return OperationStatus(false, m_debugLogPrefix + "Failed to write config file");
        }

        line.str(std::string());
        line << POLLING_INTERVAL_MS << configuration_parameter_value_delimiter << m_configuration->GetPollingIntervalMs().count() << endl;
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());

        line.str(std::string());
        line << "// Set " << MAX_SINGLE_FILE_SIZE_MB << " to 0 for no limitation" << endl;
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());

        line.str(std::string());
        line << MAX_SINGLE_FILE_SIZE_MB << configuration_parameter_value_delimiter << m_configuration->GetMaxSingleFileSizeMb() << endl;
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());

        line.str(std::string());
        line << "// Set " << MAX_NUM_OF_LOG_FILES << " to 0 for no limitation" << endl;
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());

        line.str(std::string());
        line << MAX_NUM_OF_LOG_FILES << configuration_parameter_value_delimiter << m_configuration->GetMaxNumOfLogFiles() << endl;
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());

        line.str(std::string());
        line << OUTPUT_FILE_SUFFIX << configuration_parameter_value_delimiter << m_configuration->GetOutputFileSuffix() << endl;
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());

        // Saving conversion file location
        line.str(std::string());
        line << CONVERSION_FILE_PATH << configuration_parameter_value_delimiter << m_configuration->GetConversionFilePath() << endl;
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());

        line.str(std::string());
        line << LOG_FILES_DIRECTORY << configuration_parameter_value_delimiter << m_configuration->GetLogFilesDirectoryPath() << endl;
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());

        line.str(std::string());
        line << endl << "// Log Module Levels" << endl << "// V - Verbose, I - Info, E - Error, W - Warning" << endl << "// For example: Module0=VIE" << endl <<
            "// Or: Module7=WIV" << endl << GetAllModulesVerbosityString(true);
        FileSystemOsAbstraction::AppendToFile(m_configurationFileFullPath, line.str());
        LOG_DEBUG << m_debugLogPrefix << "Log configuration file was saved" << endl;

        return OperationStatus(true);
    }

    bool LogCollectorConfigurator::ConvertModuleVerbosityStringToStruct(const string& verbosityString, module_level_info& verbosityStruct)
    {
        module_level_info verbosityStructInternal;
        for (const auto& c : verbosityString)
        {
            switch (c)
            {
            case 'V':
                verbosityStructInternal.verbose_level_enable = 1;
                break;
            case 'I':
                verbosityStructInternal.info_level_enable = 1;
                break;
            case 'E':
                verbosityStructInternal.error_level_enable = 1;
                break;
            case 'W':
                verbosityStructInternal.warn_level_enable = 1;
                break;
            default:
                return false;
            }
        }
        verbosityStruct = verbosityStructInternal;
        return true;
    }

    const string LogCollectorConfigurator::ConvertModuleVerbosityStructToString(const module_level_info& verbosityStruct) const
    {
        stringstream verbosityStringStream;

        if (verbosityStruct.verbose_level_enable)
        {
            verbosityStringStream << "V";
        }
        if (verbosityStruct.info_level_enable)
        {
            verbosityStringStream << "I";
        }
        if (verbosityStruct.error_level_enable)
        {
            verbosityStringStream << "E";
        }
        if (verbosityStruct.warn_level_enable)
        {
            verbosityStringStream << "W";
        }

        return verbosityStringStream.str();
    }

    // return success in case there were no conversions error
    OperationStatus LogCollectorConfigurator::SetParamerter(const std::string& parameter, const std::string& value, bool& valueWasChanged)
    {
        LOG_DEBUG << m_debugLogPrefix << "Set parameter " << parameter << " to value " << value << std::endl;
        LogCollectorConfigurator::SetResult res = SameValue;
        valueWasChanged = false;
        if (POLLING_INTERVAL_MS == parameter)
        {
            res = SetPollingIntervalMs(value);
        }
        else if (OUTPUT_FILE_SUFFIX == parameter)
        {
            res = SetOutputFileSuffix(value);
        }
        else if (MODULES_VERBOSITY_LEVELS == parameter)
        {
            res = SetMultipleModulesVerbosity(value, false);
        }
        else if (0 == parameter.find(LogCollectorConfigurator::MODULE))
        {
            res = SetModuleVerbosityInner(parameter, value);
        }
        else if (MAX_SINGLE_FILE_SIZE_MB == parameter)
        {
            res = SetMaxSingleFileSizeInMb(value);
        }
        else if (MAX_NUM_OF_LOG_FILES == parameter)
        {
            res = SetMaxNumOfLogFiles(value);
        }
        else if (CONVERSION_FILE_PATH == parameter)
        {
            res = SetConversionFilePath(value);
        }
        else if (LOG_FILES_DIRECTORY == parameter)
        {
            res = SetLogFilesDirectoryPath(value);
        }
        else
        {
            LOG_WARNING << m_debugLogPrefix << "No such configuration parameter: " << parameter << endl;
            return OperationStatus(false, m_debugLogPrefix + "No such configuration parameter: " + parameter);
        }

        if (InvalidParameter == res)
        {
            LOG_ERROR << m_debugLogPrefix << "Got invalid parameter: setting parameter " << parameter << " to value " << value << " has failed" << endl;
            return OperationStatus(false, m_debugLogPrefix + "Got invalid parameter: setting parameter " + parameter + " to value " + value + " has failed");
        }
        else if (InvalidValue == res)
        {
            LOG_ERROR << m_debugLogPrefix << "Got invalid value: setting parameter " << parameter << " to value " << value << " has failed" << endl;
            return OperationStatus(false, m_debugLogPrefix + "Got invalid value: setting parameter " + parameter + " to value " + value + " has failed");
        }
        else if (ValueWasChanged == res)
        {
            Save(); // TODO - can be optimaized: 1. enable multiple configuration settings via TCP
            valueWasChanged = true;
        }

        return OperationStatus(true);
    }

    bool LogCollectorConfigurator::TryConvertStringToInteger(const string& str, int& i)
    {
        i = atoi(str.c_str());
        if (0 == i && str != "0")
        {
            return false;
        }
        return true;
    }

    LogCollectorConfigurator::SetResult LogCollectorConfigurator::SetPollingIntervalMs(const string& interval)
    {
        int intInterval;
        if (!TryConvertStringToInteger(interval, intInterval))
        {
            return InvalidValue;
        }
        m_configuration->SetPollingIntervalMs(intInterval);
        return ValueWasChanged;
    }

    string LogCollectorConfigurator::GetAllModulesVerbosityString(bool forSavingTofile) const
    {
        stringstream ss;

        for (int i = 0; i < NUM_MODULES; i++)
        {
            ss << module_names[i] << configuration_parameter_value_delimiter <<
                ConvertModuleVerbosityStructToString(m_modulesVerbosity[i]) << GetConfigurationsDelimiter(forSavingTofile);
        }

        string str = ss.str();
        if(forSavingTofile)
        {
            return str;
        }
        return str.erase(str.size() - 1, 1); // delete last delimiter character
    }

    string LogCollectorConfigurator::GetModulesVerbosityString(const string& module) const
    {
        for (int i = 0; i < NUM_MODULES; i++)
        {
            if (module_names[i] == module)
            {
                return ConvertModuleVerbosityStructToString(m_modulesVerbosity[i]);
            }
        }
        return ""; // Shouldn't get here
    }

    LogCollectorConfigurator::SetResult LogCollectorConfigurator::SetMultipleModulesVerbosity(const string& modulesVerbosity, bool fromFile)
    {
        SetResult multipleRes = SameValue;
        // split in case there are multiple modules in the same line, i.e: SYSTEM=VIEW,DRIVER=VIEW
        map<string, string> moduleToLevelMap;
        stringstream ss(modulesVerbosity);
        string moduleLevel;
        while (std::getline(ss, moduleLevel, GetConfigurationsDelimiter(fromFile)))
        {
            size_t delimeterPos = moduleLevel.find('=');
            if (delimeterPos != string::npos && (moduleLevel.length() > delimeterPos)) // ">" and not ">=" for skipping '='
            {
                moduleToLevelMap.insert(make_pair(moduleLevel.substr(0, delimeterPos), moduleLevel.substr(delimeterPos + 1)));
            }
        }

        for (auto& moduleToLevel : moduleToLevelMap)
        {
            SetResult res = SetModuleVerbosityInner(moduleToLevel.first, moduleToLevel.second);
            if (InvalidParameter == res) // TODO: notify the user about the error and maybe revert module verbosities
            {
                LOG_ERROR << m_debugLogPrefix << "setting module verbosity failed due to invalid parameter. parameter is " << moduleToLevel.first << ", value is " << moduleToLevel.second << endl;
                if (SameValue == multipleRes)
                {
                    multipleRes = InvalidParameter;
                }
            }
            else if (InvalidValue == res) // TODO: notify the user about the error and maybe revert module verbosities
            {
                LOG_ERROR << m_debugLogPrefix << "setting module verbosity failed due to invalid value. parameter is " << moduleToLevel.first << ", value is " << moduleToLevel.second << endl;
                if (SameValue == multipleRes)
                {
                    multipleRes = InvalidValue;
                }
            }
            else if (ValueWasChanged == res)
            {
                multipleRes = ValueWasChanged;
            }
        }
        return multipleRes;
    }

    LogCollectorConfigurator::SetResult LogCollectorConfigurator::SetModuleVerbosityInner(const string& module, const string& level)
    {
        for (int i = 0; i < NUM_MODULES; i++)
        {
            if (module_names[i] == module)
            {
                module_level_info newValue;
                if (!ConvertModuleVerbosityStringToStruct(level, newValue))
                {
                    return InvalidValue;
                }
                if (newValue != m_modulesVerbosity[i])
                {
                    m_modulesVerbosity[i] = newValue;
                    return ValueWasChanged;
                }
                return SameValue;
            }
        }
        LOG_ERROR << "Failed to set verbosity " << level << " to module " << module << endl;
        return InvalidParameter; //TODO: notify the user + make sure DmTools asks for all module verbosities in case tracer window is opened
    }

    OperationStatus LogCollectorConfigurator::SetModuleVerbosity(const std::string& module, const std::string& level)
    {
        SetResult res = SetModuleVerbosityInner(module, level);
        switch (res)
        {
        case ValueWasChanged:
            Save();
            return OperationStatus(true);
        case SameValue:
            return OperationStatus(true);
        case InvalidValue:
            return OperationStatus(false, "Invalid value for module: " + module);
        case InvalidParameter:
            return OperationStatus(false, "Invalid parameter (module was not found)");
        }
        return OperationStatus(false); // Shouldn't get here
    }

    LogCollectorConfigurator::SetResult LogCollectorConfigurator::SetMaxSingleFileSizeInMb(const string& size)
    {
        int intSize;
        if (!TryConvertStringToInteger(size, intSize))
        {
            return InvalidValue;
        }
        m_configuration->SetMaxSingleFileSizeMb(intSize);
        return ValueWasChanged;
    }

    LogCollectorConfigurator::SetResult LogCollectorConfigurator::SetMaxNumOfLogFiles(const string& size)
    {
        int intSize;
        if (!TryConvertStringToInteger(size, intSize))
        {
            return InvalidValue;
        }
        if (intSize < 0)
        {
            LOG_ERROR << "Failed to set max number of log files to " << size << ". Minimal value is 0" << endl;
            return InvalidValue;
        }
        m_configuration->SetMaxNumOfLogFiles(intSize);
        return ValueWasChanged;
    }

    LogCollectorConfigurator::SetResult LogCollectorConfigurator::SetConversionFilePath(const std::string & conversionFilesPath)
    {
        // TODO: add here check if the folder exist and raise warning but still set the config
        m_configuration->SetConversionFilePath(conversionFilesPath);

        return ValueWasChanged;
    }

    LogCollectorConfigurator::SetResult LogCollectorConfigurator::SetLogFilesDirectoryPath(const std::string & logFilesDirectoryPath)
    {
        // TODO: add here check if the folder exist and raise warning but still set the config
        m_configuration->SetLogFilesDirectoryPath(logFilesDirectoryPath);
        return ValueWasChanged;
    }

    OperationStatus LogCollectorConfigurator::GetParameter(const string& parameter, string& value) const
    {
        if (POLLING_INTERVAL_MS == parameter)
        {
            stringstream ss;
            ss << m_configuration->GetPollingIntervalMs().count();
            value = ss.str();
        }
        else if (OUTPUT_FILE_SUFFIX == parameter)
        {
            value = m_configuration->GetOutputFileSuffix();
            if (EMPTY_STRING == value)
            {
                value = EMPTY_STRING_PLACEHOLDER;
            }
        }
        else if (MODULES_VERBOSITY_LEVELS == parameter)
        {
            value = GetAllModulesVerbosityString();
        }
        else if (MAX_SINGLE_FILE_SIZE_MB == parameter)
        {
            stringstream ss;
            ss << m_configuration->GetMaxSingleFileSizeMb();
            value = ss.str();
        }
        else if (MAX_NUM_OF_LOG_FILES == parameter)
        {
            stringstream ss;
            ss << m_configuration->GetMaxNumOfLogFiles();
            value = ss.str();
        }
        else
        {
            value = "No such parameter";
            return OperationStatus(false, "No such parameter");
        }
        return OperationStatus(true);
    }

    string LogCollectorConfigurator::ToString()
    {
        stringstream ss;
        ss << GetParameter(POLLING_INTERVAL_MS) << GetConfigurationsDelimiter(false) <<
            GetParameter(MAX_SINGLE_FILE_SIZE_MB) << GetConfigurationsDelimiter(false) <<
            GetParameter(MAX_NUM_OF_LOG_FILES) << GetConfigurationsDelimiter(false) <<
            GetParameter(OUTPUT_FILE_SUFFIX) << GetConfigurationsDelimiter(false) <<
            GetParameter(MODULES_VERBOSITY_LEVELS) << endl;
        return ss.str();
    }

    OperationStatus LogCollectorConfigurator::Load()
    {
        // check existence of file
        if (!FileSystemOsAbstraction::DoesFileExist(m_configurationFileFullPath)) // later - check user location
        {
            LOG_INFO << m_debugLogPrefix << "No log collector configuration file, creating a default configuration file in " << m_configurationFileFullPath << endl;
            Save();
            return OperationStatus(false, m_debugLogPrefix + "No log collector configuration file, creating a default configuration file in " + m_configurationFileFullPath);
        }

        // read file line by line
        ifstream fd(m_configurationFileFullPath.c_str());
        if (!fd)
        {
            LOG_ERROR << m_debugLogPrefix << "Failed to open log configuration file " << m_configurationFileFullPath << endl;
            OperationStatus(false, m_debugLogPrefix + "Failed to open log configuration file " + m_configurationFileFullPath);
        }

        string line;
        bool valueWasChangedDummy;
        while (std::getline(fd, line))
        {
            // Skip comments
            if (line.find("//") == 0)
            {
                continue;
            }

            // handle windows endline character
            if (line.length() > 0 && '\r' == line[line.length() - 1])
            {
                line[line.length() - 1] = '\0';
            }

            if (!SetParameterAssignment(line, valueWasChangedDummy, true))
            {
                LOG_ERROR << m_debugLogPrefix << "Failed to parse configuration file " << m_configurationFileFullPath << ", problematic line: " << line << ". Use the default configuration" << endl;
            }
        }

        if (fd.bad())
        {
            Reset();
            LOG_ERROR << m_debugLogPrefix << "Got error while trying to open configuration file " << m_configurationFileFullPath << endl;
            return OperationStatus(false, m_debugLogPrefix + "Got error while trying to open configuration file " + m_configurationFileFullPath);
        }

        LOG_DEBUG << m_debugLogPrefix << "Log collector configuration file was taken from " << m_configurationFileFullPath << endl;
        return OperationStatus(true);
    }

    bool LogCollectorConfigurator::SetParameterAssignment(string assignment, bool& valueWasChanged, bool acceptNonAssignmentStrings)
    {
        std::size_t found = assignment.find(configuration_parameter_value_delimiter);
        if (std::string::npos != found)
        {
            return SetParamerter(assignment.substr(0, found), assignment.substr(found + 1), valueWasChanged);
        }
        return acceptNonAssignmentStrings? true : false; // skipping this string (for file emtpy lines/ comment out lines)
    }
}
