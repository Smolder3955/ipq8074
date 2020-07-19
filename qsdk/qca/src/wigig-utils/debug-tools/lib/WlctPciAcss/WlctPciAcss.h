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

#include <list>
#include <queue>
#include "wlct_os.h"
#include "WlctPciAcssWmi.h"
#include "public.h"

#ifdef _WINDOWS
#include "ioctl_if.h"
typedef struct _FW_DBG_MSG
{
    std::string    msg;
    SYSTEMTIME    timeStamp;
}FW_DBG_MSG;
#else
#define FW_DBG_MSG int
#endif

typedef struct _SAMPLING_REGS
{
    DWORD        timeStamp; // GetTickCount
    DWORD        regArr[0];
}SAMPLING_REGS;

#define MAX_DEVICE_TYPE_SUPPORTED 2
typedef enum DType_t
{
    MST_NONE,
    MST_SPARROW,   // added here to keep backward compatibility. some tools assume MARLON == 2, we don't brake this assumption
    MST_MARLON,
    MST_TALYN,
    MST_LAST
} DType;

typedef enum DEVICE_STEP_t
{
    STEP_NONE,
    MARLON_STEP_A0,
    MARLON_STEP_B0,
    SPARROW_STEP_A0,
    SPARROW_STEP_A1,
    SPARROW_STEP_B0,
    DFT_STEP,
    STEP_LAST
} DEVICE_STEP;

#define BAUD_RATE_REGISTER 0x880050

#define MAX_IF_NAME_LENGTH 32
typedef struct _INTERFACE_NAME
{
    char    ifName[MAX_IF_NAME_LENGTH];
}INTERFACE_NAME;


#define  MAX_INTERFACES 32
typedef struct _INTERFACE_LIST
{
    INTERFACE_NAME    list[MAX_INTERFACES];
}INTERFACE_LIST;

typedef struct _WLCT_DLL_VERSION
{
    int major;
    int minor;
    int maintenance;
    int build;

}WLCT_DLL_VERSION;

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the WLCTPCIACSS_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// WLCTPCIACSS_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.

#ifdef _WINDOWS
#ifdef WLCTPCIACSS_EXPORTS
#define WLCTPCIACSS_API __declspec(dllexport)
#else
#define WLCTPCIACSS_API __declspec(dllimport)
#endif
#endif

#if defined(__GNUC__)
#if defined(__i386)
#define WLCTPCIACSS_API extern "C" __attribute__((cdecl))
#else
#define WLCTPCIACSS_API extern "C"
#endif
#endif

// These are needed for wlanapi.h for pre-vista targets

typedef UCHAR DOT11_MAC_ADDRESS[6];
typedef DOT11_MAC_ADDRESS * PDOT11_MAC_ADDRESS;

typedef struct
{
    UINT32 Ready;
    UINT32 StructVersion;
    UINT32 RfType;
    UINT32 BaseBandType;
    DOT11_MAC_ADDRESS MacAddr;
    UINT8  Padding[2];
}USER_RGF_BL_INFO_VER_0;

typedef struct
{
    UINT32 Ready;
    UINT32 StructVersion;
    UINT32 RfType;
    UINT32 BaseBandType;
    DOT11_MAC_ADDRESS MacAddr;
    UINT8    VersionMajor;       // 0x880A52 BL ver. major
    UINT8    VersionMinor;       // 0x880A53 BL ver. minor
    UINT16    VersionSubminor;    // 0x880A54 BL ver. subminor
    UINT16    VersionBuild;        // 0x880A56 BL ver. build
                                // valid only for version 2 and above
    UINT16  AssertCode;         // 0x880A58 BL Assert code
    UINT16  AssertBlink;        // 0x880A5C BL Assert Branch
    UINT16  Reserved[22];        // 0x880A60 - 0x880AB4
    UINT16  MagicNumber;        // 0x880AB8 BL Magic number
}USER_RGF_BL_INFO_VER_1;

#define BIT(x) (1 << (x))

// Register definitions - taken from the REG files. addresses are from FW point of view
#define USER_RGF_USAGE_6                    0x880018

// Boot loader registers
#define USER_RGF_BL_START_OFFSET            0x880A3C
#define USER_RGF_BL_READY                    USER_RGF_BL_START_OFFSET
#define BL_BIT_READY                    BIT(0)

