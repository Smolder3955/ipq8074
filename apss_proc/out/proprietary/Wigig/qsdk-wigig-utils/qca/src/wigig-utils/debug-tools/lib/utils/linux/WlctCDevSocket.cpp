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

#include "WlctCDevSocket.h"
#include "LoggerSupport.h"
#include "public.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <string.h>


//insted of "ioctl_if.h"
#define WILOCITY_IOCTL_INDIRECT_READ IOCTL_INDIRECT_READ_OLD
#define WILOCITY_IOCTL_INDIRECT_WRITE IOCTL_INDIRECT_WRITE_OLD
#define WILOCITY_IOCTL_INDIRECT_READ_BLOCK IOCTL_INDIRECT_READ_BLOCK
#define WILOCITY_IOCTL_INDIRECT_WRITE_BLOCK IOCTL_INDIRECT_WRITE_BLOCK

#define EP_OPERATION_READ 0
#define EP_OPERATION_WRITE 1
#define WIL_IOCTL_MEMIO (SIOCDEVPRIVATE + 2)

const char* DEBUGFS_ROOT = "/sys/kernel/debug/ieee80211/";

CWlctCDevSocket::CWlctCDevSocket():CWlctCDevFile()
{
}

int sendRWIoctl(wil_memio & io,int fd, char* interfaceName, bool activateLogs = true)
{
    int ret;
    struct ifreq ifr;
    ifr.ifr_data = &io;

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", interfaceName);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    ret = ioctl(fd, WIL_IOCTL_MEMIO, &ifr);
    if (ret < 0 && activateLogs)
    {
        perror("ioctl");
    }

    return ret;
}

// Receives interface name (wigig#, wlan#) and checks if it is responding
bool setInterfaceName(const char* interfaceName, int fd)
{
    if (interfaceName == NULL)
    {
        LOG_MESSAGE_ERROR("Invalid interface name (NULL)");
        return false;
    }

    wil_memio io;
    io.addr = 0x880050; //baud rate
    io.op = EP_OPERATION_READ;

    LOG_MESSAGE_DEBUG("Checking interface name: %s", interfaceName);

    int ret = sendRWIoctl(io, fd, (char*)interfaceName, false);
    if(ret == 0)
    {
        LOG_MESSAGE_DEBUG("Successfuly set interface name: %s", interfaceName);
    return true;
    }


    return false;
}

wlct_os_err_t CWlctCDevSocket::Open(const char *fName, const char* ifName)
{
    wlct_os_err_t res = WLCT_OS_ERROR_GEN_FAILURE;

    WLCT_ASSERT(IsOpened() == false);
    WLCT_ASSERT(strlen(fName) < WLCT_CDEV_FILE_MAX_NAME_LEN);

    LOG_MESSAGE_DEBUG("Char device file %s opening...", fName);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (INVALID_FD == fd || fd < 0)
    {
    res = errno;
    LOG_MESSAGE_ERROR("Failed to open socket to device");
    Close();
        return res;
    }

    LOG_MESSAGE_DEBUG("Trying to open socket to driver, device name: %s", ifName);

    const TCHAR* const delimit = _T("!");

    TCHAR* token;
    TCHAR *next_token = NULL;

    token = _tcstok_s( (char*)ifName, delimit, &next_token);
    if (NULL == token)
    {
        LOG_MESSAGE_ERROR("No token found in %s", ifName);
        Close();
        return -1;
    }

    LOG_MESSAGE_DEBUG(_T("token: %s"), token);

    // SPARROW
    token = _tcstok_s( NULL, delimit, &next_token);
    if (NULL == token)
    {
        LOG_MESSAGE_ERROR("No card token found in %s", ifName);
        Close();
        return -1;
    }

    LOG_MESSAGE_DEBUG(_T("token: %s"), token);

    // wlan# or wigig#
    token = _tcstok_s( NULL, delimit, &next_token);
    if (NULL == token)
    {
        LOG_MESSAGE_ERROR("No interface token found in %s", ifName);
        Close();
        return -1;
    }
    LOG_MESSAGE_DEBUG(_T("token: %s"), token);

    snprintf(interfaceName, IFNAMSIZ, "%s", token);

    // Validate interface is 11ad interface
    if(!setInterfaceName(interfaceName, fd))
    {
        LOG_MESSAGE_ERROR("Failed to query interface %s", interfaceName);
        Close();
        return res;
    }

    LOG_MESSAGE_DEBUG("Char device socket opened: fd=%d", fd);
    LOG_MESSAGE_DEBUG("Looking for wil6210 in %s", DEBUGFS_ROOT);

    const char* szCmdPattern = "find /sys/kernel/debug/ieee80211/ -name wil6210";
    FILE* pIoStream = popen(szCmdPattern, "r");
    if (!pIoStream)
    {
        LOG_MESSAGE_ERROR("Failed to run command to detect DebugFS\n" );
        Close();
        return res;
    }

    bool debugFsFound = false;
    while (fgets(debugFSPath, WLCT_CDEV_FILE_MAX_NAME_LEN, pIoStream) != NULL)
    {
        // The command output contains a newline character that should be removed
        debugFSPath[strcspn(debugFSPath, "\r\n")] = '\0';
        LOG_MESSAGE_DEBUG("Found DebugFS Path: %s", debugFSPath);
        if (debugFsFound)
        {
            // TODO - support DebugFS with Multiple interfaces for PMC
            LOG_MESSAGE_INFO("DebugFS for Multiple WIGIG cards is not currently supported\n");
            //Close();
            //return res;
        }
        debugFsFound = true;
    }

    pclose(pIoStream);
    return WLCT_OS_ERROR_SUCCESS;
}


