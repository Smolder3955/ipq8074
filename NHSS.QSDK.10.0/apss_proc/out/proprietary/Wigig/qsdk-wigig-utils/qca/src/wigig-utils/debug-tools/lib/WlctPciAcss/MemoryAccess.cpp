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

#include "MemoryAccess.h"

CMemoryAccess::CMemoryAccess(const TCHAR* tchDeviceName, DType devType)
{
    //do something with params
    (void)tchDeviceName;
    //if (devType == MST_MARLON)
    //{
    m_size = WLCT_DEV_MEMORY_SIZE;
    m_baseAddress = 0x800000;
    //}

    deviceType = devType;
    m_pMem = new char[m_size];
    if (!m_pMem)
    {
        LOG_MESSAGE_ERROR(_T("Cannot allocate memory access buffer"));
        exit(1);
    }

    memset(m_pMem, 0, m_size);
}

CMemoryAccess::~CMemoryAccess(void)
{
    CloseDevice();
}

int CMemoryAccess::GetType()
{
    return IDeviceAccess::ETypeMEMORY;
}

int CMemoryAccess::CloseDevice()
{
    if (m_pMem != NULL)
    {
        delete []m_pMem;
        m_pMem = NULL;
    }
    return 0;
}

int CMemoryAccess::r32(DWORD addr, DWORD & val)
{
    DWORD    actualAddr = addr - m_baseAddress;

    if (actualAddr >= m_size)
        return -1;

    actualAddr = actualAddr >> 2;
    val = ((DWORD*)m_pMem)[actualAddr];
    LOG_MESSAGE_DEBUG(_T("ADDR(in bytes): %04X Value: 0x%08X"), addr, val);
    return 0;
}

int CMemoryAccess::w32(DWORD addr, DWORD val)
{
    DWORD    actualAddr = addr - m_baseAddress;
    if (actualAddr >= m_size)
        return -1;

    actualAddr = actualAddr >> 2;
    ((DWORD*)m_pMem)[actualAddr] = val;
    LOG_MESSAGE_DEBUG(_T("ADDR: %04X Value: 0x%04X"),addr, val);
    return 0;
}

int CMemoryAccess::rb(DWORD addr, DWORD blockSize, char *arrBlock)
{
    DWORD    actualAddr = addr;
//    DWORD    val;

    //if (GetDeviceType() == MST_MARLON)
    //{
    actualAddr = addr - m_baseAddress;
    //}

    if ((actualAddr+blockSize) >= m_size)
        return -1;

    memcpy(arrBlock, &(m_pMem[actualAddr]), blockSize);

//     for (int inx = 0; inx < blockSize; inx++)
//     {
//         // wait between each loop
//         DWORD readFromIndex = actualAddr+inx;
//
//         if (GetDeviceType() == MST_SWIFT)
//         {
//             val = ((USHORT*)m_pMem)[readFromIndex];
//         }
//
//         if (GetDeviceType() == MST_MARLON)
//         {
//             val = ((DWORD*)m_pMem)[actualAddr];
//         }
//
//         arrBlock[inx] = val;
//     }

    return 0;
}

int CMemoryAccess::wb(DWORD addr, DWORD blockSize, const char *arrBlock)
{
    //do something with params
    (void)addr;
    (void)blockSize;
    (void)arrBlock;

    return 0;
}

void CMemoryAccess::rr32(DWORD addr, DWORD num_repeat, DWORD *arrBlock)
{
    DWORD* pFrom = &((DWORD*)m_pMem)[addr>>2];
    for (DWORD loop = 0; loop < num_repeat; loop++)
    {
        // wait between each loop

        arrBlock[loop] = *pFrom;
    }
}

int CMemoryAccess::rr(DWORD addr, DWORD num_repeat, DWORD *arrBlock)
{
    if (!isInitialized())
        return -1;

    DWORD    actualAddr = addr;

    //if (GetDeviceType() == MST_MARLON)
    //{
    actualAddr = addr - m_baseAddress;
    //}

    if (actualAddr >= m_size)
        return -1;

    //if (GetDeviceType() == MST_MARLON)
    //{
    rr32(actualAddr, num_repeat, arrBlock);
    //}
    return 0;
}

int CMemoryAccess::getFwDbgMsg(FW_DBG_MSG** pMsg)
{
    //do something with params
    (void)pMsg;

//    LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
}

int CMemoryAccess::clearAllFwDbgMsg()
{
    LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
}

int CMemoryAccess::do_reset(BOOL bFirstTime)
{
    //do something with params
    (void)bFirstTime;
    LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
}


int CMemoryAccess::do_sw_reset()
{
    LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
}


int CMemoryAccess::do_interface_reset()
{
    LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
}
