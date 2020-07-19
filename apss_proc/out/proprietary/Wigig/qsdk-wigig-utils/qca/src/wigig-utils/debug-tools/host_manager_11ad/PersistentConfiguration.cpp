/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <fstream>

#include "PersistentConfiguration.h"
#include "FileSystemOsAbstraction.h"

// *************************************************************************************************

JsonTree::JsonTree(const char* szName)
    : m_name(szName ? szName : "")
{
}

void JsonTree::Import(const Json::Value& jsonHostingContainer)
{
    const Json::Value& jsonContainer = m_name.empty() ? jsonHostingContainer : jsonHostingContainer[m_name];

    LOG_VERBOSE << "Importing json container"
        << " Name: " << (m_name.empty() ? "<empty>" : m_name.c_str())
        << " Content:\n" << jsonContainer
        << std::endl;

    if (Json::Value::nullSingleton() == jsonContainer)
    {
        LOG_WARNING << "No json container " << m_name << " found during json import" << std::endl;
        return;
    }

    for (auto& pParameter : m_children)
    {
        pParameter->Import(jsonContainer);
    }
}

void JsonTree::Export(Json::Value& jsonHostingContainer) const
{
    Json::Value jsonContainer;

    for (auto& pParameter : m_children)
    {
        pParameter->Export(jsonContainer);
    }

    if (m_name.empty())
    {
        jsonHostingContainer = jsonContainer;
    }
    else
    {
        jsonHostingContainer[m_name] = jsonContainer;
    }

    LOG_VERBOSE << "Exported json container"
        << " Name: " << m_name
        << " Content: \n" << jsonContainer
        << std::endl;
}

void JsonTree::AddChildNode(IJsonSerializable* pJsonValueMap)
{
    m_children.emplace_back(pJsonValueMap);
}

// *************************************************************************************************

ConfigurationSection::ConfigurationSection(const char* szName, JsonTree& configTreeRoot)
    : JsonTree(szName)
{
    configTreeRoot.AddChildNode(this);
}

// *************************************************************************************************

HostConfigurationSection::HostConfigurationSection(JsonTree& configTreeRoot)
    : ConfigurationSection("Host", configTreeRoot)
    , StringCommandPort("StringCommandPort", 12348, *this)
    , JsonCommandPort("JsonCommandPort", 12346, *this)
    , EventPort("EventPort", 12339, *this)
    , UdpDiscoveryInPort("UdpDiscoveryInPort", 12349, *this)
    , UdpDiscoveryOutPort("UdpDiscoveryOutPort", 12350, *this)
    , HttpPort("HttpPort", 3000, *this)
    , KeepAliveIntervalSec("KeepAliveIntervalSec", 1, *this)
{
}

// *************************************************************************************************

LogConfigurationSection::LogConfigurationSection(JsonTree& configTreeRoot)
    : ConfigurationSection("LogCollector", configTreeRoot)
    , PollingIntervalMs("PollingIntervalMs", 50, *this)
    , TargetLocation("TargetLocation", FileSystemOsAbstraction::GetOutputFilesLocation() + "Logs" + FileSystemOsAbstraction::GetDirectoryDelimiter(), *this)
{
}

// *************************************************************************************************

PersistentConfiguration::PersistentConfiguration(const std::string& filePath)
    : JsonTree("")
    , HostConfiguration(*this)
    , LogCollectorConfiguration(*this)
    , m_filePath(BuildConfigFilePath(filePath))
{
    Load(); // Overwrite default configuration with persistent configuration
    Save(); // Align persistent configuration with the up-to-date content (remove redundant/add new fields)
}
PersistentConfiguration::~PersistentConfiguration()
{
    Save();
}

void PersistentConfiguration::CommitChanges()
{
    Save();
}

void PersistentConfiguration::Load()
{
    Json::Reader jsonReader;

    std::ifstream configFile(m_filePath);
    if (!configFile && ENOENT == errno)
    {
        // Configuration file does not exist
        LOG_INFO << "Configuration file does not exist at " << m_filePath << std::endl;
        return;
    }

    if (!configFile)
    {
        // Configuration will hold default hard-coded values
        LOG_ERROR << "Failed to load configuration file."
            << " Path: " << m_filePath
            << " Error: " << strerror(errno) << std::endl;
        return;
    }

    if (!jsonReader.parse(configFile, m_configRoot, true))
    {
        LOG_ERROR << "Failed to parse configuration file."
            << " Path: " << m_filePath
            << " Error: " << jsonReader.getFormattedErrorMessages()
            << std::endl;
        return;
    }

    LOG_INFO << "Loaded persistent configuration from: " << m_filePath << std::endl;
    LOG_DEBUG << "Persistent configuration file content:\n" << m_configRoot << std::endl;

    Import(m_configRoot);
}

void PersistentConfiguration::Save()
{
    std::ofstream configFile(m_filePath);
    if (!configFile)
    {
        // Configuration will hold default hard-coded values
        LOG_ERROR << "Failed to export configuration to file."
            << " Path: " << m_filePath
            << " Error: " << strerror(errno) << std::endl;
        return;
    }

    m_configRoot.clear();
    Export(m_configRoot);

    Json::StyledStreamWriter writer;
    writer.write(configFile, m_configRoot);

    LOG_INFO << "Saved configuration file at " << m_filePath << std::endl;
    LOG_DEBUG << "Persistent configuration file content:\n" << m_configRoot << std::endl;
}

std::string PersistentConfiguration::BuildConfigFilePath(const std::string& filePath)
{
    if (filePath.empty())
    {
        std::stringstream configFileBuilder;
        configFileBuilder << FileSystemOsAbstraction::GetConfigurationFilesLocation() << "host_manager_11ad.config";
        return configFileBuilder.str();
    }

    return filePath;
}
