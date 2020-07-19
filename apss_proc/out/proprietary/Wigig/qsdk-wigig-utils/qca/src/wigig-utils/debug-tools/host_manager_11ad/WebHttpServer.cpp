/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

//configure Simple Web Server to use a standalone asio version
#define USE_STANDALONE_ASIO
#define ASIO_STANDALONE
//#define ASIO_HEADER_ONLY
#define ASIO_NO_TYPEID

#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifdef _WINDOWS
    #pragma warning(push, 3) // reduce warning level for external code
    #pragma warning(disable:4101) // unreferenced local variable in external code
#endif
#include <json/json.h>
#include <server_http.hpp>
#ifdef _WINDOWS
    #pragma warning(pop)
#endif

#include "WebHttpServer.h"
#include "Host.h"
#include "DebugLogger.h"
#include "FileSystemOsAbstraction.h"


using namespace std;
class HttpServer : public SimpleWeb::Server<SimpleWeb::HTTP>
{};

// *************************************************************************************************
#ifdef _WINDOWS
#define TIMEOUT_CODE 995
#else
#define TIMEOUT_CODE 125
#endif

WebHttpServer::WebHttpServer(unsigned int httpPort)
    :m_port(httpPort)
{
    m_pCommandsExecutor = make_shared<CommandsExecutor>();
    m_pServer = make_shared<HttpServer>();

    if ( !(m_pCommandsExecutor && m_pServer) )
    {
        LOG_ERROR << "Failed to initialize HttpServer" << endl;
        exit(1);
    }

    m_pServer->config.port = static_cast<unsigned short>(m_port);
    m_pServer->on_error = [](shared_ptr<HttpServer::Request> request, const SimpleWeb::error_code &ec)
    {
        (void)request;

        //this is not a real error but the result of closing a timed out connection
        if (ec.value() != TIMEOUT_CODE)
        {
            //LOG_ERROR << "Error in processing request: " << ec << endl;
        }
    };

    ConfigureRoutes();
}

// *************************************************************************************************

void WebHttpServer::StartServer()
{
    LOG_INFO << "Starting HTTP server at port: " << m_port << endl;
    m_pServer->start();
}

void WebHttpServer::Stop()
{
    LOG_INFO << "Stopping the web HTTP server" << endl;
    m_pServer->stop();
}


// Recursive function that sends a file in chunks using callbacks
void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs) {
    // Read and send 128 KB at a time
    static vector<char> buffer(131072); // Safe when server is running on one thread
    streamsize read_length;
    if ((read_length = ifs->read(&buffer[0], buffer.size()).gcount()) > 0) {
        response->write(&buffer[0], read_length);
        if (read_length == static_cast<streamsize>(buffer.size())) {
            response->send([response, ifs](const SimpleWeb::error_code &ec) {
                if (!ec)
                    read_and_send(response, ifs);
                else
                    LOG_ERROR << "Connection interrupted" << endl;
            });
        }
    }
}

string GetFailureJson(const string& failureReason)
{
    // create the failure object
    Json::Value failure;
    failure["reason"] = failureReason;

    Json::StyledWriter writer;
    return writer.write(failure);
}

const string API_HOST = "/api/host";
const string API_HOST_REGEXP = "^/api/host$";

