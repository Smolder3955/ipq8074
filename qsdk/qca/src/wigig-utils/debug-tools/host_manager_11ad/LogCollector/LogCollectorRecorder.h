/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _LOG_COLLECTOR_RECORDER_H
#define _LOG_COLLECTOR_RECORDER_H

#include <string>
#include <vector>
#include <memory>
#include <fstream>

#include "OperationStatus.h"
#include "LogCollector.h"

class Device;

namespace log_collector
{
    class LogCollectorRecorder
    {
    public:
        LogCollectorRecorder(
            Device* device, CpuType tracerType, std::string debugLogPrefix,
            std::shared_ptr<LogCollectorConfigurator> m_configurator, log_buffer* logHeader, RecordingType recordingType=RECORDING_TYPE_RAW);

        // recording methods
        void WriteToOutputFile(const std::string& logLine, bool debugPrint);
        OperationStatus HandleRecording();
        OperationStatus PrepareRecording();
        void StopRecording(); // Perform all required operations after collecting logs (e.g. close the log file if applicable)

        void IncreaseCurrentOutputFileSize(size_t outputFileSize);
        const std::string& GetRecordingFolder() const { return m_logFilesFolder; }

        void SetLogFilesFolder(const std::string & newPath) { m_logFilesFolder = newPath; };
        void SetLogFileExtension(const std::string & newExtension) { m_logFileExtension = newExtension; };


    private:
        Device* m_device;
        std::string m_debugLogPrefix;
        std::shared_ptr<LogCollectorConfigurator> m_configurator;
        log_buffer* m_logHeader;
        std::string m_logErrorMessagePrefix;
        RecordingType m_recorderType;

        // output file methods
        std::string GetNextOutputFileFullName();
        bool CreateNewOutputFile();
        void CloseOutputFile();
        void RemoveOldFilesIfNeeded();
        long GetNumOfLogFilesInFolder() const;
        std::vector<std::string> GetLogFilesSorted() const;
        bool RemoveOldestLogFileFromSortedVector(std::vector<std::string>& logFiles) const;

        // output file
        std::ofstream m_outputFile;
        size_t m_currentOutputFileSize;
        uint32_t m_currentNumOfOutputFiles;
        const std::string m_logFilePrefix;
        std::string m_logFilesFolder;
        std::string m_logFileExtension;
    };
}

#endif // !_LOG_COLLECTOR_RECORDER_H