wlct_os_err_t CWlctCDevSocket::Ioctl(void *dataBuf, DWORD dataBufLen, DWORD ioctlFlags)
{
    // doing something with unused params:
    (void)dataBufLen;
    (void)ioctlFlags;

    wlct_ioctl_hdr_t *header = (wlct_ioctl_hdr_t*)dataBuf;
    int32_t* inBuf = (int32_t*)((char*)dataBuf + sizeof(wlct_ioctl_hdr_t));
    int32_t* outBuf = (int32_t*)((char*)dataBuf + sizeof(wlct_ioctl_hdr_t) + header->outBufOffset);
    int32_t outBufferSize = header->outBufSize;
    int Id = header->commandID;
    int ret = 0;

    //init for switch section
    wil_memio io;
    int numReads;
    int32_t sizeToWrite;
    PFILTER_WRITE_BLOCK inParam;
    int i;
    //end init for switch section


    switch(Id){
    case IOCTL_INDIRECT_READ_OLD:
        //case WILOCITY_IOCTL_INDIRECT_READ:
        //read is allways 32 bytes
        io.addr = inBuf[0];
        io.val = outBuf[0];
        io.op = EP_OPERATION_READ;
        ret = sendRWIoctl(io, fd, interfaceName);
        *outBuf = io.val;
        break;
    case IOCTL_INDIRECT_WRITE_OLD:
        //case WILOCITY_IOCTL_INDIRECT_WRITE:
        //write parameters are passed only throgth "in param"
        io.addr = inBuf[0];
        io.val = inBuf[1];
        io.op = EP_OPERATION_WRITE;
        ret = sendRWIoctl(io, fd, interfaceName);
        break;
        //rb and wb are temporary! they should be replaced by a driver Ioctl
    case WILOCITY_IOCTL_INDIRECT_READ_BLOCK:
        //blocks must be 32bit alligned!
        numReads = outBufferSize / 4;
        io.op = EP_OPERATION_READ;
        io.addr = inBuf[0];
        for(i = 0 ; i < numReads ; i++){
            ret = sendRWIoctl(io, fd, interfaceName);
            if(ret != 0){
                return -2;
            }
            outBuf[i] = io.val;
            io.addr += sizeof(int32_t);
        }
        break;
    case WILOCITY_IOCTL_INDIRECT_WRITE_BLOCK:
        //blocks must be 32bit alligned!
        inParam = reinterpret_cast<PFILTER_WRITE_BLOCK>(inBuf);
        io.addr = inParam->address;
        sizeToWrite = inParam->size / 4;
        io.op = EP_OPERATION_WRITE;
        for(i = 0 ; i < sizeToWrite ; i++){
            io.val = inParam->buffer[i];
            ret = sendRWIoctl(io, fd, interfaceName);
            if(ret != 0){
                return -3;
            }
            io.addr += sizeof(int32_t);
        }
        break;
    default:
        return -1;
    }

    return ret;

}

wlct_os_err_t CWlctCDevSocket::DebugFS(char *FileName, void *dataBuf, DWORD dataBufLen, DWORD DebugFSFlags)
{
    //doing somethin with unused params:
    (void)dataBufLen;
    (void)DebugFSFlags;
    char file_to_open[WLCT_CDEV_FILE_MAX_NAME_LEN] ;
    int *dataBuf_debugFS = (int*) dataBuf;
    int num_desc = *dataBuf_debugFS;
    dataBuf_debugFS++;
    int size_desc = *dataBuf_debugFS;

    LOG_MESSAGE_INFO(_T("DebugFS: %s"), FileName);

    if(strlen(debugFSPath) == 0) return WLCT_OS_ERROR_OPEN_FAILED;

    snprintf( file_to_open, WLCT_CDEV_FILE_MAX_NAME_LEN, "%s/%s", debugFSPath, FileName);
    std::ofstream debugFSFile;
    debugFSFile.open(file_to_open);

    if ( (debugFSFile.rdstate() & std::ifstream::failbit ) != 0 ){
        std::cout << "error while writing to debugfs! trying to write to address : ";
        std::cout << file_to_open << std::endl;
        return WLCT_OS_ERROR_GEN_FAILURE;
    }
    if(num_desc != 0 && size_desc != 0)    debugFSFile << "alloc " << num_desc << " " << size_desc;
    else debugFSFile << "free";
    debugFSFile.close();

    return WLCT_OS_ERROR_SUCCESS;
}

CWlctCDevSocket::~CWlctCDevSocket(){}
