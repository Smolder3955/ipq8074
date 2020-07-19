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

#include "wmi.h"

#define WBE_DRIVER_NAME L"wmp0"

// use INVALID_HANDLE_VALUE to de-register the event
#define INVALID_HANDLE_VALUE_LEGACY ((ULONG)(LONG_PTR)-1)
#define INVALID_HANDLE_VALUE        ((HANDLE)(LONG_PTR)-1)

// max value of the device id (user-mode application identifier) - related to WMI_DEVICE_ID
#define IOCTL_DEVICE_ID_MAX                      0xFF
#define IOCTL_DEVICE_ID_WBE_OLD                  0x00

#define WILOCITY_DEVICE_TYPE                    FILE_DEVICE_UNKNOWN
#define WILOCITY_IOCTL(_code_)                  CTL_CODE (WILOCITY_DEVICE_TYPE, _code_, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define WILOCITY_IOCTL_DEVICE_STATUS_EVENT        WILOCITY_IOCTL (0x208)    // API for Monitoring tool
#define WILOCITY_IOCTL_INDIRECT_READ            WILOCITY_IOCTL (0x210)
#define WILOCITY_IOCTL_INDIRECT_WRITE           WILOCITY_IOCTL (0x211)
#define WILOCITY_IOCTL_INDIRECT_READ_BLOCK      WILOCITY_IOCTL (0x212)
#define WILOCITY_IOCTL_INDIRECT_WRITE_BLOCK     WILOCITY_IOCTL (0x214)
#define WILOCITY_IOCTL_SEND_WMI                 WILOCITY_IOCTL (0x800)
#define WILOCITY_IOCTL_REGISTER_EVENT_NEW       WILOCITY_IOCTL (0x850)
#define WILOCITY_IOCTL_UNREGISTER_EVENTS        WILOCITY_IOCTL (0x851)
#define WILOCITY_IOCTL_RECEIVE_WMI_NEW          WILOCITY_IOCTL (0x852)    // New API Not implemented yet by WBE APP
#define WILOCITY_IOCTL_DEVICE_RESET             WILOCITY_IOCTL (0x807)
#define WILOCITY_IOCTL_SET_MODE_NEW                WILOCITY_IOCTL (0x808)    // New API for Concurrent mode controlling (Docking/Networking/hotspot), IOCTL_EVENT_SET_MODE_COMPLETE will be sent to UI on success
#define WILOCITY_IOCTL_GET_MODE_NEW                WILOCITY_IOCTL (0x809)    // API for UI to extract driver mode (example: after set mode fail, not event will be sent to UI)

// Deprecated API
#define WILOCITY_IOCTL_RECEIVE_WMI                 WILOCITY_IOCTL (0x801)
// registration for indication of incoming WMI command.
// Following that, client will use IOCTL_RECEIVE_WMI to get the incoming WMI command
#define WILOCITY_IOCTL_REGISTER_WMI_RX            WILOCITY_IOCTL (0x803)
// Registration for driver down event
#define WILOCITY_IOCTL_REGISTER_DEVICE            WILOCITY_IOCTL (0x804)

/**
*    WILOCITY_IOCTL_DEVICE_STATUS_EVENT
**/
#pragma pack(1)
typedef struct _DEVICE_STATUS_EVENT
{
    HANDLE  hDnEvent;                      // Link Down
    HANDLE  hUpEvent;                 // Link up
    HANDLE  unplugEvent;            // Surprise removal
    HANDLE  sysAssertEvent;            // Device Fatal Error
} DEVICE_STATUS_EVENT, *PDEVICE_STATUS_EVENT;
#pragma pack()

/**
* WILOCITY_IOCTL_INDIRECT_READ
**/
#pragma pack(1)
typedef struct _DRIVER_READ_DWORD
{
    ULONG            address; // Word output will be returned in IOCTL output buffer that was allocated by application
} DRIVER_READ_DWORD, *PDRIVER_READ_DWORD;
#pragma pack()

/**
* WILOCITY_IOCTL_INDIRECT_WRITE
**/
#pragma pack(1)
typedef struct _DRIVER_WRITE_DWORD
{
    ULONG            address;
    ULONG            data;
} DRIVER_WRITE_DWORD, *PDRIVER_WRITE_DWORD;
#pragma pack()

/**
* WILOCITY_IOCTL_INDIRECT_WRITE_BLOCK
**/
#pragma pack(1)
typedef struct _DRIVER_WRITE_BLOCK
{
    ULONG            address;
    ULONG            size;                      // in bytes
    ULONG            buffer[EMPTY_ARRAY_SIZE];     // buffer's data will be allocated by user-mode app right to the end of struct
} DRIVER_WRITE_BLOCK, *PDRIVER_WRITE_BLOCK;
#pragma pack()

/**
* WILOCITY_IOCTL_INDIRECT_READ_BLOCK
**/
#pragma pack(1)
typedef struct _DRIVER_READ_BLOCK
{
    ULONG            address;                      // Size is taken from IOCTL output buffer size
} DRIVER_READ_BLOCK, *PDRIVER_READ_BLOCK;
#pragma pack()

