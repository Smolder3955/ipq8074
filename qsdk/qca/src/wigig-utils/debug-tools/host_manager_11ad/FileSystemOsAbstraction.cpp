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

#ifdef __linux
    #include <sys/ioctl.h> // for struct ifreq
    #include <net/if.h> // for struct ifreq
    #include <arpa/inet.h> // for the declaration of inet_ntoa
    #include <netinet/in.h> // for struct sockaddr_in
    #include <stdlib.h>
    #include <string.h>
    #include <cerrno>
    #include <sys/stat.h> //added for config- template path
    #include <unistd.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <stdio.h>
#elif _WINDOWS
    /* Added here so windows.h will not include winsock.h that is interfering with winsock2.h */
    #define _WINSOCKAPI_
    #include <windows.h>
    #include <KnownFolders.h>
    #include <ShlObj.h>
    #include <Shlwapi.h>
    #include <comutil.h> //for _bstr_t (used in the string conversion)
    #include "Windows/WindowsConstants.h"

    #pragma comment(lib, "comsuppw")
#else
    #include <dirent.h>
#endif

#ifdef __ANDROID__
    #include <sys/stat.h> //added for config- template path
    #include <unistd.h> //added for config- template path
    #include <sys/types.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include "DebugLogger.h"
#include "FileSystemOsAbstraction.h"

using namespace std;

const std::string FileSystemOsAbstraction::LOCAL_HOST_IP = "127.0.0.1";

bool FileSystemOsAbstraction::FindEthernetInterface(struct ifreq& ifr, int& fd)
{
#ifdef __linux
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        LOG_WARNING << "Failed to get host's IP address and broadcast IP address" << std::endl;
        return false;
    }

    for (int i = 0; i < 100; i++)
    {
        snprintf(ifr.ifr_name, IFNAMSIZ - 1, "eth%d", i);

        if (ioctl(fd, SIOCGIFADDR, &ifr) >= 0)
        {
            return true;
        }
    }
#else
    (void)ifr;
    (void)fd;
#endif
    return false;
}

HostIps FileSystemOsAbstraction::GetHostIps()
{
#ifdef __linux
    HostIps hostIps;
    int fd;
    struct ifreq ifr;

    ifr.ifr_addr.sa_family = AF_INET; // IP4V

                                      // Get IP address according to OS
    if (FindEthernetInterface(ifr, fd))
    {
        LOG_INFO << "Linux OS" << std::endl;
    }
    else
    {
        snprintf(ifr.ifr_name, IFNAMSIZ - 1, "br-lan");
        if (ioctl(fd, SIOCGIFADDR, &ifr) >= 0)
        {
            LOG_INFO << "OpenWRT OS" << std::endl;
        }
        else
        {
            // Probably Android OS
            LOG_INFO << "Android OS (no external IP Adress)" << std::endl;
            hostIps.m_ip = FileSystemOsAbstraction::LOCAL_HOST_IP;
            hostIps.m_broadcastIp = FileSystemOsAbstraction::LOCAL_HOST_IP;
            return hostIps;
        }
    }

    hostIps.m_ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);

    if (ioctl(fd, SIOCGIFBRDADDR, &ifr) < 0)
    {
        LOG_WARNING << "Failed to get broadcast IP" << std::endl;
        return hostIps;
    }
    hostIps.m_broadcastIp = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    LOG_DEBUG << "Host's IP address is " << hostIps.m_ip << std::endl;
    LOG_DEBUG << "Broadcast IP address is " << hostIps.m_broadcastIp << std::endl;

    close(fd);
    return hostIps;
#else
    HostIps empty;
    return empty;
#endif
}

bool FileSystemOsAbstraction::ReadFile(const std::string& fileName, string& data)
{
    ifstream fd(fileName.c_str());
    if (!fd.good())
    {
        // Probably file doesn't exist
        LOG_WARNING << "Failed to open file: " << fileName  << std::endl;
        data = "";
        return false;
    }

    fd.open(fileName.c_str());
    stringstream content;
    content << fd.rdbuf();
    fd.close();
    data = content.str();
    return true;
}

bool FileSystemOsAbstraction::WriteFile(const string& fileName, const string& content)
{
    std::ofstream fd(fileName.c_str());
    if (!fd.is_open())
    {
        LOG_WARNING << "Failed to open file: " << fileName << std::endl;
        return false;
    }
    fd << content;
    if (fd.bad())
    {
        LOG_WARNING << "Failed to write to file: " << fileName << std::endl;
        fd.close();
        return false;
    }
    fd.close();
    return true;
}