void WebHttpServer::ConfigureRoutes()
{
    //Get update data for host
    m_pServer->resource[API_HOST_REGEXP]["GET"] = [this](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
    {
        (void)request;

        //LOG_INFO << "Received update request" << endl;
        try
        {
            string json = GetHostDataJson();

            SimpleWeb::CaseInsensitiveMultimap header;
            header.emplace("Content-Type", "application/json; charset=UTF-8");
            response->write(json, header);

            //LOG_INFO << "Sent update response" << endl;
        }
        catch (const logic_error& e)
        {
            response->write(SimpleWeb::StatusCode::client_error_bad_request, GetFailureJson(e.what()));
            LOG_ERROR << "Failed to respond to an update request. " << e.what() << endl;
        }
        catch (const exception& e)
        {
            response->write(SimpleWeb::StatusCode::client_error_bad_request);
            LOG_ERROR << "Failed to respond to an update request. " << e.what() << endl;
        }
    };

    //Do a command on host
    m_pServer->resource[API_HOST_REGEXP]["POST"] = [this](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        LOG_INFO << "Received command request" << endl;
        try
        {
            PerformJsonHostCommands(request->content.string());
            //return empty json since we are expected to return some json on post
            response->write(SimpleWeb::StatusCode::success_ok, "{}");
        }
        catch (const logic_error& e)
        {
            response->write(SimpleWeb::StatusCode::client_error_bad_request, GetFailureJson(e.what()));
            LOG_ERROR << "Failed to respond to a change request. " << e.what() << endl;
        }
        catch (const exception& e)
        {
            response->write(SimpleWeb::StatusCode::client_error_bad_request);
            LOG_ERROR << "Failed to respond to a change request. " << e.what() << endl;
        }
    };


    m_pServer->default_resource["POST"] = [this](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {

        //workaround for non working regular expressions in GCC 4.8
        //the requests will fall through here
        if (request->path == API_HOST)
        {
            m_pServer->resource[API_HOST_REGEXP]["POST"](response, request);
        }

        //we should never get here
        response->write(SimpleWeb::StatusCode::client_error_bad_request, request->path);
        LOG_ERROR << "Invalid url in post request: " << request->path << endl;
    };

    //default get handler will return files
    m_pServer->default_resource["GET"] = [this](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {

        //workaround for non working regular expressions in GCC 4.8
        //the requests will fall through here
        if (request->path == API_HOST)
        {
            m_pServer->resource[API_HOST_REGEXP]["GET"](response, request);
        }

        LOG_INFO << "Received file request" << endl;
        try
        {
            string filePath(FileSystemOsAbstraction::GetTemplateFilesLocation());
            string fileName = GetFileName(request->path);
            filePath += fileName;

            auto ifs = make_shared<ifstream>();
            ifs->open(filePath.c_str(), ifstream::in | ios::binary | ios::ate);
            if (!ifs->good()) // file doesn't exist
            {
                throw invalid_argument("Could not open file " + filePath);
            }

            SimpleWeb::CaseInsensitiveMultimap header;

            //prohibit any cashing of returned files in browser
            header.emplace("Cache-Control", "no-cache, no-store, must-revalidate");

            string contentType  = GetFileContentType(fileName);
            if (!contentType.empty())
            {
                header.emplace("Content-Type", contentType);
            }

            auto length = ifs->tellg();
            ifs->seekg(0, ios::beg);

            std::stringstream ss;
            ss << length;
            header.emplace("Content-Length", ss.str());
            response->write(header);

            read_and_send(response, ifs);

            LOG_INFO << "Sent file response" << endl;
        }
        catch (const exception& e)
        {
            response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not provide file: " + request->path);
            LOG_ERROR << "Failed to respond to a get request. " << e.what() << endl;
        }
    };
}

string WebHttpServer::GetFileName(string requestPath)
{
    string fileName = requestPath;
    if (fileName.size() > 0 && fileName[0] == '/')
    {
        fileName = fileName.substr(1);
    }
    if (fileName.empty())
    {
        fileName = "on-target-ui.html";
    }

    return fileName;
}

bool endsWith(string const &fullString, string const &ending) {
    if (fullString.length() >= ending.length())
    {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else
    {
        return false;
    }
}

string WebHttpServer::GetFileContentType(string fileName)
{
    string contentType;
    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
    if (endsWith(fileName, "htm") || endsWith(fileName, "html"))
    {
        contentType = "text/html; charset=UTF-8";
    }
    else if (endsWith(fileName, "css"))
    {
        contentType = "text/css; charset=UTF-8";
    }
    else if (endsWith(fileName, "js"))
    {
        contentType = "application/javascript; charset=UTF-8";
    }
    else if (endsWith(fileName, "png"))
    {
        contentType = "image/png";
    }
    else if (endsWith(fileName, "jpg") || endsWith(fileName, "jpeg"))
    {
        contentType = "image/jpeg";
    }
    else if (endsWith(fileName, "gif"))
    {
        contentType = "image/gif";
    }

    return contentType;
}

string GetTimeString(const FwTimestamp& ts)
{
    ostringstream time;
    time << std::setfill('0') << std::setw(2) << ts.m_day << '/'
        << std::setfill('0') << std::setw(2) << ts.m_month << '/'
        << std::setfill('0') << std::setw(2) << ts.m_year << '-'
        << std::setfill('0') << std::setw(2) << ts.m_hour << ':'
        << std::setfill('0') << std::setw(2) << ts.m_min << ':'
        << std::setfill('0') << std::setw(2) << ts.m_sec;

    return time.str();
}

string GetFwVersionString(const FwVersion& ver)
{
    ostringstream version;
    version << ver.m_major << "." << ver.m_minor << "." << ver.m_subMinor << "." << ver.m_build;

    return version.str();
}

Json::Value GetDeviceDataJson(const DeviceData& data)
{
    Json::Value device;
    device["DeviceName"] = data.m_deviceName;
    device["Associated"] = data.m_associated;
    device["SignalStrength"] = data.m_signalStrength;
    device["FwAssert"] = data.m_fwAssert;
    device["uCodeAssert"] = data.m_uCodeAssert;
    device["MCS"] = data.m_mcs;
    device["Channel"] = data.m_channel;
    device["FwVersion"] = GetFwVersionString(data.m_fwVersion);
    device["BootLoaderVersion"] = data.m_bootloaderVersion;
    device["Mode"] = data.m_mode;
    device["CompilationTime"] = GetTimeString(data.m_compilationTime);
    device["HwType"] = data.m_hwType;
    device["HwTemp"] = data.m_hwTemp;
    device["RfType"] = data.m_rfType;
    device["RfTemp"] = data.m_rfTemp;
    device["Boardfile"] = data.m_boardFile;

    Json::Value rfArray(Json::arrayValue);
    for (const auto& rf : data.m_rf)
    {
        rfArray.append(rf);
    }
    device["Rf"] = rfArray;

    Json::Value fixed = Json::objectValue;
    for (const auto& reg : data.m_fixedRegisters)
    {
        fixed[reg.m_name] = reg.m_value;
    }
    device["FixedRegisters"] = fixed;

    Json::Value custom = Json::objectValue;
    for (const auto& reg : data.m_customRegisters)
    {
        custom[reg.m_name] = reg.m_value;
    }
    device["CustomRegisters"] = custom;

    return device;
}

string WebHttpServer::GetHostDataJson()
{
    HostData data;
    string failureReason;
    if (!m_pCommandsExecutor->GetHostData(data, failureReason))
    {
        throw logic_error(failureReason);
    }

    // create the main object
    Json::Value host;
    host["HostManagerVersion"] = data.m_hostManagerVersion;
    host["HostAlias"] = data.m_hostAlias;
    host["HostIP"] = data.m_hostIP;

    Json::Value devices(Json::arrayValue);
    for (auto it = data.m_devices.begin(); it != data.m_devices.end(); it++)
    {
        devices.append(GetDeviceDataJson(*it));
    }

    host["Devices"] = devices;

    Json::StyledWriter writer;
    return writer.write(host);
}

void UpdateAliasName(const Json::Value& command, shared_ptr<CommandsExecutor> pCommandsExecutor)
{
    if (!command.isMember("Name") || !command["Name"].isString())
    {
        throw invalid_argument("Invalid SetAlias command");
    }

    string name = command["Name"].asString();
    LOG_INFO << "Executing SetAlias command. Name: " << name << endl;
    string failureReason;
    if (!pCommandsExecutor->SetHostAlias(name, failureReason))
    {
        throw logic_error(failureReason);
    }
}

void AddRegister(const Json::Value& command, shared_ptr<CommandsExecutor> pCommandsExecutor)
{
    if (!command.isMember("Name") || !command["Name"].isString() ||
        !command.isMember("DeviceName") || !command["DeviceName"].isString() ||
        !command.isMember("Address") || !command["Address"].isString() ||
        !command.isMember("FirstBit") || !command["FirstBit"].isString() ||
        !command.isMember("LastBit") || !command["LastBit"].isString())
    {
        throw invalid_argument("Invalid AddRegister command");
    }

    string deviceName = command["DeviceName"].asString();
    string name = command["Name"].asString();
    const char *startptr = command["Address"].asCString();
    const char *startptrMaskStart = command["FirstBit"].asCString();
    const char *startptrMaskEnd = command["LastBit"].asCString();

    if ( !(startptr && startptrMaskStart && startptrMaskEnd) )
    {   // not strictly needed since all the keys where already tested
        // added to remove static code analysis warning
        throw invalid_argument("Invalid AddRegister command");
    }

    char *endptr = nullptr;
    unsigned long numAddress = strtoul(startptr, &endptr, 16);
    if (endptr == startptr || *endptr != '\0' || numAddress > UINT32_MAX)
    {
        throw invalid_argument("Invalid AddRegister command");
    }

    endptr = nullptr;
    unsigned long numFirstBit = strtoul(startptrMaskStart, &endptr, 10); // mask definition is decimal
    if (endptr == startptrMaskStart || *endptr != '\0' || numFirstBit > UINT32_MAX)
    {
        throw invalid_argument("Invalid AddRegister command");
    }

    endptr = nullptr;
    unsigned long numLastBit = strtoul(startptrMaskEnd, &endptr, 10); // mask definition is decimal
    if (endptr == startptrMaskEnd || *endptr != '\0' || numLastBit > UINT32_MAX)
    {
        throw invalid_argument("Invalid AddRegister command");
    }

    LOG_INFO << "Executing AddRegister command. Device: " << deviceName
        << ", Name: " << name << ", Address: " << Hex<>(numAddress)
        << ", Mask: bits " << numFirstBit << ":" << numLastBit << endl;
    string failureReason;
    if (!pCommandsExecutor->AddDeviceRegister(deviceName, name, static_cast<uint32_t>(numAddress),
        static_cast<uint32_t>(numFirstBit), static_cast<uint32_t>(numLastBit), failureReason))
    {
        throw logic_error(failureReason);
    }
}

void RemoveRegister(const Json::Value& command, shared_ptr<CommandsExecutor> pCommandsExecutor)
{
    if (!command.isMember("Name") || !command["Name"].isString() ||
        !command.isMember("DeviceName") || !command["DeviceName"].isString())
    {
        throw invalid_argument("Invalid RemoveRegister command");
    }

    string deviceName = command["DeviceName"].asString();
    string name = command["Name"].asString();

    LOG_INFO << "Executing RemoveRegister command. Device: " << deviceName << ", Name: " << name << endl;
    string failureReason;
    if (!pCommandsExecutor->RemoveDeviceRegister(deviceName, name, failureReason))
    {
        throw logic_error(failureReason);
    }
}

void WebHttpServer::PerformJsonHostCommands(const string& json)
{
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(json, root))
    {
        throw invalid_argument("Invalid command");
    }

    if (root.isMember("SetAlias"))
    {
        UpdateAliasName(root["SetAlias"], m_pCommandsExecutor);
    }

    if (root.isMember("AddRegister"))
    {
        AddRegister(root["AddRegister"], m_pCommandsExecutor);
    }

    if (root.isMember("RemoveRegister"))
    {
        RemoveRegister(root["RemoveRegister"], m_pCommandsExecutor);
    }
}
