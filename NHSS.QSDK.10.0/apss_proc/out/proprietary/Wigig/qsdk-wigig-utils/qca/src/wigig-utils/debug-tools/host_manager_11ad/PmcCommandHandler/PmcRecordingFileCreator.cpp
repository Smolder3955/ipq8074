/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <stdlib.h>     // srand, rand
#include <time.h>       // time

#include <sstream>
#include <fstream>

#include "Host.h"
#include "DeviceManager.h"
#include "PmcRecordingFileCreator.h"
#include "PmcSequence.h"
#include "PmcDeviceConfigurations.h"
#include "PmcService.h"
#include "ShellCommandExecutor.h"

#ifdef _WINDOWS
#include <winsock2.h>       // For gethostname
#define HOST_NAME_MAX 64
#else
#include <unistd.h>         // For gethostname
#include <limits.h>         // For HOST_NAME_MAX
#endif

PmcRecordingFileCreator::PmcRecordingFileCreator(const std::string& deviceName, BasebandType basebandType) :
         m_baseBandType(basebandType), m_deviceName(deviceName)
{
    srand((uint32_t) time(NULL));
    m_refNum = rand();
    char hostName[HOST_NAME_MAX + 1];
    if (gethostname(hostName, HOST_NAME_MAX + 1) != 0)
    {
        LOG_ERROR << "Failed reading host name" << std::endl;
    }

    m_hostName = hostName;
    std::stringstream ssbn;
    ssbn << "pmc_" << m_hostName << "_" << m_deviceName + "_" << m_refNum;
    m_baseName = ssbn.str();
    LOG_INFO << "m_baseName is:" << m_baseName << std::endl;
}

OperationStatus PmcRecordingFileCreator::CreateFinalPmcFileLocation(const std::string& originalPmcFilePath, std::string& newPmcDataPath)
{
    std::string parentFolder = originalPmcFilePath.substr(0, originalPmcFilePath.find_last_of("/\\"));
    newPmcDataPath = parentFolder + "/" + m_baseName;
    std::string commandStr = "mkdir " + newPmcDataPath;
    ShellCommandExecutor command(commandStr.c_str());
    // process result
    if (command.ExitStatus() == 0)
    {
        return OperationStatus(true);
    }
    return OperationStatus(false, "Failed creating a new directory for pmc data files at: " + commandStr);
}

OperationStatus PmcRecordingFileCreator::CreatePmcDataFiles(std::string& pmcFilePath)
{
    std::string outMessage;
    if (!(Host::GetHost().GetDeviceManager().CreatePmcFiles(m_deviceName, m_refNum, outMessage) == DeviceManagerOperationStatus::dmosSuccess))
    {
        return OperationStatus(false, outMessage);
    }
    if (!(Host::GetHost().GetDeviceManager().FindPmcDataFile(m_deviceName, m_refNum, outMessage) == DeviceManagerOperationStatus::dmosSuccess))
    {
        return OperationStatus(false, outMessage);
    }

    auto dirCreateRes = CreateFinalPmcFileLocation(outMessage, pmcFilePath);
    if(!dirCreateRes.IsSuccess())
    {
        return dirCreateRes;
    }

    std::stringstream ssdst;
    ssdst << pmcFilePath << "/" << m_baseName << ".bin";

    // copy data file (mandatory)
    std::ifstream  src(outMessage, std::ios::binary);
    std::ofstream  dst(ssdst.str(), std::ios::binary);
    if(!(dst << src.rdbuf()))
    {
        std::stringstream ss;
        ss << "Failed copying pmc data file " << outMessage << " to on-target directory " << ssdst.str();
        return OperationStatus(false, ss.str());
    }
    std::remove(outMessage.c_str());

    // copy ring file (optional)
    if (!(Host::GetHost().GetDeviceManager().FindPmcRingFile(m_deviceName, m_refNum, outMessage) == DeviceManagerOperationStatus::dmosSuccess))
    {
        return OperationStatus(true, outMessage /*error message*/); // true since it's an optional file, and it's possible that driver doesn't support it
    }

    std::stringstream ssRingDst;
    ssRingDst << pmcFilePath << "/" << m_baseName << ".ring";

    std::ifstream  srcRing(outMessage, std::ios::binary);
    std::ofstream  dstRing(ssRingDst.str(), std::ios::binary);
    if (!(dstRing << srcRing.rdbuf()))
    {
        return OperationStatus(false, "Failed copying pmc ring file to on target directory");
    }
    std::remove(outMessage.c_str());

    return OperationStatus(true);
}

