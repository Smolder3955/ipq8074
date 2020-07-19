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

#pragma once

#include "FileSystemOsAbstraction.h"
#include "OperationStatus.h"
#include "LogCollectorDefinitions.h"

#include <string>
#include <sstream>

namespace log_collector
{
    class LogCollectorConfiguration
    {
    public:
        LogCollectorConfiguration()
            : m_pollingIntervalMs(std::chrono::milliseconds(0))
            , m_maxSingleFileSizeMb(0)
            , m_maxNumOfLogFiles(0)
        {
            ResetToDefault();
        }

        LogCollectorConfiguration& operator=(const LogCollectorConfiguration&) = default;

        // getters
        const std::chrono::milliseconds& GetPollingIntervalMs() const { return m_pollingIntervalMs; }
        uint32_t GetMaxSingleFileSizeMb() const { return m_maxSingleFileSizeMb; }
        uint32_t GetMaxNumOfLogFiles() const { return m_maxNumOfLogFiles; }
        const std::string& GetOutputFileSuffix() const { return m_outputFileSuffix; }
        const std::string& GetConversionFilePath() const { return m_conversionFilePath; }
        const std::string& GetLogFilesDirectoryPath() const { return m_logFilesDirectoryPath; }

        // setters
        void SetPollingIntervalMs(uint32_t pollingIntervalMs) { m_pollingIntervalMs = std::chrono::milliseconds(pollingIntervalMs); }
        void SetMaxSingleFileSizeMb(uint32_t maxSingleFileSizeMb) { m_maxSingleFileSizeMb = maxSingleFileSizeMb; }
        void SetMaxNumOfLogFiles(uint32_t maxNumOfLogFiles) { m_maxNumOfLogFiles = maxNumOfLogFiles; }
        void SetOutputFileSuffix(const std::string& outputFileSuffix) { m_outputFileSuffix = outputFileSuffix; }
        void SetConversionFilePath(const std::string& conversionFilesPath) { m_conversionFilePath = conversionFilesPath; }
        void SetLogFilesDirectoryPath(const std::string& logFilesDirectoryPath) { m_logFilesDirectoryPath = logFilesDirectoryPath; }

        void ResetToDefault()
        {
            m_pollingIntervalMs = DEFAULT_POLLING_INTERVAL_MS;
            m_maxSingleFileSizeMb = DEFAULT_MAX_SINGLE_FILE_SIZE_MB;
            m_maxNumOfLogFiles = DEFAULT_MAX_NUM_OF_LOG_FILES;
            m_outputFileSuffix = DEFAULT_OUTPUT_FILE_SUFFIX;
            m_conversionFilePath = FileSystemOsAbstraction::GetDefaultLogConversionFilesLocation();
            m_logFilesDirectoryPath = FileSystemOsAbstraction::GetOutputFilesLocation() + "Logs" + FileSystemOsAbstraction::GetDirectoryDelimiter();
        }

    private:
        std::chrono::milliseconds m_pollingIntervalMs; // interval between two consecutive log polling in milliseconds
        uint32_t m_maxSingleFileSizeMb;
        uint32_t m_maxNumOfLogFiles;
        std::string m_outputFileSuffix;
        std::string m_conversionFilePath;
        std::string m_logFilesDirectoryPath;

        static const std::chrono::milliseconds DEFAULT_POLLING_INTERVAL_MS; // Default polling interval is set to 50 ms
        static const uint32_t DEFAULT_MAX_SINGLE_FILE_SIZE_MB; // Default max single file size is set to 50 MB
        static const uint32_t DEFAULT_MAX_NUM_OF_LOG_FILES; // Default max number of log files is set to 0 files
        static const std::string DEFAULT_OUTPUT_FILE_SUFFIX; // Default suffix for output log file is empty
    };

    class LogCollectorConfigurator
    {
    public:
        LogCollectorConfigurator(std::string deviceName, CpuType type);
        void Reset() { ResetModuleVerbosityToDefault(); m_configuration->ResetToDefault(); Save(); }
        OperationStatus Load();
        OperationStatus Save();

