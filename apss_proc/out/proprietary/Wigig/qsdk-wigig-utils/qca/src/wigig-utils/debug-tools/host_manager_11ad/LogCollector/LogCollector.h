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

#ifndef _LOG_COLLECTOR_H
#define _LOG_COLLECTOR_H
#pragma once

#include <memory>
#include <vector>

#include "LogCollectorConfiguration.h"
#include "LogTxtParser.h"
#include "RawLogLine.h"
#include "OperationStatus.h"

class Device;

namespace log_collector
{
    class LogCollectorRecorder;

    class LogCollector
    {
    public:
        static bool Tracer3ppLogsFilterOn;

        LogCollector(Device* device, CpuType tracerType, bool publishLogs, bool recordLogs, RecordingType recordingType);
        LogCollector(const LogCollector& lg) = delete;
        LogCollector& operator=(const LogCollector& lc) = delete;
        virtual ~LogCollector();

        // Publishing and recording methods
        OperationStatus SetRecordingMode(bool doRecording, RecordingType recordingType=RECORDING_TYPE_RAW);
        OperationStatus SetPublishingMode(bool publishMode);
        bool IsRecording() const;
        bool IsPublishing() const;

        // configuration functions
        OperationStatus SetModuleVerbosity(const std::string& module, const std::string& level);

        std::string GetModuleVerbosity(const std::string& module) const {return m_configurator->GetModulesVerbosityString(module); } // It isn't returned by value because if the device is destroyed, there would be a null reference (in device manager)
        LogCollectorConfiguration GetConfiguration() const { return *(m_configurator->m_configuration); }
        const std::string& GetLogsRecordingFolder() const { return m_configurator->m_configuration->GetLogFilesDirectoryPath();}
        OperationStatus SubmitConfiguration(const LogCollectorConfiguration& newConfiguration) const;

        // TODO : For the regular tcp server. should be removed in the future:
        bool SetParameter(const std::string& assignment); // TODO : For the regular tcp server. should be removed in the future
        std::string GetConfigurationString() const { return m_configurator->ToString(); }  // TODO : For the regular tcp server. should be removed in the future
        OperationStatus GetParameter(const std::string& parameter, std::string& value) const { return m_configurator->GetParameter(parameter, value); } // TODO : For the regular tcp server. should be removed in the future
        void SetCollectionMode(bool collect); // TODO : For the regular tcp server. should be removed in the future
        bool SetRecordingFlag(bool isRecordedNeeded); // TODO : For the regular tcp server. should be removed in the future

    private:
        void ResetState();
        unsigned GetAhbStartAddress(BasebandType bb) const;
        unsigned GetLinkerStartAddress(BasebandType bb) const;
        bool ComputeLogBufferStartAddress();
        int GetBufferSizeInBytesById(int bufferSizeId) const;

        std::string ParseRawMessage(log_event* evt, unsigned dword_num, std::vector<int> params, unsigned string_ofst);
        // configurations
        OperationStatus ApplyModuleVerbosity();
        bool ApplyModuleVerbosityIfNeeded();

        bool GetModuleInfoFromDevice() const;
        // OS agnostic read log function
        bool ReadLogBuffer();
        bool ReadLogWritePointer();

        void ReadBufferParseLogs(std::vector<RawLogLine>& rawLogLines);
        bool GetNextLogs(std::vector<RawLogLine>& rawLogLines, std::vector<std::string>& errorMessages); // distribute next logs chunck (without log lines that were already read) // TODO - maybe add string message and set it with the proper error message

        void PublishLogsIfNeeded(const std::vector<log_collector::RawLogLine>& rawLogLines) const;
        void WriteToFileIfNeeded(const std::string& logLine, bool debugPrint, RecordingType recordingType=RECORDING_TYPE_BOTH) const;

        void Poll();
        OperationStatus StartCollectIfNeeded();
        void StopLogCollectorRecorders();
        OperationStatus SetRecordingType(RecordingType recordingType);

        // m_recordingType should be updated before calling this method!
        OperationStatus PrepareLogCollectorRecording() const;

        // log collector's context
        std::shared_ptr<LogCollectorConfigurator> m_configurator;
        std::unique_ptr<LogCollectorRecorder> m_logCollectorRawRecorder;
        std::unique_ptr<LogCollectorRecorder> m_logCollectorTxtRecorder;
        Device* m_device;
        std::string m_deviceName;
        CpuType m_tracerCpuType;
        std::string m_debugLogPrefix;

        // managing collection
        bool m_isRecording; // log are being recorded
        bool m_isPublishing; // logs are being published in events
        RecordingType m_recordingType;

        unsigned m_pollerTaskId;

        // log buffer members
        std::unique_ptr<log_buffer> m_logHeader; // content of log buffer header
        int m_logAddress; // log buffer start address
        std::vector<unsigned char> m_log_buf; // log buffer content
        unsigned m_rptr; // last read address
        unsigned m_lastWptr; // last write ptr address (used for detecting buffer overflow)
        size_t m_logBufferContentSizeInDwords;
        LogTxtParser t_txtLogParser;
    };

}

#endif // !_LOG_COLLECTOR_H


