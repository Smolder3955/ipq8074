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
#include "WlctPciAcss.h"
#include "IoctlDev.h"
#include "Thread.h"
#include "public.h"

#ifdef _WINDOWS
#include "ioctl_if.h"
#else
// some definitions from ioctl_if.h
#define WILOCITY_IOCTL_INDIRECT_READ IOCTL_INDIRECT_READ_OLD
#define WILOCITY_IOCTL_INDIRECT_WRITE IOCTL_INDIRECT_WRITE_OLD
#define WILOCITY_IOCTL_INDIRECT_READ_BLOCK IOCTL_INDIRECT_READ_BLOCK
#define WILOCITY_IOCTL_INDIRECT_WRITE_BLOCK IOCTL_INDIRECT_WRITE_BLOCK
#endif

extern "C" class CPciDeviceAccess : public IDeviceAccess
{
public:
    CPciDeviceAccess(const TCHAR* tchDeviceName, DType devType);
    ~CPciDeviceAccess();

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
#ifdef _WINDOWS
    virtual int send_wmi_cmd(SEND_RECEIVE_WMI* wmi) ;
    virtual int recieve_wmi_event(SEND_RECEIVE_WMI* evt) ;

    virtual int SetDriverMode(int newState, int &oldState, int &payload);
    virtual int GetDriverMode(int &currentState);
#endif // _WINDOWS
    virtual int register_driver_mailbox_event(HANDLE* pMailboxEventHandle );
    virtual int register_driver_device_events(INT_PTR hDnEvent, INT_PTR hUpEvent, INT_PTR hUnplugEvent, INT_PTR hSysAssertEvent);
    virtual int unregister_driver_device_events();
    virtual int register_driver_device_event(INT_PTR hEvent, int eventId, int deviceId);
    virtual int register_device_unplug2(INT_PTR hEvent);
    virtual int unregister_device_unplug2();
    virtual int setMpDriverLogLevels(ULONG logModulesEnableMsk, ULONG logModuleLevelsMsk[WILO_DRV_NUM_LOG_COMPS]);
    virtual int alloc_pmc(int num_of_descriptors, int size_of_descriptor);

    int        reset_complete();
    void    Open();
    int        InternalCloseDevice();

    /*virtual int startSampling(
      DWORD* pRegsArr,
      DWORD regArrSize,
      DWORD interval,
      DWORD maxSampling,
      DWORD transferMethod);
      virtual int stopSampling();
      virtual int getSamplingData(DWORD** pDataSamples);*/

    // 0 - not closed
    // 1 - need to be close
    // 2 - close
    DWORD    m_close_state;
    // 0 - not opened
    // 1 - need to be open
    // 2 - open
    DWORD    m_open_state;

private:
    void rr32(DWORD addr, DWORD num_repeat, DWORD *arrBlock);
    int ReOpen();
    bool try2open(int timeout);

private:
    static void SamplingThreadProc(void *pDeviceAccss);
    static void PciPluginThreadProc(void *pDeviceAccss);

    // handle to the control device in wPci driver
    CIoctlDev            IoctlDev;

    // true if device is Local (wPciL)
    // false if device is remote (wPciR)
    bool        bIsLocal;

    DWORD        dwBaseAddress;

};
