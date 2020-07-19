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

#include "IoctlDev.h"
#include "wlct_procfs.h"
#include "public.h"
#include "LoggerSupport.h"
#include <iostream>
#include <fstream>

#include <algorithm>

#define PROC_MODULES_PATH "/proc/modules"
#define DRIVER_KEYWORD "wil6210"

void CIoctlDev::FormatCDevFName(void)
{
    int sres = snprintf(cdevFileName, sizeof(cdevFileName),
                        "/tmp/%s", WLCT_CDEV_NAME);

    WLCT_ASSERT((size_t)sres < sizeof(cdevFileName));
    (void)sres;
}

CIoctlDev::CIoctlDev(const TCHAR *tchDeviceName)
    : IIoctlDev(tchDeviceName)
{
    if( strstr(szInterfaceName, "wEP") != NULL || strstr(szInterfaceName, "wep") != NULL ){
    cdevFile = new CWlctCDevSocket();
    }  else{
        cdevFile = new CWlctCDevFile();
    }
    // Name in format WLCT_PCI_LDEVNAME_FMT/WLCT_PCI_RDEVNAME_FMT
    FormatCDevFName();
}

CIoctlDev::~CIoctlDev()
{
    delete cdevFile;
    Close();
}

bool CIoctlDev::IsOpened(void)
{
    return cdevFile->IsOpened();
}

wlct_os_err_t CIoctlDev::DebugFS(char *FileName, void *dataBuf, DWORD dataBufLen, DWORD DebugFSFlags)
{
    return cdevFile->DebugFS(FileName, dataBuf, dataBufLen, DebugFSFlags);
}

bool WilDriverExistForEP(TCHAR *szInterfaceName)
{

    if( strstr(szInterfaceName, "wEP") != NULL || strstr(szInterfaceName, "wep") != NULL ){
        ifstream ifs(PROC_MODULES_PATH);
        string DriverKey = DRIVER_KEYWORD;
        string line;
        while(getline(ifs, line)) {
            if (line.find(DriverKey, 0) != string::npos) {
                LOG_MESSAGE_INFO(_T("Found WIGIG interface [wEP0!MARLON]"));
                return true;
            }
        }
    }
    return false;
}
wlct_os_err_t CIoctlDev::Open()
{
    wlct_os_err_t res = WLCT_OS_ERROR_GEN_FAILURE;

    WLCT_ASSERT(!IsOpened());

    bOldIoctls = TRUE;

    if (WilDriverExistForEP(szInterfaceName))
    {
        // Bug fix
        char tempInterfaceName[INTERFACE_NAME_LENGTH];
        snprintf(tempInterfaceName, INTERFACE_NAME_LENGTH, "%s", szInterfaceName);

        LOG_MESSAGE_DEBUG("Tokenizing: %s\n", tempInterfaceName);

        res = cdevFile->Open(cdevFileName, tempInterfaceName);
        if (res == WLCT_OS_ERROR_SUCCESS)
        {
            LOG_MESSAGE_DEBUG("Cdev file '%s' opened (uid=%u)",
                              cdevFileName, uID);
        }
        else
        {
            LOG_MESSAGE_ERROR("Cdev file '%s' (uid=%u) opening failed (%d)",
                              cdevFileName, uID, res);
        }
    }
    else
    {
        LOG_MESSAGE_ERROR("Cannot get UID by PciDevName (%s)",
                          szInterfaceName);
        res = WLCT_OS_ERROR_NO_SUCH_ENTRY;
    }

    return res;
}

void CIoctlDev::Close()
{
    if (IsOpened())
    {
        cdevFile->Close();
    }
}

wlct_os_err_t CIoctlDev::Ioctl(uint32_t Id,
                               const void *inBuf, uint32_t inBufSize,
                               void *outBuf, uint32_t outBufSize)
{
    wlct_os_err_t     res = WLCT_OS_ERROR_GEN_FAILURE;
    wlct_ioctl_hdr_t *ioctlHdr = NULL;
    uint32_t          ioctlBufSize = 0;

    WLCT_ASSERT(IsOpened());

    if (!inBuf || !inBufSize)
    {
        inBuf     = NULL;
        inBufSize = 0;
    }

    if (!outBuf || !outBufSize)
    {
        outBuf     = NULL;
        outBufSize = 0;
    }

    ioctlBufSize = sizeof(wlct_ioctl_hdr_t) + inBufSize + outBufSize;

    ioctlHdr = (wlct_ioctl_hdr_t *)malloc(ioctlBufSize);
    if (ioctlHdr)
    {
        uint32_t offset     = 0;
        uint32_t ioctlFlags = 0;

        memset(ioctlHdr, 0, ioctlBufSize);

        ioctlHdr->deviceUID    = uID;
        ioctlHdr->commandID    = Id;
        ioctlHdr->dataSize     = inBufSize + outBufSize;
        if (inBufSize)
        {
            ioctlHdr->inBufOffset  = offset;
            ioctlHdr->inBufSize    = inBufSize;
            ioctlFlags            |= WLCT_IOCTL_FLAG_SET;

            memcpy(wlct_ioctl_data_in(ioctlHdr), inBuf, inBufSize);

            offset += inBufSize;
        }

        if (outBufSize)
        {
            ioctlHdr->outBufOffset  = offset;
            ioctlHdr->outBufSize    = outBufSize;
            ioctlFlags             |= WLCT_IOCTL_FLAG_GET;

            offset += outBufSize;
        }

        res = cdevFile->Ioctl(ioctlHdr, ioctlBufSize, ioctlFlags);

        if (res != WLCT_OS_ERROR_SUCCESS)
        {
            LOG_MESSAGE_ERROR("IOCTL failed with error %u (id=%u)",
                              res, Id);
        }
        else if (outBufSize)
        {
            memcpy(outBuf, wlct_ioctl_data_out(ioctlHdr), outBufSize);
        }

        free(ioctlHdr);
    }
    else
    {
        LOG_MESSAGE_ERROR("Cannot allocate IOCTL buffer of %u bytes (id=%u)",
                          ioctlBufSize, Id);

        res = WLCT_OS_ERROR_NOT_ENOUGH_MEMORY;
    }


    return res;
}
