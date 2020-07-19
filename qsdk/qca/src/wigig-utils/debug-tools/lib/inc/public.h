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

#ifndef __PUBLIC_H
#define __PUBLIC_H

#ifdef _WINDOWS
#include <initguid.h>
#define INITGUID


#define FSCTL_FILTER_BASE      FILE_DEVICE_UNKNOWN

#define _FILTER_CTL_CODE(_Function, _Method, _Access)  \
                        CTL_CODE(FSCTL_FILTER_BASE, _Function, _Method, _Access)

#define WCLT_SAMPLING_TO_FILE_IS_SUPPORTED
#else
typedef unsigned long ULONG;
#define _FILTER_CTL_CODE(_Function, _Method, _Access) (_Function)

typedef struct
{
  DWORD deviceUID;
  DWORD commandID;
  DWORD dataSize; /* inBufSize + outBufSize */
  DWORD inBufOffset;
  DWORD inBufSize;
  DWORD outBufOffset;
  DWORD outBufSize;
} wlct_ioctl_hdr_t;

static __INLINE BYTE *
wlct_ioctl_data_in(wlct_ioctl_hdr_t *h)
{
  return ((BYTE*)&h[1]) + h->inBufOffset;
}

static __INLINE BYTE *
wlct_ioctl_data_out(wlct_ioctl_hdr_t *h)
{
  return ((BYTE*)&h[1]) + h->outBufOffset;
}
#endif

#ifdef _WINDOWS
// Main Driver GUID
DEFINE_GUID (GUID_DEVINTERFACE_wDrvEp, 0x23d1c0df,0x48f9,0x4c38,0x97,0xa2,0x4a,0xba,0x44,0x97,0x36,0xd1);
// {23D1C0DF-48F9-4C38-97A2-4ABA449736D1}
#endif