bool FileSystemOsAbstraction::AppendToFile(const string& fileName, const string& content) // TODO - add a flag to WriteFile and remove this function (differ only in constructor)
{
    std::ofstream fd(fileName.c_str(), std::ios_base::app);
    if (!fd.is_open())
    {
        LOG_WARNING << "Failed to open file: " << fileName << std::endl;
        return false;
    }
    fd << content;
    if (fd.bad())
    {
        LOG_WARNING << "Failed to append to file: " << fileName << std::endl;
        fd.close();
        return false;
    }
    fd.close();
    return true;
}

string FileSystemOsAbstraction::GetConfigurationFilesLocation()
{
    stringstream path;

    //should check __ANDROID__ first since __LINUX flag also set in Android
#ifdef __ANDROID__
    std::string t_path = "/data/vendor/wifi/host_manager_11ad/";
    if (!FileSystemOsAbstraction::DoesFileExist(t_path))
    {
        path << "/data/host_manager_11ad/";
    }
    else
    {
        path << t_path;
    }
#elif __linux
    return "/etc/host_manager_11ad/";
#elif _WINDOWS //windows
    LPWSTR lpwstrPath = NULL;
    // Get the ProgramData folder path of windows
    HRESULT result = SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &lpwstrPath);
    if (SUCCEEDED(result))
    {
        // Convert the path to string
        std::wstring wpath(lpwstrPath);
        std::string strPath = std::string(wpath.cbegin(), wpath.cend());
        CoTaskMemFree(lpwstrPath);
        path << strPath << "\\Wilocity\\host_manager_11ad\\";
    }
#else //OS3
    return "/etc/host_manager_11ad/";
#endif // __linux
    return path.str();
}

std::string FileSystemOsAbstraction::GetTemplateFilesLocation()
{
    stringstream path;

    //should check __ANDROID__ first since __LINUX flag also set in Android


#ifdef  __ANDROID__
    std::string t_path = "/vendor/etc/wifi/host_manager_11ad/";
    if (!FileSystemOsAbstraction::DoesFileExist(t_path))
    {
        path << "/data/host_manager_11ad/";
    }
    else
    {
        path << t_path;
    }

#elif __linux
    return "/etc/host_manager_11ad/";
#elif _WINDOWS
    path << ".\\OnTargetUI\\";
#else //OS3
    return "/etc/host_manager_11ad/";
#endif // __linux
    return path.str();
}

string FileSystemOsAbstraction::GetOutputFilesLocation()
{
#ifdef __ANDROID__
    return "/data/host_manager_11ad_data/";
#elif __linux
    return "/var/host_manager_11ad_data/";
#elif _WINDOWS //windows
    return WINDOWS_OUTPUT_FILES_LOCATION;
#else //OS3
    return "/etc/host_manager_11ad_data/";
#endif // __linux
}


string FileSystemOsAbstraction::GetDefaultLogConversionFilesLocation()
{
#ifdef __ANDROID__
    return "/data/host_manager_11ad_data/";
#elif __linux
    return "/lib/firmware/";
#elif _WINDOWS //windows
    return "C:\\Qualcomm\\11adTools\\host_manager\\conversion_files\\";
#else //OS3
    return "/etc/host_manager_11ad_data/";
#endif // __linux
}


string FileSystemOsAbstraction::GetDirectoryDelimiter()
{
#ifndef _WINDOWS
    return "/";
#else
    return "\\";
#endif
}

bool FileSystemOsAbstraction::ReadHostOsAlias(string& alias)
{
#ifdef __linux
    if (!ReadFile("/etc/hostname", alias))
    {
        alias = "";
        return false;
    }
    return true;
#else
    alias = "";
    return false;
#endif // __linux
}

bool FileSystemOsAbstraction::DoesFolderExist(string path)
{
#ifndef _WINDOWS
    DIR* pDir = opendir(path.c_str());
    if (pDir != NULL)
    {
        (void)closedir(pDir);
        return true;
    }
    return false;
#else
    const DWORD fileAttributes = GetFileAttributesA(path.c_str());
    if (INVALID_FILE_ATTRIBUTES == fileAttributes) // no such path
    {
        return false;
    }
    if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        return true;   // given path is a directory
    }
    return false;    // given path isn't a directory
