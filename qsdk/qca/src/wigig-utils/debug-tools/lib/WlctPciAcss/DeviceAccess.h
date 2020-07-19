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

#include "wlct_os.h"
#include "WlctPciAcss.h"
#include "LoggerSupport.h"

#define _DeviceTypeCOM        1
#define _DeviceTypePCI        2
#define _DeviceTypeJTAG            3
#define _DeviceTypeMEMORY    4

#define MAX_DEVICE_NAME_LEN 256

class IDeviceAccess
{
public:
    typedef enum
    {
        ETypeUnknown = -1,            // Unknown
        ETypeCOM    = _DeviceTypeCOM,
        ETypePCI    = _DeviceTypePCI,
        ETypeJTAG    = _DeviceTypeJTAG,
        ETypeMEMORY    = _DeviceTypeMEMORY,
    }
    EType;

    IDeviceAccess(const TCHAR* tchDeviceName, DType devType);
    IDeviceAccess(){deviceType = MST_NONE;}
    virtual ~IDeviceAccess(void){}
    virtual int CloseDevice() = 0;
    virtual int GetType() = 0;

    virtual int r32(DWORD addr, DWORD & val) = 0;
    virtual int w32(DWORD addr, DWORD val) = 0;

    // read block
    virtual int rb(DWORD addr, DWORD blockSize, char *arrBlock) = 0;
    // write block
    virtual int wb(DWORD addr, DWORD blockSize, const char *arrBlock) = 0;
    // read repeat
    virtual int rr(DWORD addr, DWORD num_repeat, DWORD *arrBlock) = 0;

    virtual int getFwDbgMsg(FW_DBG_MSG** pMsg) = 0;
    virtual int clearAllFwDbgMsg() = 0;
    virtual int do_reset(BOOL bFirstTime = TRUE) = 0;
    virtual int do_sw_reset() = 0;
    virtual int do_interface_reset() = 0;

    virtual int startSampling(
        DWORD* pRegsArr,
        DWORD regArrSize,
        DWORD interval,
        DWORD maxSampling,
        DWORD transferMethod)
    {
        //do something with params
        (void)pRegsArr;
        (void)regArrSize;
        (void)interval;
        (void)maxSampling;
        (void)transferMethod;

        LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
        return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
    }
    virtual int stopSampling()
    {
        LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
        return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
    }
    virtual int getSamplingData(DWORD** pDataSamples)
    {
        //do something with params
        (void)pDataSamples;
        LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
        return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
    }
    virtual int alloc_pmc(int num_of_descriptors, int size_of_descriptor)
    {
        //do something with params
        (void)num_of_descriptors;
        (void)size_of_descriptor;
        LOG_MESSAGE_ERROR(_T("NOT IMPLEMENTED"));
        return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
    }

    bool isInitialized()
    {
        return bInitialized;
    }
    wlct_os_err_t GetLastError()
    {
        return lastError;
    }
    void SetLastError(wlct_os_err_t err)
    {
        lastError = err;
    }
    TCHAR* GetInterfaceName()
    {
        return szInterfaceName;
    }
    DType GetDeviceType()
    {
        return deviceType;
    }
    DEVICE_STEP GetDeviceStep()
    {
        return deviceStep;
    }
    void SetDeviceStep(DEVICE_STEP devStep)
    {
        deviceStep = devStep;
    }

    //virtual int alloc_pmc(){
    //    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
    //}
protected:
    bool bInitialized;
    DType deviceType;
    DEVICE_STEP deviceStep;
    TCHAR szInterfaceName[MAX_DEVICE_NAME_LEN];
    std::deque<FW_DBG_MSG*> m_dbgMsgsList;

public:
    // Sampling
    DWORD    m_no_sampling_regs;
    DWORD    m_interval;
    DWORD    m_maxSampling;
    DWORD*    m_pRegsArr;
    DWORD    m_sampling_head;
    DWORD    m_sampling_tail;
    DWORD    m_loops;
    DWORD    oneSamplingSize;
    // cyclic buffer that hold the sampling
    DWORD*    m_sampling_arr;
    // I'm using this buffer for optimizations
    // when getSamplingData will called, we will copy the data that was capture
    // till now to this buffer.
    // We do not need to allocate it each time and not release it.
    DWORD*    m_get_sampling_arr;
    DWORD    m_transferMethod; // 1-save to file 2-save to buffer

    LONG m_flag_busy;
    wlct_os_err_t lastError;
};