/**
* WILOCITY_IOCTL_REGISTER_EVENT_NEW
**/
typedef enum _NOTIFY_EVENT {
    IOCTL_EVENT_PCI_LINK_UP,
    IOCTL_EVENT_PCI_PRE_LINK_DOWN,
    IOCTL_EVENT_PCI_LINK_DOWN,
    IOCTL_EVENT_DEVICE_READY,
    IOCTL_EVENT_MAILBOX_MSG,
    IOCTL_EVENT_DEBUG_MAILBOX_MSG,
    IOCTL_EVENT_SYSASSERT,
    IOCTL_EVENT_DRIVER_DISABLE,
    IOCTL_EVENT_SET_MODE_COMPLETE,
    IOCTL_EVENT_DEVICE_UNPLUG,
    IOCTL_EVENT_ORACLE_START,
    IOCTL_EVENT_ORACLE_STOP,

    // !!!do not define any event below MAX_EVENT!!!
    IOCTL_EVENT_MAX,
} NOTIFY_EVENT, *PNOTIFY_EVENT;

#pragma pack(1)
typedef struct _DRIVER_REGISTER_EVENT_NEW
{
    ULONG           DeviceId;            // The application identifier from type WMI_DEVICE_ID
    NOTIFY_EVENT    EventId;
    HANDLE          UserEventHandle;        // The application CB to be called on event

} DRIVER_REGISTER_EVENT_NEW, *PDRIVER_REGISTER_EVENT_NEW;
#pragma pack()

/**
* WILOCITY_IOCTL_UNREGISTER_EVENTS
**/
#pragma pack(1)
typedef struct _DRIVER_UNREGISTER_EVENTS
{
    ULONG           DeviceId;            // of type WMI_DEVICE_ID, driver will unregistered all APP events

} DRIVER_UNREGISTER_EVENTS, *PDRIVER_UNREGISTER_EVENTS;
#pragma pack()

/**
* WILOCITY_IOCTL_REGISTER_WMI_RX_EVENT    // Used for WMI event registration
* WILOCITY_IOCTL_REGISTER_DEVICE          // USed for driver unload event registration
**/
#pragma pack(1)
typedef struct _DRIVER_REGISTER_EVENT
{
    ULONG UserEventHandle; // The application CB to be called on event

} DRIVER_REGISTER_EVENT, *PDRIVER_REGISTER_EVENT;
#pragma pack()

/**
* WILOCITY_IOCTL_SEND_WMI
**/
#pragma pack(1)
/* used by IOCTL_SEND_WMI and IOCTL_RECEIVE_WMI */
typedef struct SEND_RECEIVE_WMI_S
{
    enum WMI_COMMAND_ID uCmdId;
    U16                 uContext;
    U08                 uDevId;
    U16                 uBufLen;
    U08                 uBuf[EMPTY_ARRAY_SIZE];
} SEND_RECEIVE_WMI, *PSEND_RECEIVE_WMI;
#pragma pack()

/**
* WILOCITY_IOCTL_RECEIVE_WMI_NEW
**/
#pragma pack(1)
typedef struct RECEIVE_WMI_NEW_S
{
    ULONG       DeviceId;                             // The application identifier from type WMI_DEVICE_ID
    U08         IsFilterAccordingToMyDeviceId;       // Input param. If should pass all events (=FALSE) or only events with BCASD id or my id (=TRUE)
    U08         Reserved[3];
    U32         OutputBuffSize;                      // Output param. Number of bytes written to output buffer
    union {
        struct {
        U16     StartSequenceNumber;
        U16     StartRepCount;
        };
        U32     Counter;                         // Input param. 32 bit counter build from 16 bit FW sequence numbers and 16 bit driver's wrap counter
                                                 // To request first event, set counter to 0x0
    };
    U32         NumEventsToRead;                 // Input/Output param.
                                                 // Input: max number of events to read. Driver reads from the currently available events (does not block if requested more than available)
                                                 // Output: number of events written to output buffer
} RECEIVE_WMI_NEW, *PRECEIVE_WMI_NEW;
#pragma pack()

/**
* WILOCITY_IOCTL_SET_MODE_NEW
* input buffer contains SET_MODE_CMD_INPUT
* output buffer contains SET_MODE_CMD_OUTPUT
* WILOCITY_IOCTL_GET_MODE_NEW
* output buffer contains GET_MODE_CMD_OUTPUT
**/
typedef enum _DRIVER_MODE {
    IOCTL_WBE_MODE,
    IOCTL_WIFI_STA_MODE,
    IOCTL_WIFI_SOFTAP_MODE,
    IOCTL_CONCURRENT_MODE,    // This mode is for a full concurrent implementation  (not required mode switch between WBE/WIFI/SOFTAP)
    IOCTL_SAFE_MODE,          // A safe mode required for driver for protected flows like upgrade and crash dump...
} DRIVER_MODE, *PDRIVER_MODE;

#pragma pack(1)
typedef struct _SET_MODE_CMD_INPUT
{
    DRIVER_MODE mode;
} SET_MODE_CMD_INPUT, *PSET_MODE_CMD_INPUT;

typedef struct _SET_MODE_CMD_OUTPUT
{
    BOOLEAN result;
    DRIVER_MODE previousMode; // relevant only if result is TRUE
} SET_MODE_CMD_OUTPUT, *PSET_MODE_CMD_OUTPUT;

typedef struct _GET_MODE_CMD_OUTPUT
{
    DRIVER_MODE currentMode;
} GET_MODE_CMD_OUTPUT, *PGET_MODE_CMD_OUTPUT;
#pragma pack()