#endif
}

bool FileSystemOsAbstraction::DoesFileExist(const std::string& name)
{
#ifdef _WINDOWS
    struct _stat buf = {0};
    int Result = _stat(name.c_str(), &buf);
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    struct stat buf = {};
#pragma GCC diagnostic pop
    int Result = stat(name.c_str(), &buf);
#endif
    return Result == 0;
}

bool FileSystemOsAbstraction::CreateFolder(string path)
{
#ifndef _WINDOWS
    if (system(("mkdir " + path).c_str()) != 0)
    {
        return false;
    }
    return true;
#else
    if (path.empty())
    {
        return false;
    }
    std::wstring wpath = std::wstring(path.cbegin(), path.cend());

    bool ok = true;
    unsigned int pos = 0;
    do
    {
        pos = wpath.find_first_of(L"\\/", pos + 1);
        ok &= CreateDirectory(wpath.substr(0, pos).c_str(), NULL) == TRUE || GetLastError() == ERROR_ALREADY_EXISTS;
    } while (ok && pos != std::string::npos);
    return ok;
#endif
}

bool FileSystemOsAbstraction::MoveFileToNewLocation(const std::string& oldFileLocation, const std::string& newFileLocation)
{
#ifndef _WINDOWS
    system(("mv " + oldFileLocation + " " + newFileLocation).c_str());
    return true;
#else
    std::wstring wOldPath = std::wstring(oldFileLocation.cbegin(), oldFileLocation.cend());
    std::wstring wNewPath = std::wstring(newFileLocation.cbegin(), newFileLocation.cend());
    return MoveFile(wOldPath.c_str(), wNewPath.c_str()) == TRUE;
#endif
}

// "directory" argument ends with "/"
vector<std::string> FileSystemOsAbstraction::GetFilesByPattern(const string& directory, const string& filePrefix, const string& fileExtension)
{
    LOG_VERBOSE << "Enumerating files."
                << " Path: " << directory
                << " Prefix: " << filePrefix
                << " Ext: " << fileExtension
                << std::endl;

    vector<string> files;

#ifndef _WINDOWS

    DIR* pDirStream = opendir(directory.c_str());
    if (!pDirStream)
    {
        LOG_WARNING << "Failed to open " << directory << ". error: " << strerror(errno) << endl;
        return files;
    }

    struct dirent *pDirEntry = nullptr;
    while ((pDirEntry = readdir(pDirStream)) != nullptr)
    {
        const string fileName(pDirEntry->d_name);
        if (fileName.size() >= filePrefix.size() &&
            fileName.size() >= fileExtension.size() &&
            fileName.substr(0, filePrefix.size()) == filePrefix &&
            fileName.substr(fileName.size() - fileExtension.size(), fileExtension.size()) == fileExtension)
        {
            LOG_VERBOSE << "Found file: " << fileName << std::endl;
            files.push_back(fileName);
        }
    }

    if (errno != 0)
    {
        LOG_ERROR << "Error reading file list in directory."
                  << " Path: " << directory
                  << " Error: " << strerror(errno) << endl;
    }

    closedir(pDirStream);

#else

    stringstream pattern;
    pattern << directory << filePrefix << "*" << fileExtension;
    string patternStr(pattern.str());
    wstring wp(patternStr.begin(), patternStr.end());

    WIN32_FIND_DATA fdFile;
    HANDLE hFind = FindFirstFile(wp.c_str(), &fdFile);

    if (INVALID_HANDLE_VALUE == hFind)
    {
        if (ERROR_FILE_NOT_FOUND == GetLastError())
        {
            LOG_VERBOSE << "No file matches the pattern " << pattern.str() << endl;
        }
        else
        {
            LOG_WARNING << "Error while trying to retrieve first match of pattern: " << pattern.str() << " error: " << GetLastError() << endl;
        }
        return files;
    }

    do
    {
        wstring fileName(fdFile.cFileName);
        files.push_back(string(fileName.begin(), fileName.end()));
    }
    while (FindNextFile(hFind, &fdFile));

    if (GetLastError() != ERROR_NO_MORE_FILES)
    {
        LOG_WARNING << "Error while trying to retrive a match for pattern: " << pattern.str() << " error: " << GetLastError() << endl;
    }

    FindClose(hFind);

#endif
    return files;
}