const char* PmcRecordingFileCreator::GetBasebandTypeString()
{
   if(m_baseBandType == BasebandType::BASEBAND_TYPE_SPARROW)
   {
       return "SPARROW";
   }
   if(m_baseBandType == BasebandType::BASEBAND_TYPE_TALYN)
   {
       return "TALYN";
   }
   return "UNKNOWN";
}

std::string PmcRecordingFileCreator::GetManifestXmlString(uint32_t swHead)
{
    std::string endRecordingTime = PmcService::GetInstance().GetPmcDeviceContext(m_deviceName).GetDeviceRecordingState().GetStopRecordingTime();
    std::string startRecordingTime = PmcService::GetInstance().GetPmcDeviceContext(m_deviceName).GetDeviceRecordingState().GetStartRecordingTime();
    uint32_t descriptorSize = DESCRIPTOR_SIZE_KB * 1024;
    const char* XML_INDENT = "    ";
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << std::endl
       << "<PmcRawDataSet xmlns:i=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://schemas.datacontract.org/2004/07/DmTools.PMC\">" << std::endl
       << XML_INDENT << "<BaseName>" << m_baseName << "</BaseName>" << std::endl
       << XML_INDENT << "<BasebandType>" << GetBasebandTypeString() << "</BasebandType>" << std::endl
       << XML_INDENT << "<DescriptorSize>" << descriptorSize << "</DescriptorSize>" << std::endl
       << XML_INDENT << "<DeviceName>" << m_deviceName << "</DeviceName>" << std::endl
       << XML_INDENT << "<EndRecordingTime>" << endRecordingTime << "</EndRecordingTime>" << std::endl
       << XML_INDENT << "<HostName>" << m_hostName << "</HostName>" << std::endl
       << XML_INDENT << "<NumberOfDescriptors>" << NUM_OF_DESCRIPTORS << "</NumberOfDescriptors>" << std::endl
       << XML_INDENT << "<StartRecordingTime>" << startRecordingTime << "</StartRecordingTime>" << std::endl
       << XML_INDENT << "<SwHead>" << swHead << "</SwHead>" << std::endl
       << "</PmcRawDataSet>" << std::endl;
    return ss.str();
}

OperationStatus PmcRecordingFileCreator::CreateManifestFile(const std::string& pmcFilePath)
{
    uint32_t swHead = 0;
    auto res = PmcSequence::ReadPmcHead(m_deviceName, swHead);
    if (!res.IsSuccess())
    {
        return OperationStatus(false,"Failed reading SW head");
    }
    std::string xmlString = GetManifestXmlString(swHead);
    std::stringstream ss;
    ss << pmcFilePath << "/" << m_baseName << ".manifest";
    std::ofstream out(ss.str());
    if(!(out << xmlString))
    {
        return OperationStatus(false, "Failed writing manifest file");
    }
    out.close();
    return OperationStatus(true);
}

OperationStatus PmcRecordingFileCreator::CreatePmcFiles(std::string& pmcFilesPath)
{
    auto pmcFileRes = CreatePmcDataFiles(pmcFilesPath);
    if(!pmcFileRes.IsSuccess())
    {
        return pmcFileRes;
    }
    auto manifestFileRes = CreateManifestFile(pmcFilesPath);
    if(!manifestFileRes.IsSuccess())
    {
        return manifestFileRes;
    }
    return OperationStatus(true);
}