// reset sequence related
#define USER_RGF_HP_CTRL                    0x88265c
#define USER_RGF_FW_DEDICATED_REG_REV_ID    0x880a8c
#define PCIE_RGF_MAC_HW_ID_DB1          0x1
#define PCIE_RGF_MAC_HW_ID_DB2          0x2
#define USER_RGF_CLKS_CTL_SW_RST_MASK_0     0x880b14
#define USER_RGF_MAC_CPU_0                  0x8801fc
#define USER_RGF_USER_CPU_0                 0x8801e0
#define USER_RGF_CLKS_CTL_SW_RST_VEC_0      0x880b04
#define USER_RGF_CLKS_CTL_SW_RST_VEC_1      0x880b08
#define USER_RGF_CLKS_CTL_SW_RST_VEC_2      0x880b0c
#define USER_RGF_CLKS_CTL_SW_RST_VEC_3      0x880b10
#define USER_RGF_LOS_COUNTER_CTL            0x882dc4
#define USER_RGF_SERIAL_BAUD_RATE           0x880050
#define USER_RGF_SERIAL_BAUD_RATE_READY 0x15e
#define USER_RGF_PLL_CLR_LOS_COUNTER_CTL    0x882dc4
#define USER_RGF_HW_MACHINE_STATE           0x8801dc
#define HW_MACHINE_BOOT_DONE                0x3FFFFFD
#define USER_RGF_CLKS_CTL_0                 0x880abc
#define BIT_USER_CLKS_CAR_AHB_SW_SEL        BIT(1) /* ref clk/PLL */
#define BIT_USER_CLKS_RST_PWGD              BIT(11)

#define USER_RGF_JTAG_DEVICE_TYPE           0x880B34
#define USER_RGF_JTAG_DEVICE_SPARROW_A0                0x0632072F
#define USER_RGF_JTAG_DEVICE_SPARROW_A1                0x1632072F
#define USER_RGF_JTAG_DEVICE_SPARROW_B0                0x2632072F
#define USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_0    (0x880c18)
#define USER_RGF_CLKS_CTL_EXT_SW_RST_VEC_1    (0x880c2c)
#define USER_RGF_SPARROW_M_4            (0x880c50) /* Sparrow */
#define BIT_SPARROW_M_4_SEL_SLEEP_OR_REF    BIT(2)

#define USER_RGF_CAF_ICR                        0x88946c /* struct RGF_ICR */
#define RGF_CAF_ICR_ICR                         (USER_RGF_CAF_ICR+4)
#define RGF_CAF_ICR_IMV                         (USER_RGF_CAF_ICR+16)
#define USER_RGF_CAF_OSC_CONTROL                0x88afa4
#define BIT_CAF_OSC_XTAL_EN        BIT(0)
#define USER_RGF_CAF_PLL_LOCK_STATUS        0x88afec
#define BIT_CAF_OSC_DIG_XTAL_STABLE    BIT(0)

#define XTAL_STABLE_WAIT_MS     0x7

#define ALIGN_DOWN_BY(length, alignment) \
    (length & ~(alignment - 1))

#define ALIGN_UP_BY(length, alignment) \
    (ALIGN_DOWN_BY((length + alignment - 1), alignment))

