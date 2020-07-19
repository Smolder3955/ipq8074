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

#include "WlctPciAcssHelper.h"
#include "PciDeviceAccess.h"
#include <iostream>
#include <fstream>
#include <string>

#ifdef _WINDOWS
#include "JtagDeviceAccess.h"
#endif

#include <sys/time.h>

#define WLCT_MS_PER_S  1000
#define WLCT_US_PER_MS 1000
#define PROC_MODULES_PATH "/proc/modules"
#define DRIVER_KEYWORD "wil6210"

#define END_POINT_RESULT_MAX_SIZE 1000

using namespace std;

void CreatePciInterface(const char* interfaceName, DType devType, INTERFACE_LIST* ifList, int* ReturnItems);
void CreateMultipleInterfaces(const char* interfaceName, DType devType, INTERFACE_LIST* ifList, int* ReturnItems);
bool GetInterfaceNameFromEP(const char* pciEndPoint, char* interfaceName);
int GetNumberOfInterfaces();

int GetSimulatorInterfaces(INTERFACE_LIST* ifList, int* ReturnItems)
{
    // Not supported in Linux
    WLCT_UNREFERENCED_PARAM(ifList);

    return *ReturnItems;
}
bool WilDriverExist()
{
    ifstream ifs(PROC_MODULES_PATH);
    string DriverKey = DRIVER_KEYWORD;
    string line;
    while(getline(ifs, line)) {
        if (line.find(DriverKey, 0) != string::npos) {
            LOG_MESSAGE_INFO(_T("Found WIGIG interface [wEP0!MARLON]"));
            return true;
        }
    }

    return false;
}

int GetPciInterfaces(INTERFACE_LIST* ifList, int* ReturnItems)
{
    CreateMultipleInterfaces("wEP0L!SPARROW", MST_SPARROW, ifList, ReturnItems);

    return *ReturnItems;
}

void CreateMultipleInterfaces(const char* interfaceName, DType devType, INTERFACE_LIST* ifList, int* ReturnItems)
{
    //do something with params
    (void)devType;
    // Holds the console ouput containing the current End Point
    char pciEndPoints[END_POINT_RESULT_MAX_SIZE];

    // This command retrieves the PCI endpoints enumerated
    const char* szCmdPattern = "ls /sys/module/wil6210/drivers/pci\\:wil6210 | grep :";

    FILE* pIoStream = popen(szCmdPattern, "r");
    if (!pIoStream)
    {
        LOG_MESSAGE_ERROR("Failed to run command to detect End Points\n" );
        return;
    }

    LOG_MESSAGE_DEBUG("Searching for PCI end points: %s", szCmdPattern);

    while (fgets(pciEndPoints, END_POINT_RESULT_MAX_SIZE, pIoStream) != NULL)
    {
        // The command output contains a newline character that should be removed
        pciEndPoints[strcspn(pciEndPoints, "\r\n")] = '\0';
        LOG_MESSAGE_DEBUG("PCI End Point Found: %s", pciEndPoints);

        // Get interface name from End Point
        char networkInterfaceName[IFNAMSIZ] = {0};
        GetInterfaceNameFromEP(pciEndPoints, networkInterfaceName);

        // Append driver interface name
        std::string ifName = interfaceName;
        ifName += "!";
        ifName += networkInterfaceName;

        LOG_MESSAGE_DEBUG("Attempting to create interface named: %s", ifName.c_str());

        CreatePciInterface(ifName.c_str(), MST_SPARROW, ifList, ReturnItems);
    }

    pclose(pIoStream);
}


bool GetInterfaceNameFromEP(const char* pciEndPoint, char* interfaceName)
{
    char szInterfaceCmdPattern[END_POINT_RESULT_MAX_SIZE];

    // This command translates the End Point to the interface name
    snprintf(szInterfaceCmdPattern, END_POINT_RESULT_MAX_SIZE, "ls /sys/module/wil6210/drivers/pci:wil6210/%s/net", pciEndPoint);

    LOG_MESSAGE_DEBUG("Running command: %s", szInterfaceCmdPattern);

    FILE* pIoStream = popen(szInterfaceCmdPattern, "r");
    if (!pIoStream)
    {
        LOG_MESSAGE_ERROR("Failed to run command to detect End Points\n" );
        return false;
    }

    while (fgets(interfaceName, IFNAMSIZ, pIoStream) != NULL)
    {
        // The command output contains a newline character that should be removed
        interfaceName[strcspn(interfaceName, "\r\n")] = '\0';
        LOG_MESSAGE_DEBUG("PCI interface found: %s", interfaceName);

        return true;
    }

    pclose(pIoStream);
    return false;
}


