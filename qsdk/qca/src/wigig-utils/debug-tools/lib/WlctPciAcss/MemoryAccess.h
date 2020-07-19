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

#pragma once
#include "DeviceAccess.h"

#define WLCT_DEV_MEMORY_SIZE    (0x129000)


extern "C" class CMemoryAccess : public IDeviceAccess
{
public:
    CMemoryAccess(const TCHAR* tchDeviceName, DType devType);
    virtual ~CMemoryAccess(void);
    virtual int CloseDevice();
    virtual int GetType();
    virtual int r32(DWORD addr, DWORD & val);
    virtual int w32(DWORD addr, DWORD val);
    virtual int rb(DWORD addr, DWORD blockSize, char *arrBlock);
    virtual int wb(DWORD addr, DWORD blockSize, const char *arrBlock);
    virtual int rr(DWORD addr, DWORD num_repeat, DWORD *arrBlock);
    virtual int getFwDbgMsg(FW_DBG_MSG** pMsg);
    virtual int clearAllFwDbgMsg();
    virtual int do_reset(BOOL bFirstTime = TRUE);
    virtual int do_sw_reset();
    virtual int do_interface_reset();

private:
    void rr32(DWORD addr, DWORD num_repeat, DWORD *arrBlock);

private:
    char*    m_pMem;
    DWORD    m_size;
    DWORD    m_baseAddress;
};