WLCTPCIACSS_API int GetMyVersion(WLCT_DLL_VERSION *pVer);
WLCTPCIACSS_API bool wPciDisableEnable();
WLCTPCIACSS_API int GetInterfaces(INTERFACE_LIST* ifList, int* ReturnItems);
WLCTPCIACSS_API int StrGetInterfaces(char* ifListStr, int capacity);
WLCTPCIACSS_API int CreateDeviceAccessHandler(const char* deviceName, DType devType, void** pDeviceAccess);
WLCTPCIACSS_API int CloseDeviceAccessHandler(void* pDeviceAccss);
WLCTPCIACSS_API int SwReset(void* pDeviceAccss);
WLCTPCIACSS_API int InterfaceReset(void* pDeviceAccss);
WLCTPCIACSS_API int WlctAccssRead(void* pDeviceAccss, DWORD addr, DWORD & val);
WLCTPCIACSS_API int WlctAccssWrite(void* pDeviceAccss, DWORD addr, DWORD val);
WLCTPCIACSS_API bool isInit(void* pDeviceAccss);
WLCTPCIACSS_API int writeBlock(void* pDeviceAccss, DWORD addr, DWORD blockSize, const char *arrBlock);
WLCTPCIACSS_API int readBlock(void* pDeviceAccss, DWORD addr, DWORD blockSize, char *arrBlock);
WLCTPCIACSS_API int readRepeat(void* pDeviceAccss, DWORD addr, DWORD num_repeat, DWORD *arrBlock);
WLCTPCIACSS_API int getDbgMsg(void* pDeviceAccss, FW_DBG_MSG** pMsg);
WLCTPCIACSS_API int clearAllFwDbgMsg(void* pDeviceAccss);
WLCTPCIACSS_API int card_reset(void* pDeviceAccss);
WLCTPCIACSS_API TCHAR* getInterfaceName(void* pDeviceAccss);
WLCTPCIACSS_API DType getDeviceType(void* pDeviceAccss);
WLCTPCIACSS_API int startSampling(void* pDeviceAccss, DWORD* pRegsArr, DWORD regArrSize, DWORD interval, DWORD maxSampling, DWORD transferMethod);
WLCTPCIACSS_API int stopSampling(void* pDeviceAccss);
WLCTPCIACSS_API int getSamplingData(void* pDeviceAccss, DWORD** pDataSamples);
WLCTPCIACSS_API int SetMachineSpeed(void* pComDeviceAccss, DWORD baud_rate_target);
#ifndef _ANDROID
WLCTPCIACSS_API int WlctAccssJtagSendDR(void* pDeviceAccss, DWORD num_bits, BYTE *arr_in, BYTE *arr_out);
WLCTPCIACSS_API int WlctAccssJtagSendIR(void* pDeviceAccss, DWORD num_bits, BYTE *arr_in);
WLCTPCIACSS_API int JtagTmsShiftToReset(void* pDeviceAccss);
WLCTPCIACSS_API int JtagPutTmsTdiBits(void* pDeviceAccss, BYTE *tms, BYTE *tdi, BYTE* tdo, UINT length);
WLCTPCIACSS_API int JtagSetClockSpeed(void* pDeviceAccss, DWORD requestedFreq, DWORD* frqSet);
#endif
#ifdef _WINDOWS
WLCTPCIACSS_API int WlctAccssSendWmiCmd(void* pDeviceAccss, SEND_RECEIVE_WMI* wmi);
WLCTPCIACSS_API int WlctAccssRecieveWmiEvent(void* pDeviceAccss, SEND_RECEIVE_WMI* evt);
WLCTPCIACSS_API int WlctAccssMpSetDriverLogLevels(void* pDeviceAccss, ULONG logModulesEnableMsk, ULONG logModuleLevelsMsk[WILO_DRV_NUM_LOG_COMPS]);
#endif
WLCTPCIACSS_API int WlctAccssRegisterDriverMailboxEvents(void* pDeviceAccss, HANDLE* pMailboxEventHandle );
WLCTPCIACSS_API int WlctAccssUnregisterDriverMailboxEvents(void* pDeviceAccss);
WLCTPCIACSS_API int WlctAccssRegisterDriverEvent(void* pDeviceAccss, INT_PTR event_ptr, int eventId, int appId);
WLCTPCIACSS_API int WlctAccssRegisterDeviceForUnplug2Event(void* pDeviceAccss, INT_PTR event_ptr);

WLCTPCIACSS_API int WlctAccssAllocPmc(void* pDeviceAccss, DWORD size, DWORD numOfDescriptors);
WLCTPCIACSS_API int WlctAccssRegisterDriverDeviceEvents(void* pDeviceAccss, INT_PTR hDnEvent, INT_PTR hUpEvent, INT_PTR hUnplugEvent, INT_PTR hSysAssertEvent );


WLCTPCIACSS_API    int accssRead(void* pDeviceAccss, DWORD addr, DWORD & val);
WLCTPCIACSS_API    int accssWrite(void* pDeviceAccss, DWORD addr, DWORD val);
WLCTPCIACSS_API int accssClear32(void* pDeviceAccss, DWORD addr, DWORD val);
WLCTPCIACSS_API int accssSet32(void* pDeviceAccss, DWORD addr, DWORD val);

WLCTPCIACSS_API void hwCopyFromIO(void* dst, void* pDeviceAccss, DWORD src, UINT32 len);