        /*** get functions ***/
        bool GetModuleVerbosity(int i, module_level_info& verbosity) const { // change verbosity only in case of valid operation!
            if (i < 0 || i >= NUM_MODULES) { return false; }
            else { verbosity = m_modulesVerbosity[i]; return true; }
        }
        std::string GetAllModulesVerbosityString(bool forSavingTofile = false) const;
        std::string GetModulesVerbosityString(const std::string& module) const;
        std::shared_ptr<LogCollectorConfiguration> m_configuration;

        /*** set functions ***/
        bool SetParameterAssignment(std::string line, bool& valueWasChanged, bool acceptNonAssignmentStrings = false);
        OperationStatus SetParamerter(const std::string& parameterName, const std::string& value, bool& valueWasChanged);
        OperationStatus SetModuleVerbosity(const std::string& module, const std::string& level);

        // TODO : For the regular tcp server. should be removed in the future:
        std::string ToString(); // TODO : For the regular tcp server. should be removed in the future
        OperationStatus GetParameter(const std::string& parameter, std::string& value) const; // TODO : For the regular tcp server. should be removed in the future
        std::string GetParameter(const std::string& parameter) const { std::stringstream ss;  std::string value; GetParameter(parameter, value); ss << parameter << configuration_parameter_value_delimiter << value; return ss.str(); } // TODO : For the regular tcp server. should be removed in the future

    private:
        void ResetModuleVerbosityToDefault();
        const char& GetConfigurationsDelimiter(bool fileFormat) const
        {
            if (fileFormat)
            {
                return new_line_delimiter;
            }
            else return configuration_delimiter_string;
        }

        /*** set functions ***/
        enum SetResult
        {
            InvalidParameter,
            InvalidValue,
            ValueWasChanged,
            SameValue
        };

        bool TryConvertStringToInteger(const std::string& str, int& i);

        // TODO : For the regular tcp server. should be removed in the future: (probably it is possible to delete all 5 below. it has to be checked carefully)
        SetResult SetPollingIntervalMs(const std::string& interval);
        SetResult SetOutputFileSuffix(const std::string& suffix) { m_configuration->SetOutputFileSuffix(suffix);  return ValueWasChanged; }
        SetResult SetModuleVerbosityInner(const std::string& module, const std::string& level);
        inline SetResult SetMaxSingleFileSizeInMb(const std::string& size);
        inline SetResult SetMaxNumOfLogFiles(const std::string& size);
        SetResult SetConversionFilePath(const std::string& conversionFilesPath);
        SetResult SetLogFilesDirectoryPath(const std::string& logFilesDirectoryPath);
        /*** module verbosity functions ***/
        bool ConvertModuleVerbosityStringToStruct(const std::string& verbosityString, module_level_info& verbosityStruct);
        const std::string ConvertModuleVerbosityStructToString(const module_level_info& verbosityStruct) const;
        SetResult SetMultipleModulesVerbosity(const std::string& modulesVerbosity, bool fromFile = false);

        module_level_info m_modulesVerbosity[NUM_MODULES];

        /*** other members ***/
        const std::string m_configurationFileFullPath;
        const std::string m_debugLogPrefix;
        std::string GetDebugLogPrefix(const std::string& deviceName, const CpuType& cpuType) const;

        /*** static consts ***/
        static const std::string m_configFileNamePrefix;
        static const std::string m_configFileNameExtension;

        static const std::string MODULE; // this one in use in configuration file (each module in a separate line)

        static const std::string EMPTY_STRING;
        static const std::string EMPTY_STRING_PLACEHOLDER;

        static const std::string configuration_parameter_value_delimiter;
        static const char configuration_delimiter_string; // delimiter between two configurations in string as oppose to configuration delimiter in file (line for each configuration)
        static const char new_line_delimiter;
    };
}



