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

#define PREPACK
#define POSTPACK
#pragma pack(1)
#include "wmi.h" // definition of all WMI command structures
#pragma pack()


/* used by IOCTL_REGISTER_WMI_RX_EVENT and IOCTL_REGISTER_DEVICE_EVENT */
typedef struct
{
    HANDLE hEvent; /* use INVALID_HANDLE_VALUE to de-register */
} REGISTER_EVENT;


/* used by IOCTL_OPEN_USER_MODE_EVENT  */
/*
    original structure inside the driver is as follows:
            typedef struct _SHARED_EVENT
            {
                HANDLE  hDnEvent;
                HANDLE  hUpEvent;
                HANDLE  unplugEvent;
                HANDLE  sysAssertEvent;

            } SHARED_EVENT, *PSHARED_EVENT;

    The HANDLE type inside the driver is platform speciefic, and
    can be either 32bit or 64bit!
    Since our DLL is assumed to be always compiled for 32bit platforms,
    it is a problem.

    Until the HANDLE types in the driver are changed to ULONG,
    or until this DLL will have two diffrent barnches for 32bit and 64bit platforms,
    the following Workarround is implemented:
    1. Diffrent structs for 32bit and 64bit platforms.
    2. when sending the IOCTL_OPEN_USER_MODE_EVENT, dynamically descide which struct to use.
*/


//those structs are deprecated
typedef struct _SHARED_EVENT_64BIT
{
    INT_PTR hDnEvent_l;
    INT_PTR hDnEvent_h;

    INT_PTR hUpEvent_l;
    INT_PTR hUpEvent_h;

    INT_PTR unplugEvent_l;
    INT_PTR unplugEvent_h;

    INT_PTR sysAssertEvent_l;
    INT_PTR sysAssertEvent_h;

} SHARED_EVENT_64BIT, *PSHARED_EVENT_64BIT;

typedef struct _SHARED_EVENT_32BIT
{
    INT_PTR hDnEvent;
    INT_PTR hUpEvent;
    INT_PTR unplugEvent;
    INT_PTR sysAssertEvent;

} SHARED_EVENT_32BIT, *PSHARED_EVENT_32BIT;