//-----------------------------------------------------------------------------------
// Old IOCTLS
//-----------------------------------------------------------------------------------
#define IOCTL_FILTER_GET_CFG   \
            _FILTER_CTL_CODE(0x202, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_FILTER_SET_CFG   \
            _FILTER_CTL_CODE(0x203, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_FILTER_CLOSE_LOCAL_DEVICE   \
    _FILTER_CTL_CODE(0x204, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_FILTER_CLOSE_REMOTE_DEVICE   \
    _FILTER_CTL_CODE(0x205, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_FILTER_OPEN_LOCAL_DEVICE   \
    _FILTER_CTL_CODE(0x206, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_FILTER_OPEN_REMOTE_DEVICE   \
    _FILTER_CTL_CODE(0x207, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_OPEN_USER_MODE_EVENT   \
    _FILTER_CTL_CODE(0x208, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_CLOSE_USER_MODE_EVENT   \
    _FILTER_CTL_CODE(0x209, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_FILTER_GET_INTERRUPT_MODE   \
    _FILTER_CTL_CODE(0x20A, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_FILTER_SET_INTERRUPT_MODE   \
    _FILTER_CTL_CODE(0x20B, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_INDIRECT_READ_OLD \
    _FILTER_CTL_CODE(0x210, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_INDIRECT_WRITE_OLD \
    _FILTER_CTL_CODE(0x211, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_INDIRECT_READ_BLOCK \
    _FILTER_CTL_CODE(0x212, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_INDIRECT_READ_REPEAT \
    _FILTER_CTL_CODE(0x213, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_INDIRECT_WRITE_BLOCK \
    _FILTER_CTL_CODE(0x214, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


//-----------------------------------------------------------------------------------
// New IOCTLS
//-----------------------------------------------------------------------------------
#ifdef _WINDOWS
#define WILOCITY_DEVICE_TYPE                FILE_DEVICE_UNKNOWN
#define WILOCITY_IOCTL(_code_)                CTL_CODE (WILOCITY_DEVICE_TYPE, _code_, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#else
#define WILOCITY_IOCTL(_code_)                          _FILTER_CTL_CODE (_code_, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define ERROR_SUCCESS                                   0L
#endif


/* send WMI command to device.
   The content of pBuf is the WMI message itself. It doesn't include WMI header
   and Marlon MBOX header. */
#define WILOCITY_IOCTL_SEND_WMI                    WILOCITY_IOCTL (0x800)

/* receive WMI command from device.
   upon return, pBuf contains the WMI message itself. It doesn't include WMI header
   and Marlon MBOX header.
   This IOCTL should return with error code if there is no pending Rx command */
#define WILOCITY_IOCTL_RECEIVE_WMI                WILOCITY_IOCTL (0x801)

/* registration for indication of incoming WMI command.
   Following that, client will use IOCTL_RECEIVE_WMI to get the incoming WMI command */
#define WILOCITY_IOCTL_REGISTER_WMI_RX_EVENT    WILOCITY_IOCTL (0x803)

/* registration for user mode evnts from Driver. all events besides WMI RX Events */
#define WILOCITY_IOCTL_REGISTER_DEVICE_EVENT    WILOCITY_IOCTL (0x804)
#define WILOCITY_IOCTL_GET_DEVICE_EVENT            WILOCITY_IOCTL (0x805)
#define WILOCITY_IOCTL_REGISTER_EVENT            WILOCITY_IOCTL (0x850)

/* Special IOCTLS */
#define WILOCITY_IOCTL_DEVICE_SW_RESET            WILOCITY_IOCTL (0x807)

/* MiniPort driver only */
#define WILOCITY_IOCTL_DRIVER_LOG_LEVELS_CFG    WILOCITY_IOCTL(0x900)



//-----------------------------------------------------------------------------------
// Other Definitions
//-----------------------------------------------------------------------------------

#define BASE_ADDRESS    0x880000

// used to transfer address to the driver (input parameter)
// or value from the driver (output parameter)
// for example read register
typedef struct _FILTER_1_ULONG_PARAM
{
    DWORD            param;

} FILTER_1_ULONG_PARAM, *PFILTER_1_ULONG_PARAM;

// used to transfer address and value to the driver (input parameter)
// for example write register
typedef struct _FILTER_2_ULONG_PARAMS
{
    DWORD            param1;
    DWORD            param2;

} FILTER_2_ULONG_PARAM, *PFILTER_2_ULONG_PARAM;

#ifdef _WINDOWS
#pragma warning( disable : 4200 ) //suppress warning
#endif
typedef struct _FILTER_WRITE_BLOCK
{
    DWORD            address;
    DWORD            size;    // of 32 bits
    DWORD            buffer[0];

} FILTER_WRITE_BLOCK, *PFILTER_WRITE_BLOCK;

typedef struct _FILTER_INTERRUPT_MODE
{
    BYTE            interruptMode; // 0 - Interrupt, 1 - MSI enable

} FILTER_INTERRUPT_MODE, *PFILTER_INTERRUPT_MODE;


//
//  Structure to go with IOCTL_FILTE_OPEN_DEVICE.
//
typedef struct _FILTER_MAP_DEVICE
{
    BYTE            *userCRSAddress;
    size_t            userCRSSize;

} FILTER_MAP_DEVICE, *PFILTER_MAP_DEVICE;


typedef struct _FILTER_GET_CFG
{
    DWORD            offset;
    DWORD            value;

} FILTER_GET_CFG, *PFILTER_GET_CFG;


typedef struct _FILTER_SET_CFG
{
    DWORD            offset;
    DWORD            value;

} FILTER_SET_CFG, *PFILTER_SET_CFG;


typedef struct _FILTER_OPEN_DEVICE
{

    DWORD             reserved;
} FILTER_OPEN_DEVICE, *PFILTER_OPEN_DEVICE;

enum{
    PCI,
    CONF
};



/**
* WILOCITY_IOCTL_DRIVER_LOG_LEVELS_CFG
**/
enum {WILO_DRV_NUM_LOG_COMPS = 32};

typedef struct _DRIVER_LOGS_CFG
{
    ULONG logModulesEnableMsk;
    ULONG logModuleLevelsMsk[WILO_DRV_NUM_LOG_COMPS];
} DRIVER_LOGS_CFG, *P_DRIVER_LOGS_CFG;

#ifdef _WINDOWS
typedef struct _EVENT_HANDLE_64BIT
{
    ULONG    DeviceId;
    int    EventId;
    INT_PTR hEvent_l;
    INT_PTR hEvent_h;

} EVENT_HANDLE_64BIT, *PEVENT_HANDLE_64BIT;

typedef struct _EVENT_HANDLE_32BIT
{
    ULONG    DeviceId;
    int    EventId;
    INT_PTR hEvent;
} EVENT_HANDLE_32BIT, *PEVENT_HANDLE_32BIT;
#endif //_WINDOWS

#endif

