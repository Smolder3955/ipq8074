/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef PMC_RECORDING_FILE_CREATOR_H_
#define PMC_RECORDING_FILE_CREATOR_H_

#include <string>
#include "OperationStatus.h"
#include "HostManagerDefinitions.h"

//This class is responsible for creating the PMC data file and manifest file in a location which will be then used as an output for the user
class PmcRecordingFileCreator
{
    public:
        PmcRecordingFileCreator(const std::string& deviceName, BasebandType basebandType);
        OperationStatus CreatePmcFiles(std::string& pmcPath);

    private:
        std::string GetManifestXmlString(uint32_t swHead);
        const char* GetBasebandTypeString();
        OperationStatus CreateFinalPmcFileLocation(const std::string& originalPmcFilePath, std::string& newPmcDataPath);
        OperationStatus CreatePmcDataFiles(std::string& pmcFilePath);
        OperationStatus CreateManifestFile(const std::string& pmcFilePath);

        BasebandType m_baseBandType;
        std::string m_baseName;
        std::string m_deviceName;
        std::string m_hostName;
        uint32_t m_refNum;
};



#endif /* PMC_RECORDING_FILE_CREATOR_H_ */