void CreatePciInterface(const char* interfaceName, DType devType, INTERFACE_LIST* ifList, int* ReturnItems)
{
    void* res = NULL;
    void** tempAccess = &res;

    if(!CreateDeviceAccessHandler(interfaceName, devType, tempAccess))
    {
        snprintf(ifList->list[(*ReturnItems)].ifName, MAX_IF_NAME_LENGTH, "%s", interfaceName);
        (*ReturnItems) += 1;
        LOG_MESSAGE_INFO(_T("Found WIGIG interface"));
        if(res)
        {
            CloseDeviceAccessHandler(*tempAccess);
        }
    }
}

int GetSerialInterfaces(INTERFACE_LIST* ifList, int* ReturnItems)
{
    // Not supported in Linux
    WLCT_UNREFERENCED_PARAM(ifList);

    return *ReturnItems;
}

int GetJtagInterfaces(INTERFACE_LIST* ifList, int* ReturnItems)
{

#ifdef _WINDOWS
    DType devType;
    void* handler = NULL;
    DWORD err;
    uint32_t addr;
    uint32_t val;
    int ret = 0;

    char ifName[MAX_IF_NAME_LENGTH];

    //////////////////////////////////////////////////////////////////////////
    // find all jtag channels that connect to a WIGIG card
    //////////////////////////////////////////////////////////////////////////
    devType = MST_MARLON;
    addr = BAUD_RATE_REGISTER;

    int cdvc = 0;
    DVC dvc;
    char infoSet[28];

    LOG_MESSAGE_INFO(_T("scan jtag devices"));

    if (!funcDmgrEnumDevices(&cdvc)) {
        LOG_MESSAGE_INFO(_T("Error enumerating JTAG devices"));
        cdvc = 0;
    }

    for (int idvc = 0; idvc < cdvc; idvc++) {

        // DMGR API Call: funcDmgrGetDvc
        if (!funcDmgrGetDvc(idvc, &dvc)) {
            LOG_MESSAGE_INFO(_T("Error getting device info"));
            break;
        }
        DINFO dinfo = dinfoUsrName;
        BOOL bRes;
        snprintf(infoSet, sizeof(infoSet), "wjtag%d", idvc);
        bRes = funcDmgrSetInfo(&dvc, dinfo, (void*)infoSet);

        WLCT_UNREFERENCED_PARAM(bRes);

        snprintf(ifName, MAX_IF_NAME_LENGTH, "%s!MARLON", infoSet);
        err = CreateDeviceAccessHandler(ifName, devType, &handler);
        if (err == 0 && isInit(handler))
        {
            ret = WlctAccssRead(handler, addr, val);
            if (ret == 0 && val > 0 && val < 0xFFFF)
            {
                snprintf(ifList->list[(*ReturnItems)++].ifName, MAX_IF_NAME_LENGTH, "%s", ifName);
                LOG_MESSAGE_INFO(_T("Found JTAG WIGIG interface [%s]"),
                                 ifName);
            }
            CloseDeviceAccessHandler(handler);
            handler = NULL;
        }
    }

    /* Clean up and get out */
    // DMGR API Call: DmgrFreeDvcEnum
    funcDmgrFreeDvcEnum();

#endif
//do something with params
    WLCT_UNREFERENCED_PARAM(ifList);
    return *ReturnItems;
}

void ComSetParameters(CComPortDeviceAccess* pComDeviceAccss, DType devType)
{
    // Not supported in Linux
    WLCT_UNREFERENCED_PARAM(pComDeviceAccss);
    WLCT_UNREFERENCED_PARAM(devType);
}

int SetMachineSpeed(void* pDeviceAccss, DWORD baud_rate_target)
{
    // Not supported in Linux
    WLCT_UNREFERENCED_PARAM(pDeviceAccss);
    WLCT_UNREFERENCED_PARAM(baud_rate_target);

    return -1;
}

uint64_t GetTimeStampMs()
{
    struct timeval tv = {0,0};
    if (gettimeofday(&tv, NULL) == 0)
    {
        return tv.tv_sec * WLCT_MS_PER_S + tv.tv_usec / WLCT_US_PER_MS;
    }

    return 0;
}

int GetPciAcssVersion(WLCT_DLL_VERSION *pVer)
{
    WLCT_ASSERT(pVer != NULL);
    pVer->major       = 0;
    pVer->minor       = 0;
    pVer->maintenance = 0;
    pVer->build       = 0;
    return 0;
}

